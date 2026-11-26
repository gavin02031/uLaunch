# GlassNX v0.1 — uLaunch integration preview

This branch retains uLaunch 1.2.6's native uSystem, uLoader, uMenu and uManager, with an initial GlassNX presentation layer. Upstream license and credits remain unchanged.

- Pastel Dream wallpaper, translucent panels and luminous selection ring generated offline into the bundled default theme at build time. Texture dimensions are inherited from upstream; custom installed themes may override the default.
- Restyled native lock screen, dialogs, quick menu and notification toasts.
- Notification history (24 entries, newest first, session-only) accessed from the ZL/ZR quick menu. Existing uLaunch notification sources are retained. Low battery notice at 15%, rearmed at 25% or charging. No fake events.
- Existing game and homebrew launching, suspend/resume, folders and system settings remain upstream.

## Build
Use the existing GitHub Actions workflow for this branch. The expected artifact is SdOut.zip. Local build follows the upstream devkitPro instructions; `make umenu` now invokes the standard-library Python asset generator before packing romfs.

## Testing / recovery
This is an unverified integration preview, not the final six-screen design or a verified NRO. Test only after CI succeeds. Back up the SD card and existing HOME replacement. Prefer offline emuMMC for initial validation; online use of any HOME replacement carries ban risk. If boot or menu fails, restore the original HOME override using uManager or remove only the replacement `atmosphere/contents/0100000000001000/exefs.nsp` after powering off, preserving other files and themes. Restore any prior backup if applicable. Do not install on an unmodified console.

## Next milestones
Dedicated six-page navigation and actual game artwork dashboard; reusable dynamic GPU glass material and transition animations; durable notification service and homebrew API; real brightness/volume and optional compatible performance controls. No clocks are changed by this preview. Target 720p/60fps requires device measurements. Original mockups remain the acceptance target, not the current visual result.

## v0.2 — native shell layout pass
The default main menu now renders a separate controller/touch-driven GlassShell over uLaunch's native entry data. L/R switches Home, Library, Apps and Settings; ZL/ZR opens Control Center, + opens Notifications, A selects, B returns Home, Y filters, and Minus switches to the legacy grid. In the legacy grid hold Plus and press Minus to restore GlassShell. Existing launch handlers, title suspension and system services are reused. Library and app lists are scoped to the active uLaunch folder; manage folders/entries in legacy. Theme and glass controls are session-only. Lock screen uses a large clock and latest real session alert. No fake statistics, no clock changes. Brightness, volume, temperature, favorites, search, persistent preferences, banner art and GPU refraction remain unfinished. Native compile and hardware verification are required.

### v0.2 rendering and hardware follow-up
Cached half-resolution textured glass bevels and reflective edges replace flat card fills. The GPU composites the cached surfaces, not live backdrop refraction. Home and Control Center query actual battery, SD free space and SoC temperature; brightness and master volume use libnx with service-result checks and display Unavailable if unsupported. Controller left/right adjusts a focused slider, up/down changes selection. Touch adjusts slider position. Brightness is clamped to at least 10%. No CPU/GPU/RAM clocks are changed. Service behavior and visual performance still need hardware confirmation.

## v0.3 — visual fidelity and personalization
- Wallpaper-aware cached glass with low-resolution blur and edge displacement; no live game/backbuffer sampling. Optional custom wide title artwork at `/ulaunch/glassnx/art/<16-digit-title-id>.jpg` (8 MiB file limit); real installed icon fallback. Use your own art; none downloaded automatically.
- Search via X and Switch keyboard, favorites via left-stick click, A-Z sort via touch, appearance previews, theme selection via X on Settings, saved transparency/blur/motion. Favorites and preferences are separate for sysMMC/emuMMC, shared across users in each environment.
- Persistent 24-item notification history, top-right toasts, illustrated twilight lock wallpaper and custom `/ulaunch/glassnx/lockscreen.jpg` override. Page transition is a time-based fade, not spring physics.
- All six pages are still a development implementation; no device screenshot or successful Switch compile yet. Full live GPU refraction, exact hero artwork, advanced categories, fan/overclock settings and 60fps measurement remain outstanding.

