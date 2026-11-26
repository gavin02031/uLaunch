#include <ul/menu/ui/ui_GlassShell.hpp>
#include <ul/menu/ui/ui_MenuApplication.hpp>
#include <ul/menu/ui/ui_Common.hpp>
#include <algorithm>
#include <cstdio>

extern ul::menu::ui::GlobalSettings g_GlobalSettings;
extern ul::menu::ui::MenuApplication::Ref g_MenuApplication;

namespace ul::menu::ui {
    namespace {
        constexpr const char *Themes[] = {"Pastel Dream","Midnight Glass","Frosted White","Aurora"};
        constexpr pu::ui::Color Tints[] = {{113,111,190,0},{18,28,62,148},{198,205,237,70},{36,123,122,100}};
        constexpr pu::ui::Color GlassColors[] = {{177,186,242,255},{85,108,175,255},{193,208,241,255},{105,206,197,255}};
        std::string EntryName(Entry &e) {
            if(e.Is<EntryType::Folder>()) return e.folder_info.name;
            if(e.Is<EntryType::Application>() || e.Is<EntryType::Homebrew>()) {
                if(e.control.name.empty()) e.TryLoadNacp();
                return e.control.name.empty() ? (e.Is<EntryType::Homebrew>() ? "Homebrew" : "Game") : e.control.name;
            }
            return "System shortcut";
        }
        std::string Battery() {return std::to_string(os::GetBatteryLevel())+"%";}
        std::string Clock() {
            const auto t=os::GetCurrentTime(); char b[16]={};
            snprintf(b,sizeof(b),"%02d:%02d",int(t.h),int(t.min)); return b;
        }
    }
    GlassShell::GlassShell(EntryMenu::Ref e,std::function<void(u32)> l,std::function<void()> old)
        :entries(e),launch(l),legacy(old) {}

