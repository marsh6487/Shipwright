# Lost Woods material binding and decorative time pedestal

The Lost Woods scene uses an authored time pedestal and a transplanted Chamber
of Sages material. These opt-ins do not replace the original Temple of Time
sword or infer animation from texture identity.

## Sages platform

Prelude material/paste records can request:

```json
{
  "nativeAnimation": {
    "version": 1,
    "binding": "material-motion",
    "source": "oot.chamber_of_sages.platform",
    "logicalWidth": 32,
    "logicalHeight": 32
  }
}
```

This is the opaque main platform material from
`kenjyanoma_room_0DL_001020`, using `kenjyanoma_room_0Tex_00D618`.
The profile preserves authored sampling shifts and uses native gameplay-frame
scroll: layer 0 moves `(-1, +1)`, layer 1 moves `(+1, +1)`, with logical 32x32
tiles. The import validator requires the expected two RGBA16 render tiles and
rejects unsupported control flow or repeated material setup.

The factory accepts ordinary `custom/prelude/` paths and their single `alt/`
companions. Only the metadata key is normalized. Archive ownership still comes
from the physical resource path, or the explicit resource parent, so an
unrelated higher-priority project's metadata cannot bind the material.

The scene's caustics continue using the existing
`oot.zoras_domain.caustics` profile. Caustic brightness and face selection are
authored in the scene archive; this profile does not introduce dynamic lights.

## Time pedestal

Set a `Bg_Toki_Swd` actor's parameters to exactly `0x4C57`. Other parameter
values keep the original story sword behavior. The authored Lost Woods actor
is in room 10 at `(-736, -63, -2395)`, yaw `15101`.

The actor clones the native child/adult sword cutscene, keeping its player cue,
four camera splines and white transition. Camera and player positions receive
the same rigid transform from the native Temple of Time sword origin
`(-1, 68, 0)` to the authored actor. The native scene destination, story effects
and Temple-specific lighting commands are excluded. The original destination
frame determines when the local age swap occurs.

The player returns to the position and facing captured at interaction, in the
same room, through the existing SwitchAge respawn machinery. This variant does
not award an item, require Prelude of Light, discover the vanilla child/adult
spawn entrances, or change quest/event/info flags. The equipment handoff uses
owned, age-compatible gear and supports an empty sword slot. The ceremonial
adult sword is a live player model, without an ownership grant.

A default-zero field in the existing NEI save metadata distinguishes this
intentional age swap from vanilla adult-save repair. It allows saving and
reloading without the engine granting an unowned Master Sword. Existing saves
retain the original repair behavior; genuine Master Sword acquisition ends
the exception. This does not use a quest flag or change the vanilla save layout.

## Verification

```sh
python3 scripts/diagnostics/run_time_pedestal_tests.py
python3 scripts/diagnostics/run_native_material_probe.py \
  --json-include /path/to/nlohmann/include \
  --spdlog-include /path/to/spdlog/include
```

The material tests exercise the production adapter and binary display-list
parser with controlled archive I/O, including physical archive ownership for
alternate resources. The sword tests compile the relevant production C/C++
functions and native cutscene data with engine boundary fixtures. Neither is
an in-game rendering test or a substitute for a complete platform build.

In game, verify repeated child/adult cycles, camera clearance, exact room-10
return placement, reset/skip behavior, owned and unowned sword saves, and music
resumption. Check the Sages tunnel with alternate assets both enabled and
disabled, and evaluate the authored caustic brightness against water clarity.