## v0.4 — original concept alignment
- Six-column/two-row library shelves, actual recent title launch records, dedicated All/Favorites/Collections/Recently Played and Apps/Homebrew/Tools/Media tabs. Recent list records successful upstream launch requests, not measured playtime.
- Native vector-like line icons, floating icon navigation, grouped quick toggles, vertical brightness/volume, session-only quiet notifications (low battery still shown), explicit unavailable performance/fan readouts. No fake or unsafe overclock profiles.
- Settings sidebar matches the original category structure; system/network/storage links open the upstream settings page, not dedicated new subpages. Lock screen shows two actual history alerts and uses generated glass card assets.
- CI dependency repair replaces system pip install with distro python3-requests. Full hardware verification and true dynamic refraction remain pending.

## v0.5 — independent boot-time installation
GlassNX now uses its own /glassnx SD directory, and the system backend opens /glassnx/bin/uMenu and /glassnx/bin/uLoader, rather than stock /ulaunch paths. The existing uLaunch installation/config is not needed. The installer is GlassNXManager.nro with a disabled upstream updater. CI generates GlassNX-v0.5.zip with atmosphere/, glassnx/, switch/ at archive root. Back up any previous HOME exefs.nsp before copying the archive; only one HOME override can be active. A complete archive already includes the boot override. Reboot into Atmosphere to start GlassNX. Manager can disable only a matching GlassNX override, leaving theme romfs intact. The manager is optional for normal boot. Startup logs include GlassNX version and shell visibility. Source-based fork retains upstream license/credits. Hardware test pending.

## v0.6 — hardware feedback polish
The first hardware screenshots showed excessive white rims, square artwork, utility forwarders on Home, a legacy System page and unavailable temperature. This pass uses subtle directionally lit glass, unified tab/dock segments, rounded cropped artwork via SDL geometry (rectangular fallback), a darker sculpted wallpaper, game-first recent ranking and known utility filtering. Unknown installed forwarders remain classified as titles rather than silently hidden. Settings includes separate native category pages with working links to advanced options, not a complete rewrite of every upstream applet. Storage uses statvfs then direct SD filesystem fallback, reporting actual bytes to decimal GiB; temperature uses modern TS sessions before the older API. Service results may remain unavailable on hardware. No fan or overclock changes. Install the full v0.6 package with the console powered off and preserve the prior known working exefs.nsp for recovery. Existing /glassnx config stays intact.

## v0.7 — interaction polish
Time-based critically damped springs animate the navigation selection and artwork focus; reduced motion disables both. Touch now selects an artwork tile first and opens on a second tap, and Control Center supports continuous drag with throttled service calls. B returns to the previous main tab from Control Center/Notifications, navigates out of a folder, or returns from a settings subcategory. X on HOME offers to close the suspended game, reusing upstream confirmation. Notification Center scrolls saved alerts and clears with confirmation. No new background services, clock manipulation or unverified performance claims. Native build and hardware QA remain required.

Temporary overlays restore the previous main tab's focus, filter, search and settings category on dismissal.

## v0.8 — artwork and library ownership
Right-stick click opens installed title options, with Game/App/Automatic classification stored per environment in a bounded categories file. Actual custom wide art remains supported; without it, HOME uses the installed icon as an artwork-derived cropped background plus a readable gradient and rounded foreground icon (not a real banner). Profile and status-area touch shortcuts, a filled battery glyph and a saved-alert count badge improve everyday access. Badge is total saved alerts, not an unread count. All changes require compilation and hardware validation. No network art downloads or live GPU backdrop effects added.

## v0.9 — direct hardware feedback
HOME now prominently displays the running version and a distinct rose-tinted X Close Game control next to Resume for suspended titles. Closing continues through the existing confirmation and backend termination path. Game/app art uses more pronounced rounded corners; when SDL textured geometry is unsupported, a bounded rounded-band crop preserves rounded corners rather than reverting to square artwork. Long labels are measured, fitted to available font sizes, then UTF-8-safe ellipsized instead of marquee cropping. Build archives include /glassnx/BUILD.txt with the exact commit. No Codex branch or PR was visible at integration time, so this builds on the verified v0.8 branch. Compile/hardware validation pending.

