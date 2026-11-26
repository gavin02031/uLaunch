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
