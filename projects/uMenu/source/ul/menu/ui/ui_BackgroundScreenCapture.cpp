#include <ul/menu/ui/ui_BackgroundScreenCapture.hpp>
#include <ul/menu/ui/ui_MenuApplication.hpp>

extern ul::menu::ui::GlobalSettings g_GlobalSettings;
extern ul::menu::ui::MenuApplication::Ref g_MenuApplication;

namespace ul::menu::ui {

    namespace {

        constexpr size_t CaptureBufferWidth = pu::ui::render::BaseScreenWidth;
        constexpr size_t CaptureBufferHeight = pu::ui::render::BaseScreenHeight;
        constexpr size_t CaptureBufferBytesPerPixel = 4;

        constexpr size_t CaptureBufferSize = CaptureBufferWidth * CaptureBufferHeight * CaptureBufferBytesPerPixel;
        static_assert(CaptureBufferSize == 0x384000);

        RawRgbaImage::Ref g_ScreenCaptureBackground;

        constexpr u8 ScreenCaptureBackgroundAlphaIncrement = 14;

        enum class SuspendedImageMode {
            ShowingAfterStart = 0,
            Focused = 1,
            HidingForResume = 2,
            NotFocused = 3,
            ShowingGainedFocus = 4,
            HidingLostFocus = 5,
            ResumeRequested = 6
        };

        u8 g_ScreenCaptureBackgroundMinimumAlpha = 0xFF;
        SuspendedImageMode g_ScreenCaptureBackgroundMode = SuspendedImageMode::ShowingAfterStart;
        s32 g_ScreenCaptureBackgroundAlpha = 0xFF;

    }
    
    void InitializeScreenCaptures(const smi::MenuStartMode start_mode) {
        g_ScreenCaptureBackground = RawRgbaImage::New(0, 0);
        g_ScreenCaptureBackgroundAlpha=0xFF;
        g_ScreenCaptureBackgroundMode=SuspendedImageMode::ShowingAfterStart;
        // GlassNX never draws the old capture in its HOME. Avoid allocating
        // or clearing a game framebuffer entirely. Legacy recovery retains
        // functional navigation/resume, but uses the wallpaper, not a capture.
        (void)start_mode;
        g_ScreenCaptureBackgroundMode=SuspendedImageMode::NotFocused;
        g_ScreenCaptureBackgroundAlpha=0;
        g_ScreenCaptureBackground->SetAlpha(0);

        g_ScreenCaptureBackgroundMinimumAlpha = (u8)GetRequiredUiValue<u32>("suspended_app_final_alpha");
    }

    RawRgbaImage::Ref GetScreenCaptureBackground() {
        return g_ScreenCaptureBackground;
    }

    bool HasScreenCaptureBackground() {
        return g_ScreenCaptureBackground->HasImage();
    }

    void RequestResumeScreenCaptureBackground() {
        if(!g_GlobalSettings.IsSuspended() || IsScreenCaptureResumePending())return;
        g_ScreenCaptureBackgroundMode = SuspendedImageMode::HidingForResume;
        if(!HasScreenCaptureBackground() || !g_ScreenCaptureBackground->IsVisible())g_ScreenCaptureBackgroundAlpha=0xFF;
    }

    void RequestHideScreenCaptureBackground() {
        g_ScreenCaptureBackgroundMode = SuspendedImageMode::NotFocused;
        g_ScreenCaptureBackgroundAlpha=0;
        g_ScreenCaptureBackground->SetAlpha(0);
    }

    void ClearScreenCaptureBackground() {
        RequestHideScreenCaptureBackground();
        g_ScreenCaptureBackground->ClearImage();
    }

    void RequestHideLoseFocusScreenCaptureBackground() {
        if(IsScreenCaptureResumePending() || !HasScreenCaptureBackground())return;
        g_ScreenCaptureBackgroundMode = SuspendedImageMode::HidingLostFocus;
    }

    void RequestShowGainFocusScreenCaptureBackground() {
        if(IsScreenCaptureResumePending() || !g_GlobalSettings.IsSuspended() || !HasScreenCaptureBackground())return;
        g_ScreenCaptureBackgroundMode = SuspendedImageMode::ShowingGainedFocus;
    }

    bool IsScreenCaptureBackgroundNotTransitioning() {
        return (g_ScreenCaptureBackgroundMode == SuspendedImageMode::NotFocused) || (g_ScreenCaptureBackgroundMode == SuspendedImageMode::Focused);
    }

    bool IsScreenCaptureBackgroundFocused() {
        return g_ScreenCaptureBackgroundMode == SuspendedImageMode::Focused;
    }

    bool IsScreenCaptureResumePending() {
        return g_ScreenCaptureBackgroundMode==SuspendedImageMode::ResumeRequested || g_ScreenCaptureBackgroundMode==SuspendedImageMode::HidingForResume;
    }