    void GlassShell::SetPage(Page p) {
        if(p==page) return;
        if(p==Page::Control || p==Page::Notifications) previous=page;
        page=p; focus=0;offset=0;filter=0;
    }
    std::vector<u32> GlassShell::List() const {
        std::vector<u32> result;
        const auto &all=entries->GetEntries();
        for(u32 i=0;i<all.size();++i) {
            const auto &e=all[i];
            if(page==Page::Apps) {
                if(e.Is<EntryType::Homebrew>() || (filter==0 && e.IsSpecial())) result.push_back(i);
            } else if(page==Page::Library) {
                if((filter!=1 && e.Is<EntryType::Application>()) || (filter!=2 && e.Is<EntryType::Folder>())) result.push_back(i);
            } else if(e.Is<EntryType::Application>() || e.Is<EntryType::Homebrew>()) result.push_back(i);
        }
        if(page==Page::Home) {
            const auto it=std::find_if(result.begin(),result.end(),[&](u32 i){return g_GlobalSettings.IsEntrySuspended(all[i]);});
            if(it!=result.end()) std::rotate(result.begin(),it,it+1);
        }
        return result;
    }
    void GlassShell::Picture(Renderer &r,const pu::sdl2::TextureHandle::Ref &t,int x,int y,int w,int h) {
        if(t && t->Get()) r->RenderTexture(t->Get(),x,y,pu::ui::render::TextureRenderOptions({},w,h,{},{},{}));
    }
    void GlassShell::Card(Renderer &r,int x,int y,int w,int h,bool selected) {
        const int radius=std::min(34,h/3);
        const auto c=GlassColors[theme];
        r->RenderRoundedRectangleFill({23,26,65,65},x+3,y+7,w,h,radius);
        r->RenderRoundedRectangleFill({c.r,c.g,c.b,static_cast<u8>(glass*2)},x,y,w,h,radius);
        r->RenderRoundedRectangle({233,239,255,selected?u8(255):u8(135)},x,y,w,h,radius);
        r->RenderRoundedRectangle({255,255,255,selected?u8(160):u8(55)},x+3,y+3,w-6,h-6,radius-3);
        if(selected) {
            r->RenderRoundedRectangle({185,228,255,155},x-5,y-5,w+10,h+10,radius+5);
            r->RenderRoundedRectangle({238,248,255,255},x-2,y-2,w+4,h+4,radius+2);
        }
    }
    void GlassShell::Label(Renderer &r,const std::string &s,int x,int y,int size,int maxw,bool center) {
        if(s.empty())return;
        const auto key=std::to_string(size)+"/"+std::to_string(maxw)+"/"+s;
        auto it=texts.find(key);
        if(it==texts.end()) {
            if(texts.size()>=160) texts.clear();
            auto t=pu::ui::elm::TextBlock::New(0,0,s);
            t->SetFont(pu::ui::MakeDefaultFontName(size));
            t->SetColor({255,255,255,255});
            if(maxw>0)t->SetClampWidth(maxw);
            it=texts.emplace(key,t).first;
        }
        const auto &t=it->second;
        t->OnRender(r,center?x-t->GetWidth()/2:x,y);
    }
    void GlassShell::Button(Renderer &r,int x,int y,int w,int h,const std::string &s,bool selected,std::function<void()> cb) {
        Card(r,x,y,w,h,selected); Label(r,s,x+w/2,y+(h-37)/2,30,w-24,true);
        hits.push_back({x,y,w,h,cb});
    }
    void GlassShell::Tile(Renderer &r,u32 index,int x,int y,int w,int h,bool selected) {
        Card(r,x,y,w,h,selected);
        auto &e=entries->GetEntries().at(index);
        auto icon=entries->GlassIcon(index,icon_budget);
        if(!icon && e.Is<EntryType::Application>()) icon=GetDefaultApplicationIconTexture();
        int side=std::min(w-36,h-58);
        Picture(r,icon,x+(w-side)/2,y+12,side,side);
        const auto name=EntryName(e);
        Label(r,name,x+w/2,y+h-40,27,w-24,true);
        hits.push_back({x,y,w,h,[this,index](){launch(index);}});
    }
    void GlassShell::ToggleRadio(bool wifi) {
        bool &value=wifi?g_GlobalSettings.wireless_lan_enabled:g_GlobalSettings.bluetooth_enabled;
        const bool next=!value;
        const auto rc=wifi?setsysSetWirelessLanEnableFlag(next):setsysSetBluetoothEnableFlag(next);
        if(R_SUCCEEDED(rc)) {value=next;g_MenuApplication->ShowNotification(std::string(wifi?"Wi-Fi ":"Bluetooth ")+(next?"enabled":"disabled"));}
        else g_MenuApplication->ShowNotification("Unable to change system setting");
    }
    std::vector<GlassShell::Choice> GlassShell::Choices() {
        std::vector<Choice> c;
        if(page==Page::Settings) {
            c={{466,210,1360,105,"Theme",Themes[theme],[this](){theme=(theme+1)%4;}},
               {466,335,1360,105,"Glass transparency",std::to_string(glass)+"%",[this](){glass=glass>=85?35:glass+10;}},
               {466,460,1360,105,"Lock screen","Open lock screen",[](){g_MenuApplication->LoadMenu(MenuType::Lockscreen);}},
               {466,585,1360,105,"System settings","Network, controllers, console",[](){ShowSettingsMenu();}},
               {466,710,1360,105,"Installed themes","Open theme manager",[](){ShowThemesMenu();}},
               {466,835,1360,105,"Recovery / legacy menu","Return to uLaunch interface",legacy}};
        } else if(page==Page::Control) {
            c={{70,435,420,170,"Wi-Fi",g_GlobalSettings.wireless_lan_enabled?"On":"Off",[this](){ToggleRadio(true);}},
               {510,435,420,170,"Bluetooth",g_GlobalSettings.bluetooth_enabled?"On":"Off",[this](){ToggleRadio(false);}},
               {950,435,420,170,"Controllers","Connect or configure",[](){ShowController();}},
               {1390,435,460,170,"Power","Sleep / reboot / shutdown",[](){ShowPowerDialog();}},
               {70,640,560,200,"Notifications","Recent alerts",[this](){SetPage(Page::Notifications);}},
               {650,640,560,200,"Display & audio","Open system settings",[](){ShowSettingsMenu();}},
               {1230,640,620,200,"Lock screen","Lock now",[](){g_MenuApplication->LoadMenu(MenuType::Lockscreen);}}};
        } else if(page==Page::Notifications) {
            c={{430,905,510,90,"View / clear history","",[](){g_MenuApplication->ShowNotificationHistory();}},
               {980,905,510,90,"Back","",[this](){SetPage(previous);}}};
        }
        return c;
    }
    void GlassShell::Move(int amount) {
        int count=0;
        if(page==Page::Home || page==Page::Library || page==Page::Apps)count=static_cast<int>(List().size());
        else count=static_cast<int>(Choices().size());
        if(count==0){focus=0;offset=0;return;}
        focus=std::clamp(focus+amount,0,count-1);
        const int cap=page==Page::Home?5:10;
        if(focus<offset)offset=(focus/cap)*cap;
        if(focus>=offset+cap)offset=(focus/cap)*cap;
    }
    void GlassShell::Activate() {
        if(page==Page::Home || page==Page::Library || page==Page::Apps) {
            auto list=List();
            if(focus>=0 && focus<static_cast<int>(list.size()))launch(list[focus]);
        } else {
            auto choices=Choices();
            if(focus>=0 && focus<static_cast<int>(choices.size()))choices[focus].action();
        }
    }
    void GlassShell::HandleInput(u64 down,pu::ui::TouchPoint touch) {
        if(touch.IsEmpty())touched=false;
        else if(!touched) {
            touched=true;
            for(const auto &hit:hits)if(touch.HitsRegion(hit.x,hit.y,hit.w,hit.h)) {auto action=hit.action;action();return;}
        }
        if(down&HidNpadButton_Minus){legacy();return;}
        if(down&(HidNpadButton_ZL|HidNpadButton_ZR)){SetPage(page==Page::Control?Page::Home:Page::Control);return;}
        if(down&HidNpadButton_Plus){SetPage(page==Page::Notifications?Page::Home:Page::Notifications);return;}
        if(down&HidNpadButton_B){SetPage(Page::Home);return;}
        if(down&HidNpadButton_L){int p=static_cast<int>(page);SetPage(static_cast<Page>(p>=4?0:(p+3)%4));return;}
        if(down&HidNpadButton_R){int p=static_cast<int>(page);SetPage(static_cast<Page>(p>=4?0:(p+1)%4));return;}
        if(down&HidNpadButton_Y){
            if(page==Page::Library || page==Page::Apps){filter=(filter+1)%(page==Page::Library?3:2);focus=0;offset=0;}return;
        }
        if(down&(HidNpadButton_Left|HidNpadButton_StickLLeft))Move(-1);
        else if(down&(HidNpadButton_Right|HidNpadButton_StickLRight))Move(1);
        else if(down&(HidNpadButton_Up|HidNpadButton_StickLUp))Move(page==Page::Library||page==Page::Apps?-5:-1);
        else if(down&(HidNpadButton_Down|HidNpadButton_StickLDown))Move(page==Page::Library||page==Page::Apps?5:1);
        else if(down&HidNpadButton_A)Activate();
    }
    void GlassShell::OnRender(Renderer &r,const s32,const s32) {
        hits.clear();icon_budget=true;
        Picture(r,GetBackgroundTexture(),0,0,1920,1080);
        if(Tints[theme].a)r->RenderRectangleFill(Tints[theme],0,0,1920,1080);
        Picture(r,GetSelectedUserIconTexture(),44,28,68,68);
        Label(r,"GlassNX",128,34,45);
        Card(r,555,25,810,78);
        const char *tabs[]={"Home","Library","Apps","Settings"};
        for(int i=0;i<4;++i)Button(r,565+i*198,32,190,64,tabs[i],static_cast<int>(page)==i,[this,i](){SetPage(static_cast<Page>(i));});
        Label(r,Clock(),1480,43,30);
        Label(r,g_GlobalSettings.wireless_lan_enabled?"Wi-Fi":"Offline",1590,43,27);
        Label(r,Battery(),1755,43,30);

        if(page==Page::Home) {
            auto list=List();
            focus=std::clamp(focus,0,std::max(0,static_cast<int>(list.size())-1));offset=(focus/5)*5;
            Card(r,70,140,1160,435);
            Card(r,1260,140,590,205);Card(r,1260,365,590,210);
            Label(r,"SYSTEM OVERVIEW",1300,166,30);
            Label(r,Battery(),1300,214,64);Label(r,os::IsConsoleCharging()?"Charging":"Battery",1510,241,30);
            Label(r,"YOUR LIBRARY",1300,391,30);Label(r,std::to_string(list.size())+" entries",1300,446,45);
            Label(r,"In this folder",1300,506,27);
            if(!list.empty()) {
                const auto idx=list[focus];auto &e=entries->GetEntries()[idx];
                const bool suspended=g_GlobalSettings.IsEntrySuspended(e);
                Label(r,suspended?"CONTINUE PLAYING":"SELECTED GAME / APP",112,185,30);
                Label(r,EntryName(e),112,260,64,680);
                Label(r,suspended?"Currently suspended":"Ready to open",112,362,30,680);
                Button(r,112,445,250,78,suspended?"Resume":"Play",false,[this,idx](){launch(idx);});
                auto icon=entries->GlassIcon(idx,icon_budget);if(!icon&&e.Is<EntryType::Application>())icon=GetDefaultApplicationIconTexture();
                Picture(r,icon,845,175,360,360);
            } else {
                Label(r,"Your home awaits",112,245,64);
                Label(r,"Add or organize entries in the legacy menu.",112,355,30);
                Button(r,112,445,360,78,"Manage entries",false,legacy);
            }
            Label(r,"Your games & apps",75,600,37);
            for(int i=0;i<5&&offset+i<static_cast<int>(list.size());++i)Tile(r,list[offset+i],70+i*360,665,340,250,focus==offset+i);
            Card(r,392,954,1136,87);
            Button(r,404,960,215,74,"Notifications",false,[this](){SetPage(Page::Notifications);});
            Button(r,628,960,210,74,"Control",false,[this](){SetPage(Page::Control);});
            Button(r,847,960,210,74,"Album",false,[](){ShowAlbum();});
            Button(r,1066,960,210,74,"Settings",false,[this](){SetPage(Page::Settings);});
            Button(r,1285,960,228,74,"Power",false,[](){ShowPowerDialog();});
        } else if(page==Page::Library || page==Page::Apps) {
            const bool games=page==Page::Library;
            auto list=List();focus=std::clamp(focus,0,std::max(0,static_cast<int>(list.size())-1));offset=(focus/10)*10;
            Label(r,games?"Game Library":"App Library",70,140,64);
            Label(r,games?"Your games, together.":"Homebrew and system utilities.",70,225,30);
            Card(r,70,305,320,630);
            Label(r,games?"Collections":"Categories",94,330,37);
            Button(r,92,405,275,80,"All",filter==0,[this](){filter=0;focus=offset=0;});
            Button(r,92,505,275,80,games?"Folders":"Homebrew",filter==1,[this](){filter=1;focus=offset=0;});
            if(games)Button(r,92,605,275,80,"Games",filter==2,[this](){filter=2;focus=offset=0;});
            Button(r,92,815,275,80,"Manage entries",false,legacy);
            Label(r,std::to_string(list.size())+" entries in this folder",430,287,30);
            if(list.empty())Label(r,"Nothing here yet. Use Manage entries.",430,445,37);
            for(int i=0;i<10&&offset+i<static_cast<int>(list.size());++i)Tile(r,list[offset+i],430+(i%5)*286,350+(i/5)*288,265,268,focus==offset+i);
            Label(r,"L/R  Pages     Y  Filter     A  Open     -  Legacy menu",70,1005,30);
            if(list.size()>10)Label(r,std::to_string(offset/10+1)+"/"+std::to_string((list.size()+9)/10),1720,1005,30);
        } else if(page==Page::Settings || page==Page::Control || page==Page::Notifications) {
            const bool settings=page==Page::Settings,control=page==Page::Control;
            Label(r,settings?"Settings":control?"Control Center":"Notifications",70,137,64);
            if(settings) {
                Card(r,70,210,360,730);
                Label(r,"Appearance",95,242,37);
                for(int i=0;i<4;++i)Button(r,94,315+i*105,308,80,Themes[i],theme==i,[this,i](){theme=i;});
                Label(r,"Session settings",95,815,27);
                Label(r,"Saved settings: legacy",95,855,27);
            } else if(control) {
                Card(r,70,240,560,165);Label(r,"BATTERY",103,260,30);Label(r,Battery(),103,310,64);
                Card(r,650,240,560,165);Label(r,"CONNECTION",683,260,30);Label(r,g_GlobalSettings.wireless_lan_enabled?"Wi-Fi enabled":"Offline",683,319,37);
                Card(r,1230,240,620,165);Label(r,"POWER PROFILE",1263,260,30);Label(r,"System default",1263,319,37);
                Label(r,"Brightness / volume / clock controls: not available in this build.",70,895,30);
            } else {
                const auto &history=g_MenuApplication->GetNotificationHistory();
                if(history.empty()) {Card(r,430,270,1060,170);Label(r,"You're all caught up.",960,325,45,0,true);}
                for(size_t i=0;i<std::min<size_t>(history.size(),4);++i) {
                    int y=245+static_cast<int>(i)*155;Card(r,430,y,1060,130);
                    Label(r,"GlassNX",465,y+12,27);Label(r,history[history.size()-1-i],465,y+53,37,970);
                }
            }
            auto choices=Choices();focus=std::clamp(focus,0,std::max(0,static_cast<int>(choices.size())-1));
            for(size_t i=0;i<choices.size();++i) {
                auto &c=choices[i];Card(r,c.x,c.y,c.w,c.h,focus==static_cast<int>(i));
                Label(r,c.label,c.x+25,c.y+15,37,c.w-50);
                Label(r,c.detail,c.x+25,c.y+65,27,c.w-50);
                hits.push_back({c.x,c.y,c.w,c.h,c.action});
            }
            Label(r,"A  Select     B  Home     ZL/ZR  Control     +  Notifications",70,1005,30);
        }
    }
}
