# Shadow Scepter visibility candidate

| Field | Record |
| --- | --- |
| Baseline | `marsh6487/Shipwright`, `integration/nei-weather-static-actors`, `628deea3cf851af19a18f915e0665d772294c383`. |
| User evidence | Shadow activation shows nothing; the other five elemental wand modes work. Exact installed executable, scene, age, packs/order and Alt state were not supplied. |
| Candidate | `poc/shadow-scepter-visibility-20260930`, directly from the baseline. |
| Scope | Shadow projectile resource resolution, body opacity/eyes, visible missing-resource fallback, target selection and hit height. |
| Preservation | Other five wand sources, shared input, unlocks, magic costs, Shadow speed/lifetime/stun duration, scenes, audio assets and all other renderers are unchanged. |
| Status | Implemented and focused checks pass. Full application build and in-game visibility are not yet verified. No promotion to the integration branch. |
| Recovery | Use the unchanged integration baseline; candidate changes are isolated to the Shadow implementation plus tests and this record. |

## Findings and changes

- The baseline gates drawing on a one-time global `ResourceMgr_FileExists` lookup of one MM resource. A miss remains cached for the rest of the process, even if MM resources are subsequently available. No fallback is drawn.
- Baseline drawing omits the native Boe's explicit environment alpha and its separate eye display list. A preceding translucent draw can leave an unsuitable alpha value.
- The candidate initializes the existing MM loader and resolves all four display-list graphs through its archive-scoped API, including retained vertices/textures. It sets full body opacity and draws the eyes in the opaque pass. Partial/missing models use a small self-contained shadow-puff billboard with bright eyes; no extra O2R is needed for that fallback.
- The baseline projectile travels 25 units above Link's feet but checks a 22-unit sphere against enemy origins. With enemy origins on the same floor, the projectile cannot reach that sphere. The candidate seeks and hits the same enemy focus point in all three dimensions, selects the nearest eligible enemy/boss, and checks live actor membership before dereferencing a retained target.

## Verification

`python3 -B scripts/diagnostics/run_shadow_scepter_tests.py` executes the actual Shadow source with production actor/graphics types and stubbed resource/audio/matrix boundaries.

- Pass: active casts submit a visible fallback with MM absent or with any of the four model parts missing.
- Pass: a complete MM model submits body/material/end/eyes, restores alpha to 255 after an inherited zero alpha, and refreshes strict texture bindings.
- Pass: rendering recovers after a failed lookup; matrix push/pop is balanced; inactive/reset projectiles draw nothing.
- Pass: stuns reach foot-level, torso, elevated and lower focus points, without subtracting health.
- Pass: nearest target, one-shot limit, removed-target safety, range, untargeted flight, expiry and scene reset.
- Pass: the entire production `z_player.c` translation unit, including all six wand modes, passes GCC C23 syntax checking. This baseline emits existing compiler warnings; this is not a warning-free full build.
- Control: compiled baseline Shadow source fails the same missing-MM visibility and level-ground stun checks as expected.
- Pass: `git diff --check`; other five wand source files remain byte-identical to baseline.

GPU execution and the user's runtime configuration are outside these tests.

## Small runtime check

On the candidate build, draw Shadow and cast into open space: verify a visible Boe with eyes, or the dark puff with bright eyes if MM assets are unavailable. Cast at a nearby grounded enemy and verify the stun. Stow the wand during flight, then change rooms and cast again. Repeat with Alt Assets off/on and confirm the other five wand modes still behave as before. Record the candidate build, scene/age and loaded packs with the clip.
