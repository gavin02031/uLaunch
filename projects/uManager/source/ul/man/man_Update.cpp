#include <ul/man/man_Manager.hpp>
#include <ul/fs/fs_Stdio.hpp>
#include <ul/util/util_Zip.hpp>
#include <zlib.h>
#include <cerrno>
#include <cstdio>
#include <sys/stat.h>

namespace ul::man {
    namespace {
        constexpr const char *Work="sdmc:/glassnx/update-work/";
        constexpr const char *Package="sdmc:/glassnx/update.zip";
        constexpr const char *Active="sdmc:/atmosphere/contents/0100000000001000/exefs.nsp";
        constexpr const char *Recovery="sdmc:/switch/GlassNXRecovery.nro";
        constexpr const char *Files[]={
            "glassnx/bin/uSystem/exefs.nsp",
            "glassnx/bin/uMenu/main",
            "glassnx/bin/uMenu/main.npdm",
            "glassnx/bin/uMenu/romfs.bin",
            "glassnx/bin/uLoader/applet/main",
            "glassnx/bin/uLoader/applet/main.npdm",
            "glassnx/bin/uLoader/application/main",
            "glassnx/bin/uLoader/application/main.npdm",
            "switch/GlassNXManager.nro",
            "glassnx/BUILD.txt"
        };
        constexpr size_t Count=sizeof(Files)/sizeof(Files[0]);
        constexpr size_t ManagerIndex=8;
        constexpr unsigned All=(1u<<Count)-1;
        std::string W(const char *s){return std::string(Work)+s;}
        std::string W(const char *s,size_t i){return W(s)+std::to_string(i);}
        std::string Dest(size_t i){return std::string("sdmc:/")+Files[i];}
        bool Exists(const std::string &p){return fs::ExistsFile(p);}
        bool Remove(const std::string &p){return !Exists(p)||remove(p.c_str())==0;}
        bool Copy(const std::string &src,const std::string &dst){
            auto a=fopen(src.c_str(),"rb");if(!a)return false;
            auto b=fopen(dst.c_str(),"wb");if(!b){fclose(a);return false;}
            unsigned char buf[16384];bool ok=true;
            for(;;){
                size_t n=fread(buf,1,sizeof(buf),a);
                if(n && fwrite(buf,1,n,b)!=n){ok=false;break;}
                if(n<sizeof(buf)){if(ferror(a))ok=false;break;}
            }
            if(fclose(a)!=0)ok=false;
            if(fclose(b)!=0)ok=false;
            if(!ok)remove(dst.c_str());
            return ok;
        }
        bool Same(const std::string &a,const std::string &b){
            auto x=fopen(a.c_str(),"rb");if(!x)return false;
            auto y=fopen(b.c_str(),"rb");if(!y){fclose(x);return false;}
            unsigned char xx[8192],yy[8192];bool ok=true;
            for(;;){
                size_t n=fread(xx,1,sizeof(xx),x),m=fread(yy,1,sizeof(yy),y);
                if(n!=m||memcmp(xx,yy,n)!=0||ferror(x)||ferror(y)){ok=false;break;}
                if(n==0)break;
            }
            fclose(x);fclose(y);return ok;
        }
        bool Replace(const std::string &src,const std::string &dst){
            const auto tmp=dst+".gnx-new";
            if(!Remove(tmp)||!Copy(src,tmp)||!Same(src,tmp)){Remove(tmp);return false;}
            if(!Remove(dst)||rename(tmp.c_str(),dst.c_str())!=0){Remove(tmp);return false;}
            return Same(src,dst);
        }
        struct Writer {FILE *fp; uint64_t bytes=0,limit=0;uLong crc=0;};
        size_t Extract(void *arg,uint64_t offset,const void *data,size_t n){
            auto &w=*static_cast<Writer*>(arg);
            if(offset!=w.bytes||n>w.limit-w.bytes)return 0;
            if(fwrite(data,1,n,w.fp)!=n)return 0;
            w.crc=crc32(w.crc,static_cast<const Bytef*>(data),static_cast<uInt>(n));
            w.bytes+=n;return n;
        }
        bool Stage(std::string &message){
            auto z=zip_open(Package,0,'r');
            if(!z){message="Cannot open /glassnx/update.zip";return false;}
            unsigned seen=0;uint64_t total=0;bool ok=true;
            const auto n=zip_entries_total(z);
            if(n<=0||n>300)ok=false;
            for(ssize_t i=0;ok&&i<n;i++){
                if(zip_entry_openbyindex(z,i)!=0){ok=false;break;}
                const char *name=zip_entry_name(z);
                if(!name){ok=false;zip_entry_close(z);break;}
                for(size_t k=0;k<Count;k++)if(strcmp(name,Files[k])==0){
                    if(seen&(1u<<k)){ok=false;break;}
                    const auto size=zip_entry_size(z);
                    if(zip_entry_isdir(z)||size==0||size>64ULL*1024*1024||total+size>128ULL*1024*1024){ok=false;break;}
                    const auto path=W("new-",k);
                    auto fp=fopen(path.c_str(),"wb");if(!fp){ok=false;break;}
                    Writer w{fp,0,size,crc32(0L,Z_NULL,0)};
                    const int rc=zip_entry_extract(z,Extract,&w);
                    const bool closed=fclose(fp)==0;
                    if(rc!=0||!closed||w.bytes!=size||w.crc!=zip_entry_crc32(z)){ok=false;break;}
                    seen|=1u<<k;total+=size;break;
                }
                if(zip_entry_close(z)!=0)ok=false;
            }
            zip_close(z);
            if(!ok||seen!=All){message="Invalid or incomplete GlassNX ZIP. Use the full inner release ZIP, not the Actions outer archive.";return false;}
            auto fp=fopen(W("new-",Count-1).c_str(),"rb");
            char line[128]={};const bool valid=fp&&fgets(line,sizeof(line),fp)&&strncmp(line,"GlassNX v",9)==0;
            if(fp)fclose(fp);
            if(!valid){message="Missing GlassNX build identifier";return false;}
            return true;
        }
        bool WriteJournal(bool active,unsigned mask){
            auto fp=fopen(W("pending.tmp").c_str(),"wb");if(!fp)return false;
            const bool wrote=fprintf(fp,"GNX1 %u %u\n",active?1u:0u,mask)>0;
            const bool closed=fclose(fp)==0;
            return wrote&&closed&&rename(W("pending.tmp").c_str(),W("pending").c_str())==0;
        }
        bool ReadJournal(const std::string &path,bool &active,unsigned &mask){
            auto fp=fopen(path.c_str(),"rb");if(!fp)return false;
            unsigned a=0,m=0;const bool ok=fscanf(fp,"GNX1 %u %u",&a,&m)==2&&a<=1&&(m&~All)==0&&(m&((1u<<(ManagerIndex+1))-1))==((1u<<(ManagerIndex+1))-1);
            fclose(fp);if(!ok)return false;
            active=a!=0;mask=m;return true;
        }
        bool Restore(const std::string &journal,std::string &message){
            bool active=false;unsigned mask=0;
            if(!ReadJournal(journal,active,mask)){message="No valid recovery journal";return false;}
            for(size_t k=0;k<Count;k++)if((mask&(1u<<k))&&!Exists(W("old-",k))){message="Recovery backup incomplete";return false;}
            // Never remove an unrelated HOME override.
            if(Exists(Active)&&!Same(Active,W("old-",0))&&!Same(Active,W("new-",0))){message="Different HOME override present; recovery stopped";return false;}
            if(!Remove(Active)){message="Could not disable HOME override";return false;}
            for(size_t k=0;k<Count;k++){
                const bool ok=(mask&(1u<<k))?Replace(W("old-",k),Dest(k)):Remove(Dest(k));
                if(!ok){message="Recovery incomplete; HOME disabled. Retry with GlassNXRecovery.";return false;}
            }
            if(active&&!Replace(W("old-",0),Active)){message="Recovery files restored; HOME remains disabled. Retry recovery.";return false;}
            if(!Remove(journal)){message="Files restored but recovery journal could not be cleared";return false;}
            message="Previous GlassNX files restored. Restart console.";return true;
        }
    }
    bool GlassUpdatePending(){return Exists(W("pending"));}
    bool RestoreGlassUpdate(std::string &message){
        if(GlassUpdatePending())return Restore(W("pending"),message);
        if(!Exists(W("last"))){message="No previous update backup";return false;}
        if(rename(W("last").c_str(),W("pending").c_str())!=0){message="Cannot start recovery";return false;}
        return Restore(W("pending"),message);
    }
    bool InstallGlassUpdate(std::string &message){
        if(GlassUpdatePending()){message="Interrupted update detected. Restore previous version first.";return false;}
        if(!IsBasePresent()||!Exists(Dest(ManagerIndex))){message="Current GlassNX installation incomplete";return false;}
        const bool active=IsSystemActive();
        if(Exists(Active)&&!active){message="Different HOME override installed. Update stopped.";return false;}
        if(mkdir("sdmc:/glassnx/update-work",0777)!=0&&errno!=EEXIST){message="Cannot create update workspace";return false;}
        // Backups are replaced only before any live files are changed.
        if(!Remove(W("last"))){message="Cannot clear old recovery journal";return false;}
        if(!Stage(message))return false;
        unsigned mask=0;
        for(size_t k=0;k<Count;k++){
            if(Exists(Dest(k))){
                if(!Copy(Dest(k),W("old-",k))||!Same(Dest(k),W("old-",k))){message="Backup failed; installation unchanged";return false;}
                mask|=1u<<k;
            } else if(k!=Count-1){message="Required installed file missing";return false;}
        }
        // A separate executable remains available if the manager replacement is interrupted.
        if(!Replace(W("old-",ManagerIndex),Recovery)){message="Could not create recovery manager";return false;}
        if(!WriteJournal(active,mask)){message="Could not create recovery journal";return false;}
        if(active&&!Remove(Active)){message="Could not disable HOME; restore using manager";return false;}
        for(size_t k=0;k<Count;k++)if(!Replace(W("new-",k),Dest(k))){
            std::string recovery;
            const bool restored=Restore(W("pending"),recovery);
            message=restored?"Update failed; previous files restored":recovery;
            return false;
        }
        // Enable only after all new binaries were verified. Preserve disabled state.
        if(active&&!Replace(W("new-",0),Active)){
            std::string recovery;
            const bool restored=Restore(W("pending"),recovery);
            message=restored?"Activation failed; previous files restored":recovery;return false;
        }
        if(rename(W("pending").c_str(),W("last").c_str())!=0){message="Files installed; recovery journal remains. Restore or reboot carefully.";return false;}
        Remove(Package);
        message="GlassNX updated. Previous files backed up. Restart console.";return true;
    }
}
