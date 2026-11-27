# GlassNX 1.0 — Prism design and release contract

Status: proposed product direction, first implementation in v0.22. Not a claim of a completed OS or tested 1.0.

## Product identity
GlassNX is a native alternative HOME shell for modified Nintendo Switch consoles, built on uLaunch and Horizon OS. It is not a replacement kernel, firmware, or Nintendo account service. Preserve upstream license and credits, recovery access, existing SD paths, and compatibility. Keep compatibility code below the presentation layer, not in everyday product naming.

Prism is the proposed original visual language. The guiding idea is optical depth, not an imitation of Apple's interface. Use atmospheric wallpaper, consistent curved silhouettes, directional light, restrained highlights, and stable readable foregrounds. Keep artwork the focal point. Reserve color accents for selection, state, warnings and destructive actions. Avoid decoration that reduces readability. Use native Nintendo system applets where a supported replacement is unavailable, and do not misrepresent them as original GlassNX features.

## Shell architecture
- Home: resume-first featured game, game shelf, real device glance, configurable floating dock. No fake playtime, statistics, or artwork.
- Library: games, collections, favorites, recent launches, local search and per-title artwork. Future: controller-operable organization and accurate local play sessions where measurable.
- Apps: categorized installed homebrew and utilities; no invented compatibility or app store.
- Quick Controls: display, sound, connectivity, power, controllers, system state and Activity. Distinguish measured data, last known data and unavailable readings.
- Activity: actual alerts with clear history, quiet mode, severity and later timestamps only where reliable.
- Personalization: theme previews, independent opacity/motion, readable high contrast mode, optional original wallpaper/icon packs, consistent focus treatment.
- Lock and profiles: original welcome, clock and glance widgets; no claim of authentication/security beyond underlying services.
- System and recovery: settings, update/recovery, accessibility, explicit diagnostics and safe mode. Keep privileged applets and game lifecycle stable.

## Interaction and material rules
Design for 1280x720 handheld output first, using the existing logical 1920x1080 layout. Controller and touch parity for all ordinary shell actions; visible focus, directional movement, cancel/back, no focus traps and no accidental launch. Destructive actions use clear confirmation with cancel default. Motion is time-based, optional and disabled in conservative mode. No background capture of suspended games, new sysmodules or clock changes to deliver cosmetic changes. Default to bounded cached glass and simple fallbacks; experimental GPU effects remain opt-in. Do not add per-frame texture allocation or expensive blur without measured justification. Never sacrifice title suspension or recovery to achieve visual fidelity.

## Roadmap (milestones, not promised dates)
| Milestone | Acceptance |
| --- | --- |
| v0.22–0.29 | Prism identity, control navigation, shared material/token library, original icons, reusable sheets and visible focus |
| v0.30–0.49 | Home and game-library organization, controller-first search, lock/profile and consistent activity center |
| v0.50–0.69 | Settings consolidation, accessibility, persistent preferences, local measured session records, update/recovery workflow |
| v0.70–0.89 | Performance and compatibility profiling, visual polish, resource budgets, offline defaults |
| v0.90–0.99 | Release candidates, install/rollback, regression fixes and repeated device acceptance |
| v1.0 | All required checks pass; publish build plus changelog, known limitations and recovery instructions |

## v1.0 exit criteria
- Clean CI compile, host tests and package validation for the exact released commit.
- Actual Switch Lite device runs at stock clocks: target 59–60 HOME callback FPS after warm-up, p95 around 17 ms, less than 1% intervals over 18 ms in steady navigation. Report measurements and stalls; do not label these game FPS or claim performance on untested devices.
- Cold boot, 20 game launch/HOME/resume/close cycles across multiple owned titles, 10 sleep/wake cycles, 30-minute navigation and multi-hour normal play without recurring crashes; preserve saves and existing system behavior.
- Every main screen usable by controller, and touch where supported; reduced motion, contrast, text fitting and all four wallpapers verified at handheld scale.
- Standard and safe rendering pass first; experimental rendering is separate and clearly labeled.
- External SD backup, full package installation and rollback tested on spare media; no claim of atomic FAT update or guaranteed ban safety.
- No unresolved critical lifecycle, loss-of-data, install/recovery or accessibility regressions.

## Design continuity — v0.23
Keep the existing Home composition, optical dock, four wallpaper identities and user-approved artwork presentation. Use iiSU (https://iisu.network/) as a reference for artwork-first handheld navigation and SUMEE! (https://sumee.online/) for coherent built-in applications and personalization. These are inspiration, not dependencies or asset sources. Rework each remaining legacy user-facing surface incrementally: system settings, theme manager, profile/lock, dialogs, folder/entry organization, manager/recovery and original sounds. Preserve upstream credits, config identifiers and safe recovery, plus stable internal interfaces unless there is a tested migration. Native Nintendo applet boundaries remain explicit. New features must use real data and supported APIs.
