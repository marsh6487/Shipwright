# OoT transformation cosmetics candidate

Repository: `marsh6487/Shipwright`.
Baseline: the integrated SoH branch `integration/nei-weather-static-actors` at
`9a76eb637a4090279ccbcca14e5c043412ea766b`, plus the existing native TorchFlame
sampling candidate from PR #13 at `0b745bc8b9ce361b3e3cd6ed9dc37e7d220d843a`.
The baseline rain, audio, actor behavior, geometry and archives are preserved.
The TorchFlame loader is already included through that parent; it is not
reimplemented by this patch. This work targets Shipwright, not ComboShip.

## Changes

- Cosmetic Editor > Mods groups loaded tagged materials by Child, Adult,
  Deku, Goron, Zora and Fierce Deity. Existing custom CVar keys and manifest
  category/entry names remain intact. Only forms with available entries appear.
  The user's 3DS Custom Colors manifest also uses `object_link_goy` for Goron
  materials; skeleton/category identification covers that custom directory.
- Mod colors participate in Randomize All and the existing automatic modes.
  On New Scene generates fresh colors on each scene initialization, including
  reentry. Seeded modes retain their repeatability, and locked colors stay put.
  Global reset/lock/rainbow controls include the same custom entries.
- Rescanning uses color-command position instead of original RGB equality,
  and refreshes CVar string pointers after moving/sorting entries. Alt changes
  rescan bindings. Cached barrier copies receive live tagged color updates.
- Zora barrier land/idle-water scale now matches MM's intensity-dependent
  scale, also used by fast swimming: `intensity * (10 / 51) * 0.01` in world
  space. At full intensity the land effect is twice its former linear size.
- Cosmetic Editor > Effects > Magic Effects adds Zora Magic Shield,
  Zora Shield Glow and Zora Shield Highlights. The native two-layer shader
  keeps its alpha, primitive LOD fields, texture scroll and geometry.
  These controls share the existing Zora shield mod's custom color keys, so
  its Mods controls and the native effect controls stay consistent.
  Unrecognized custom shader layouts retain their own tagged controls.

The shield change affects visuals only. Magic drain, damage collider, movement,
immunity, sound and environment lighting are not changed. It also applies to
the existing human Zora-tunic barrier, which shares the same draw function.

## Verification and acceptance

Focused production-code fixtures cover scanner bindings, grouping, rescan,
randomization/locks/seeded modes, cached-copy colors, and the barrier's matrix
scale and emitted color commands. Both runners are included in the Shipwright CI checks:

```
python3 scripts/diagnostics/run_oot_custom_cosmetics_tests.py
python3 scripts/diagnostics/run_zora_barrier_cosmetics_tests.py
```

These fixtures replace resource/graphics services; they do not render the game.
Full Windows/Linux builds and runtime acceptance are separate evidence.

Runtime check: enable the relevant form packs in the OoT mod set; inspect each
loaded form's Mods group; change and reset one color; use On New Scene and leave
and reenter twice, also checking a locked color. Test Zora R+B on land and R
underwater, the three shield colors, reset, charge/fade, and an Alt-assets toggle.
Keep the prior build until the candidate is accepted.
