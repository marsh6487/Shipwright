# SoH seasonal weather candidate

This candidate ports the ComboShip seasonal presentation to standalone SoH and
includes the winter mist/aurora checkpoint. Skijer's existing Rod season world
functions remain the authority for gameplay. The UI cleanup is scoped to weather
controls in the Audio Editor.

## Sources and boundaries

| Item | Source |
| --- | --- |
| Protected SoH baseline | `integration/nei-weather-static-actors`, `628deea3cf851af19a18f915e0665d772294c383` |
| Candidate | `poc/soh-season-weather-20261012` |
| Existing engine | SoH 9.2.3; LUS `c57da1b4afa775b24b58b2adf93d63d3b561bb65` |
| Presentation donor | ComboShip `980e6fe09`; the same weather tree is published at `1fa85cd66` |
| Paused weather checkpoint | `793e77299958aaed791c846b1905cfd8713cffff`, tree `f22a2c3d43dbffd26b719907209bd6a00299a4fc` |
| Winter checkpoint | `ce03851a3cf1d7acb34aed33fdf48c3df1034844`; see [winter implementation record](winter-mist-aurora-20261012.md) |

`season_scene.cpp` and `season_water_dls.inc` are unchanged. Rod ownership,
season pickups, the season wheel, scene reload, water/ice, desert behavior and
Autumn bean growth still use the saved Rod season. The original saved Winter
rule that retires native fairy actors is preserved. Manual Winter only changes
presentation and does not invoke that gameplay rule.

No scene archive, geometry, collision, UVs, save layout, LUS gitlink or skybox
resource is changed. This does not migrate the host to SoH 9.3.

## Audio Editor layout

Use **Enhancements → Audio Editor → Weather & Seasons**. The old weather list
is moved out of Audio Options, and menu search uses the new location. Existing
CVar names and defaults are retained. Hyrule Field night music remains in
Audio Options under Night Music.

| Group | Controls |
| --- | --- |
| Seasonal Presentation | Seasonal Weather selector; Spring, Summer, Autumn and Winter settings tabs |
| Spring | Rain mode, Cherry Blossoms |
| Summer | Atmosphere, Drifting Fluff, Nighttime Fireflies, Soft Sunbeams |
| Autumn | Rain mode, Falling Leaves |
| Winter | Snow, Windblown Flurries, Freezing Mist, Nighttime Aurora |
| Outdoor Rain | Enable Rain in Outdoor Scenes, Global Rain Mode/Color, Overcast Sky During Rain |
| Rain Audio and Thunder | Rain Volume, Enable Weather Thunder, Thunder Style/Volume/Frequency |
| Diagnostics | Weather audio logging, collapsed by default |

The selector defaults to Follow Game / Rod. Native suppresses seasonal
presentation; manual seasons select outdoor presentation without granting the
Rod or changing world state. Opening a season settings tab does not select that
season. Each effect setting is remembered independently. Summer sunbeams and
winter windblown flurries default off; mist and aurora work with snow off.

## Rendering and material scope

Spring colors canopy foliage pink, applies a light pink understory tint and
leaves ground colors unchanged. Autumn uses the donor warm foliage palette;
winter uses the donor pale blue/white palette and brightness scale. The pinned
SoH renderer does not support the donor's extended grayscale modes, so the
seasonal tint is applied to reversible texture copies.

The material table contains 92 exact identities: reviewed Djipi keys and three
native shared foliage keys. These saved Djipi OoT packs were inspected:

| Pack | SHA-256 |
| --- | --- |
| 20 Scenes Map(3) | `0f79d188684b1459b722437999a44101df48a1aae44fbfc5674ba51f9e9ad2d4` |
| 21 Scenes Map 3DS Textures(3) | `cc2d44fc66b4a46e22db75207d5b8a518f02514a4452f443a02f482668aac81d` |
| 09 Objects World | `0f787a5c53a48396adf3b8c4550c7ab0dbd9835e242cd5ceeaa9174ac9779cfd` |
| 10 Objects World Textures(3DS) | `e49655dae13b304db9b77ff837d2131cb0e6a33cd1be226781e905b157b87be8` |