## v0.9 — single visual polish update

This update targets `glassnx` from `402ce8a53cb7da790841758c925ef33294907db1`, on the temporary `glassnx-v0.9-polish` branch. It does not change the roadmap or backend ownership.

- Shared material: accelerated SDL render-target blur (two separable nine-tap passes at 240×135), rounded GPU lens geometry with displaced wallpaper UVs, directional specular lighting, softer layered shadows and cyan-white focus rims. The blurred texture is reused until wallpaper, tint or blur changes. Geometry topology and arc trigonometry are cached. The existing cached CPU wallpaper material remains the fallback for unsupported GPU targets; no framebuffer readback or background services are added.
- Rendering: 1280×720 framebuffer with the established 1920×1080 logical drawing and touch coordinates. This reduces framebuffer pixels to 44.4% of the previous size, including docked mode. Existing vsync, time-based navigation/focus springs and reduced-motion preference remain; page fades now use a 260 ms smoothstep curve. CPU wallpaper sampling is skipped when the GPU material is ready, and equal-size GPU bevel textures are shared across panel positions.
- Home: pastel wallpaper, shared glass/depth treatment, clearer text hierarchy and layered artwork focus. Real installed artwork and user-supplied wide banners remain the source of title art.
- Library: five-column, two-row shelf and glass filter sidebar; controller row movement, page capacity and touch regions follow the new layout. Search, favorites, folders, recents and sorting remain available.
- Apps and Settings: shared pastel material, directional reflections, focus and typography treatment. Existing app filters, settings categories and links are preserved.
- Control Center: horizontal brightness and volume sliders with matching drag regions, readable percentage labels and reorganized metrics/actions. All existing service calls and result handling remain. Performance controls still report stock clocks/unavailable profiles; no overclocking is introduced.
- Lock screen: pastel curved-sheet wallpaper, 220 px logical clock, rebalanced date, two saved-alert cards and larger unlock pill. A, touch and HOME unlock behavior remain. Custom lock wallpaper overrides remain supported. Notification Center and toasts retain their existing real notification sources, persistence and quiet mode.
- Package names and displayed GlassNX version remain v0.9; upstream uLaunch version macros, licenses, installation paths and recovery mechanisms are preserved.

### Validation and remaining limits

The production material has a host SDL/Mesa smoke test (`tests/liquid_glass.cpp`) for state restoration, render-target preservation, rounded coverage, blur changes and measured reduction of high-frequency contrast. CI runs this independently of the native build. Generated PNG assets were opened for inspection. Native build results and exact toolchain revisions are recorded in the PR.

60 FPS on a stock Switch Lite is a target, **not a measured result**. No console was available here. Hardware acceptance still needs frame-time measurements (16.67 ms budget, including cold-page navigation), touch/controller checks on all screens, suspend/resume, applets, low-battery notifications and recovery. Cached CPU bevel creation, font rasterization, artwork decode and service polling can still cause cold-frame stalls. The GPU material retains about 127 KiB of RGBA pixels, with two additional 127 KiB targets while rebuilding (peak three textures including the old cache); this excludes driver allocations and existing artwork/font/material caches.

The lens blurs/refracts the wallpaper, not moving artwork, applets or objects behind each panel. Reflections are an artistic directional-light approximation, not physically traced reflections. Lock cards and native notification/toast assets use offline glass artwork rather than the dynamic shell material. Unsupported render targets retain the compatibility appearance. Render-device-loss recovery remains the existing application's responsibility.

Five references were supplied in this session (Lock, Settings, Apps, Library and Control Center); the existing Home composition was retained. Exact mockup parity is not claimed: Apps retains its functional grid/filter model rather than invented category content; Library uses installed square artwork with titles below rather than fabricated covers/playtime; unavailable system/profile controls stay truthful. No illustration, avatar, game name or download event from the concepts is shipped as fake user data. Existing custom themes can override default assets and positions. Native 720p output trades docked sharpness for fill-rate headroom.
