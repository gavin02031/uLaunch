#pragma once
#include <cstdio>
#include <cerrno>

namespace ul::glassnx {
    // Console-wide UI preference, not an authentication or access-control feature.
    inline constexpr const char *HideLockscreenPreviewsPath="sdmc:/glassnx/hide-lockscreen-previews";
    inline bool HideLockscreenPreviews(){
        auto fp=std::fopen(HideLockscreenPreviewsPath,"rb");
        if(!fp)return false;
        std::fclose(fp);
        return true;
    }
    inline bool SetHideLockscreenPreviews(bool hide){
        if(!hide)return std::remove(HideLockscreenPreviewsPath)==0||errno==ENOENT;
        auto fp=std::fopen(HideLockscreenPreviewsPath,"wb");
        return fp&&std::fclose(fp)==0;
    }
}