Only green pixels in those exact resources are transformed. Non-green pixels,
alpha and texture metadata remain intact. Native RGBA5551, RGBA32 and Djipi raw
RGBA resources are supported; matching Native/Alt cache entries are handled
independently. Cold resources use the regular loader. Copies always derive from
the original image, and external resource replacements are respected.

Scene retirement restores original cache bindings, then clears GPU texture
cache keys before releasing the outgoing copy cohort. Fifty scene visits in
the binding regression demonstrate bounded retired cohorts.

The reviewed scene pack does **not** contain Hyrule Field/`spot00`. Its ground
material coverage is therefore unproven. Reviewed actor grass/tree resources
can still tint wherever used. Do not infer coverage of other packs, renamed
materials or every vanilla resource from the texture-format tests. The scene
entries use Djipi's custom names; vanilla scene ground/canopy names have not
been mapped in this candidate.

Seasonal particles use native translucent display lists and a separate private
state: up to 96 spring petals or autumn leaves, 64 supplemental snowflakes,
summer fluff/fireflies/sunbeams and winter terrain mist/aurora. Autumn imports
the four exact donor leaf OTEX resources. Existing native snow actors keep
their layer; supplemental snow is skipped when a live native snow actor exists.
The layers do not own native actors, precipitation counters, gameplay RNG or
save state. Eligibility excludes interiors, underwater views and story/game-over
presentation. Winter renderer details and limits are in the winter record.

## Weather composition

Season rain requests flow through the existing native/global rain resolver.
Native scene and scripted weather retain priority, and global outdoor rain is
independent. Spring defaults dry; Autumn defaults to the original steady rain.
Autumn and Winter keep their original displayed lighting shade without writing
shared bomb/lightning adjustment fields. Native lightning retains its flash.

Active seasonal sky requests yield to living native cloud/snow/Lake/Kakariko
weather controllers. Queued versus already-applied overcast is distinguished
when restoring the native sky handshake. Native selection, an unowned Rod and
inactive seasonal rain do not alter the original global rain policy.

## Verification and runtime handoff

The cumulative CI gate now runs the real seasonal controller/native weather
actor regressions, all particle state and renderer regressions, donor art hashes
and real LUS foliage pixel/cache regressions. CTest also runs the complete
particle renderer instead of state-only checks.

The complete local cumulative gate passes, including all 26 required feature
history baselines and the existing production regressions. Four seasonal CTest
checks pass in the Release configuration: controller/native actors, complete
particle renderer, foliage pixels and foliage bindings. Foliage checks use the
existing always-active `REQUIRE` helper, so Release does not erase them.

The full particle renderer and foliage runners also pass AddressSanitizer and
UndefinedBehaviorSanitizer; leak detection is disabled for the executor's
process-inspection restriction. The foliage runner is verified with the Debian
dependency headers used by CI. Independent source review has no remaining
concrete findings. Changed production C++ bodies compile against the real
pinned SoH/LUS headers, and source formatting/whitespace checks pass.

The existing `GenerateSohOtr` target builds successfully. The generated core
archive contains all four donor autumn leaf resources with exact source bytes;
its SHA-256 is `79810f2045cc70860ec7f0123fc84cfb9a73168a5a0f882419f527031f8050f7`.
Full executable/platform build verification is still pending. Source/object
tests and asset packaging do not establish visual quality or runtime acceptance.

The focused game pass should cover:

1. Audio Editor menu/search access, season settings persistence and toggles;
   manual seasons with an unowned Rod must leave progression/world behavior
   unchanged, while Follow Game / Rod must retain the existing world rules.
2. Reviewed Djipi scenes and objects with Alt on/off: Spring → Autumn → Winter
   → Native, room/scene travel and texture reloads. Check rocks, bark, flowers
   and alpha edges, and verify Hyrule Field separately with its actual archive.
3. Existing native snow, authored rain/storm/cloud/snow tags and story skies;
   test seasonal rain on/off while entering and leaving weather zones.
4. Bomb flashes and lightning under Autumn/Winter; interior, underwater,
   pause, cutscene, save/reload and repeated scene transitions.
5. Winter mist with snow off on slopes and during interpolated camera motion;
   aurora at dusk/night/dawn, depth occlusion and scene far clips.

Implementation, build verification and this configuration-specific game pass
are separate evidence. The protected baseline is not promoted by publishing
this candidate.
