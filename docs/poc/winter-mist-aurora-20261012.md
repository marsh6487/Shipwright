# Winter mist and aurora on the held SoH weather integration

This records the isolated winter checkpoint. The [combined weather candidate](soh-season-weather-20261012.md)
records its current Audio Editor layout, broader verification and runtime handoff.

| Field | Record |
| --- | --- |
| Preserved source | Shipwright `628deea3cf851af19a18f915e0665d772294c383` remains unchanged. |
| Weather parent | Exact staged tree `f22a2c3d43dbffd26b719907209bd6a00299a4fc`, saved as local checkpoint `793e77299958aaed791c846b1905cfd8713cffff`. This is the held seasonal-weather implementation, not an accepted runtime baseline. |
| Candidate | `poc/winter-mist-aurora-20261012`, a separate worktree containing that weather parent and the winter additions. |
| Scope | Independent freezing mist and nighttime aurora in SoH Audio Editor; private winter state and supplemental rendering. Two inherited weather-integration failures are repaired here. |
| Preservation | Native season ownership/Rod progression, scene geometry/collision, texture identities/UVs/alpha, accepted assets, seasonal tints, spring/autumn/summer layers, native snow actors, rain/BGM/SFX routing, and the original skybox resources. |
| Dependencies | The exact existing LUS gitlink `c57da1b4afa775b24b58b2adf93d63d3b561bb65`; no dependency revision change. |
| Promotion | Implemented candidate; no runtime acceptance or master promotion. The broader weather publication remains held. |

## Controls and visible behavior

Audio Editor → Weather & Seasons → Winter has separate **Winter
Freezing Mist** and **Winter Nighttime Aurora** checkboxes. Both default to on,
and each works with supplemental Winter Snow disabled. Snow and Windblown
Flurries retain their existing controls and native snow actor behavior.

Freezing mist uses up to twelve broad, pale blue, feathered sheets. Their centers
sample actual terrain collision; absent floor samples draw nothing. Wisps stay
low, drift slowly, and fade at entry and view edges. Floor queries are cached,
refreshed every four gameplay frames or when a wisp is recycled. Mist positions
and floor queries freeze during pause and repeated interpolation draws. Each
wisp has a world transform and a distinct interpolation lifetime, keeping it
grounded as the camera moves and preventing lifetime reuse after re-entry.

The aurora uses three softly shaded blue/green ribbons with fixed compass
orientation. It translates with the sky rather than rotating with camera yaw.
It fades from 18:00 to 20:00, stays visible through the night, and fades from
04:00 to 06:00. Normal-sky eligibility and the existing outdoor/story/view
exclusions apply. Buildings and mountains use the normal depth test. The
aurora's radius follows the current far clip plane and stays within it.

Both effects use native translucent geometry and vertex colors. They introduce
no skybox texture replacements, texture archive members, dedicated actors,
gameplay RNG calls, save writes, or precipitation/fog ownership. Fully disabled
or culled layers emit no material setup. Drawn layers restore the production
textured XLU material. At maximum visibility, winter adds 252 dynamic vertices,
up to twelve mist matrices and one sky matrix; together with supplemental snow
the matrix ceiling is 77.

## Repairs to the inherited weather checkpoint

- The existing `ScriptedRainRetainsItsExistingSkyOwnership` regression failed
  because overcast ownership cleared a story-rain handoff. Scripted rain now
  relinquishes the enhancement's ownership without issuing a clear-sky change.
- The foliage binding regression referenced a missing `Retire()` operation.
  The renderer's original/copy texture cohort now remains alive through
  `OnPlayDestroy` restoration, then the incoming `OnSceneInit` clears the GPU
  texture cache before releasing it. Exit performs the same retirement. The
  fixture uses the actual LUS cache-clear interface, and repeated scene cohorts
  are bounded. External reload/replacement guards remain intact.

## Verification and limits

The new renderer regression first failed because disabling snow disabled the
entire winter layer. The state regression first failed on the absent winter
implementation. Review then reproduced camera-relative mist displacement at
intermediate frames; a submitted-transform/vertex regression failed before the
world-transform correction. A separate re-entry test reproduced interpolation
lifetime reuse before the generation correction.

Checks exercise production state/controller/render bodies and real engine
headers, not a parallel weather implementation:

- `run_soh_season_particle_tests.py`: falling, summer and winter states;
  native renderer; independent controls; day/night/dusk/dawn; pause and camera
  movement; terrain and missing floors; normal-sky/far-plane constraints;
  native snow coexistence; source/save/environment preservation; texture state,
  matrices, allocation/command bounds; scene/room/rollback cleanup; exact donor
  autumn texture bytes. AddressSanitizer and UndefinedBehaviorSanitizer cover
  the full runner. Leak detection is disabled because this environment blocks
  the process inspection it needs.
- `run_soh_season_weather_tests.py`: native/Rod ownership and season controls,
  private rain cadence, native/story weather, overcast and lighting handoffs.
- `run_rain_runtime_test.py`: actual native weather actors, overlaps, room
  persistence, scene/age setup, scripted weather, revisits and sky cleanup.
- `run_soh_season_foliage_tests.py`: real native/HD texture data, pixel/alpha/
  metadata preservation, Alt/native cache ownership and repeated retirement,
  with AddressSanitizer and UndefinedBehaviorSanitizer.
- Production C++20 object compilation includes AudioEditor, SeasonWeather,
  GlobalOutdoorRain, SeasonFoliage, SeasonParticles and SeasonWinterState with
  the actual SoH/LUS headers. This is object compilation, not a full application
  link. Clang-format 14 and source-whitespace checks cover the winter changes.

No game or GPU session was launched. Full application/hosted build validation,
visual quality, actual menu input/capture behavior, frame interpolation on a
real driver, and scene/pack/Alt parity remain untested. No claim of an existing
accepted screenshot proving these additions is made.

Independent read-only review closed the camera/interpolation finding after its
regression and correction. No Critical, Important or Minor code findings remain;
that verdict is readiness for the focused runtime POC, not gameplay acceptance.

## Focused runtime review

1. Use the combined candidate executable and the weather integration's matching
   assets. With the installed Djipi/native configuration, select manual Winter
   in Hyrule Field, turn Snow off, and compare Mist off/on during the day.
   Walk across slopes and rotate/move the camera at the normal interpolation
   FPS. The mist should stay low and leave the route ahead readable.
2. At night, toggle Aurora separately. Check slow blue/green ribbons against
   the existing sky, camera yaw, mountains/buildings, and dusk/dawn transitions.
   Snow off must not disable either effect. A scene without a normal open sky
   should not receive an aurora.
3. Compare snow alone with both new checkboxes off, then combine all three.
   Check pause toggles, interior/underwater/story exclusions, room re-entry and
   switching Winter → Spring → Autumn → Summer → Native. Existing season
   assets, tints, audio and native particle owners should retain their behavior.

## Integration and recovery

This branch already contains the complete held weather tree. It can be used
directly for the combined weather build; the final winter commit can instead be
cherry-picked after committing the identical weather parent in another checkout.
Do not apply it to an earlier branch lacking SeasonWeather/SeasonParticles.
Recover the original integration with `628deea3cf851af19a18f915e0665d772294c383`,
or the exact held weather-only state with
`793e77299958aaed791c846b1905cfd8713cffff`. The paused source checkout was not
edited while this candidate was prepared.
