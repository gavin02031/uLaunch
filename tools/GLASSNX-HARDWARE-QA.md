# GlassNX v0.15 device acceptance checklist

Use a backed-up SD card, known working HOME override and stock clocks. Do not add any sysmodule or enable Enhanced Glass during the baseline test. Keep the existing safe-mode file mechanism and recovery manager available. Install the complete release with the console powered off.

1. Cold boot, select profile, idle for 2 minutes. Visit all six pages. Test touch and controller navigation.
2. Launch an installed game, play, press HOME, resume. Repeat 10 times. Check for screenshot ghosting, input or audio bugs, and stalled transitions.
3. Close a suspended game from HOME. Verify Resume/Close disappears and wallpaper is clean. Repeat across three games. Never interrupt saving.
4. Sleep and wake from HOME and during a game. Repeat 5 times. Check that the game still works.
5. Enable About > Performance overlay. Record 30 seconds idle HOME, 30 seconds library navigation, 30 seconds Control Center, and 30 seconds while a game is suspended. Logs record callback FPS and longest frame interval; 60 FPS / 16.7ms is a target, not verified performance.
6. Test custom system icons, all four theme files if installed, and SteamGridDB with no game running. Try invalid key, offline mode and cancellation; original artwork should remain available.
7. Only after baseline passes, repeat 1–5 with Enhanced GPU Glass enabled. If crashes occur, disable it or use Safe Mode.
8. Save the relevant /glassnx/log_uMenu.log, /glassnx/log_uSystem.log and /atmosphere/crash_reports/ or fatal_errors/ records. Note firmware, Atmosphere version, emuMMC, active sysmodules, title, action, and build identifier. Do not include API keys or personal information.

Stop and revert to the known working HOME override if Atmosphere crashes recur. Test Discord separately only after baseline is stable. A host smoke test does not validate Switch GPU behavior.