    void UpdateScreenCaptureBackground(const bool after_start_stay_at_min_alpha) {
        if(!g_GlobalSettings.IsSuspended() && g_ScreenCaptureBackgroundMode!=SuspendedImageMode::ShowingAfterStart) {
            if(HasScreenCaptureBackground())ClearScreenCaptureBackground();
            return;
        }
        if(!HasScreenCaptureBackground() && !IsScreenCaptureResumePending()) {RequestHideScreenCaptureBackground();return;}
        switch(g_ScreenCaptureBackgroundMode) {
            case SuspendedImageMode::ShowingAfterStart: {
                if(after_start_stay_at_min_alpha && (g_ScreenCaptureBackgroundAlpha <= g_ScreenCaptureBackgroundMinimumAlpha)) {
                    g_ScreenCaptureBackgroundAlpha = g_ScreenCaptureBackgroundMinimumAlpha;
                    g_ScreenCaptureBackground->SetAlpha(g_ScreenCaptureBackgroundAlpha);
                    g_ScreenCaptureBackgroundMode = SuspendedImageMode::Focused;
                }
                else if(!after_start_stay_at_min_alpha && (g_ScreenCaptureBackgroundAlpha == 0)) {
                    g_ScreenCaptureBackground->SetAlpha(g_ScreenCaptureBackgroundAlpha);
                    g_ScreenCaptureBackgroundMode = SuspendedImageMode::NotFocused;
                }
                else {
                    g_ScreenCaptureBackground->SetAlpha(g_ScreenCaptureBackgroundAlpha);
                    g_ScreenCaptureBackgroundAlpha -= ScreenCaptureBackgroundAlphaIncrement;
                    if(g_ScreenCaptureBackgroundAlpha < 0) {
                        g_ScreenCaptureBackgroundAlpha = 0;
                    }
                }
                break;
            }
            case SuspendedImageMode::Focused: {
                break;
            }
            case SuspendedImageMode::HidingForResume: {
                if(g_ScreenCaptureBackgroundAlpha == 0xFF) {
                    g_ScreenCaptureBackground->SetAlpha(g_ScreenCaptureBackgroundAlpha);
                    g_ScreenCaptureBackgroundMode=SuspendedImageMode::ResumeRequested;
                    const auto rc=smi::ResumeApplication();
                    if(R_FAILED(rc)) {
                        UL_LOG_WARN("GlassNX resume failed: 0x%x",rc);
                        RequestHideScreenCaptureBackground();
                        g_MenuApplication->ShowNotification("Could not resume game");
                    }
                }
                else {
                    g_ScreenCaptureBackground->SetAlpha(g_ScreenCaptureBackgroundAlpha);
                    g_ScreenCaptureBackgroundAlpha += ScreenCaptureBackgroundAlphaIncrement;
                    if(g_ScreenCaptureBackgroundAlpha > 0xFF) {
                        g_ScreenCaptureBackgroundAlpha = 0xFF;
                    }
                }
                break;
            }
            case SuspendedImageMode::ResumeRequested:
            case SuspendedImageMode::NotFocused: {
                break;
            }
            case SuspendedImageMode::ShowingGainedFocus: {
                if(g_ScreenCaptureBackgroundAlpha == g_ScreenCaptureBackgroundMinimumAlpha) {
                    g_ScreenCaptureBackground->SetAlpha(g_ScreenCaptureBackgroundAlpha);
                    g_ScreenCaptureBackgroundMode = SuspendedImageMode::Focused;
                }
                else {
                    g_ScreenCaptureBackground->SetAlpha(g_ScreenCaptureBackgroundAlpha);
                    g_ScreenCaptureBackgroundAlpha += ScreenCaptureBackgroundAlphaIncrement;
                    if(g_ScreenCaptureBackgroundAlpha > g_ScreenCaptureBackgroundMinimumAlpha) {
                        g_ScreenCaptureBackgroundAlpha = g_ScreenCaptureBackgroundMinimumAlpha;
                    }
                }
                break;
            }
            case SuspendedImageMode::HidingLostFocus: {
                if(g_ScreenCaptureBackgroundAlpha == 0) {
                    g_ScreenCaptureBackground->SetAlpha(g_ScreenCaptureBackgroundAlpha);
                    g_ScreenCaptureBackgroundMode = SuspendedImageMode::NotFocused;
                }
                else {
                    g_ScreenCaptureBackground->SetAlpha(g_ScreenCaptureBackgroundAlpha);
                    g_ScreenCaptureBackgroundAlpha -= ScreenCaptureBackgroundAlphaIncrement;
                    if(g_ScreenCaptureBackgroundAlpha < 0) {
                        g_ScreenCaptureBackgroundAlpha = 0;
                    }
                }
                break;
            }
        }
    }

}
