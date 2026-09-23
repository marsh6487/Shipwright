# Young Epona song riding POC1

Candidate branch: `poc/child-epona-song-riding`, based on the complete integration branch at
`46d01e8ab7e7894102e782a72ba2e4ead65a50c4`.
This retains all 25 required feature baselines, including today's Midna fixes,
Chest Size Matches Contents and the Zora shield ground/swim anchor fix.

## Enable and test

1. Put `Young_Epona_SoH_POC1_Assets.o2r` in the build's `mods` directory.
2. Turn on **Enhancements → Quality of Life → Ride Young Epona as Child**.
   The checkbox also appears in menu search. Reload the area after changing it.
3. As child Link with Epona's Song and an ocarina, play the song in Hyrule Field,
   Lake Hylia, Gerudo Valley, Gerudo's Fortress or Lon Lon Ranch. In randomizer,
   the three required ocarina notes must also be available.
4. Check summon, both mount sides, ride/dismount, native fence jumps, blocked
   surfaces, mounted area transitions and returning to her remembered location.
5. Change to adult: young Epona must be absent and adult Epona's position and
   quest state unchanged. Return to child and check her remembered position.

The animation pack adds eight namespaced MM resources. OoT's existing child
horse rig, textures and five shared animations remain native. MM mount clips
use the child offsets; the rendered body supplies the rider's seat position.
Native horse world collision and jump heights remain intact. Young Epona uses
separate optional save fields, cleared for old saves and new files. The native
ranch child horse hands off to the rideable actor after the learned song.

The pack SHA256 is
`d1990518522c4b781a9b62f3b2df21e9aec5df0eae99542892093359ecab945d`.
Rebuild it with `scripts/diagnostics/build_young_epona_assets.py` using the
user-supplied native MM and OoT archives. The pack does not replace adult TP
Epona POC3; that independent eye-skin fix remains unchanged.

## Evidence and limits

- Production spawn/summon/save behavior fixture passes with undefined-behavior
  sanitizer: off/missing-song/missing-assets/missing-notes guards, all five
  supported scenes, mounted transitions, failed allocation, duplicate summon,
  child location restore and adult-data isolation.
- All five affected C translation units compile against integration headers.
- All 15 animation asset checks pass against the actual donor archives.
- The full cumulative regression suite passes, including Midna draw/audio,
  chest contents/sizing, Zora shield anchors, time pedestal, weather and audio.
- Full platform builds run in GitHub Actions. In-game riding, rendering and
  transitions still require runtime verification. This is a candidate, not an
  accepted master promotion.
- Adult/young Epona cosmetic controls are still pending and are not in this
  urgent port build.
