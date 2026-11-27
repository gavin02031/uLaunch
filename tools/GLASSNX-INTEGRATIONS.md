# GlassNX artwork and Discord
## SteamGridDB hero art (optional, PC side)
1. In GlassNX, Settings > Homebrew > Export title list. The export covers the current folder. Alternatively, use the title ID shown by right-stick title options.
2. Copy /glassnx/titles.tsv to a staging folder containing a glassnx subfolder, or mount your powered-off Switch's SD card on the PC.
3. Install Python 3 and Pillow: py -m pip install Pillow (Windows).
4. Generate your own SteamGridDB API key at https://www.steamgriddb.com/profile/preferences/api. In PowerShell set $env:STEAMGRIDDB_API_KEY="your-key" locally. Never share or commit it.
5. Run py glassnx_artwork.py --sd-root "E:\\" (use actual SD drive or staging folder).
6. Choose a title, matching game and hero. The tool writes a resized JPG under glassnx/art/<title-id>.jpg. If staging, transfer that JPG with SwitchDrop. The launcher reads it when featured, with installed icon fallback. Safe Mode and suspended titles skip optional banners.
7. Optional manual import: py glassnx_artwork.py --sd-root "E:\\" --title-id 0100000000000000 --query "Game name" (replace example ID/name).

The API key is sent only to SteamGridDB over HTTPS; artwork requests do not include the key. No new networking is added to HOME. Art belongs to its respective contributors; do not redistribute without permission. Live API access has not been tested with a user key.

## Discord presence (not built in yet)
Discord officially supports Rich Presence through desktop RPC and supported SDK platforms, not a native Horizon SDK. A separate optional presence bridge is preferable while testing launcher stability. Existing projects include https://github.com/SunResearchInstitute/SwitchPresence-Rewritten (requires a separate sysmodule and PC companion) and https://github.com/llsc12/SwitchRPC (separate native project). Neither is bundled or verified with GlassNX. Do not install additional sysmodules while diagnosing Atmosphere crashes. No Discord token, selfbot, Nintendo login or presence service is added to this update. A future opt-in bridge must clear activity on exit/sleep and be tested independently.

## Direct import on Switch (v0.14 experimental)
Export titles from Settings > Homebrew, then open GlassNX Manager > SteamGridDB artwork. Put your own API key in /glassnx/steamgriddb.key and a trusted CA bundle from https://curl.se/docs/caextract.html in /glassnx/cacert.pem. Select a matching game and JPG banner. No preview yet. Keep the key private and never upload it or include it in logs. This feature is untested on device; use the PC importer if TLS or artwork selection fails.

## From a game's options
In GlassNX highlight a title and press the right stick, then Change artwork. Enter your API key once in the on-device keyboard, choose Cover or Hero, confirm the matching title and preview/apply an image. Requires /glassnx/cacert.pem as above. Close games first; Safe Mode disables imports. Artwork changes affect GlassNX only.

In v0.15 full manual packages include the trusted CA bundle. Enter the API key from a game's options; no separate CA download should be required if the complete release is installed.

## Discord desktop bridge (v0.15, optional)
After hardware stability testing only: use a compatible separately installed https://github.com/SunResearchInstitute/SwitchPresence-Rewritten server, on a trusted private LAN (no port forwarding). It is not included in GlassNX. Create an application named GlassNX at https://discord.com/developers/applications and copy its Application ID (not client secret). On a PC with Discord desktop running, install `py -m pip install pypresence`, then run `py glassnx_presence.py SWITCH_IP APPLICATION_ID`. Ctrl+C clears activity. No user token is needed. The external server reports running games and cannot distinguish all suspended states; it may continue displaying a suspended game. Live testing is required. This is an optional companion, not a native standalone Discord service.
