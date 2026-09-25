# Custom pedestal prompt and child sword grip

Repository: `marsh6487/Shipwright` (SoH only).
Baseline: cumulative integration `74edaf1019dfdd0ad1cb40dbece407995ac7a174`.
Candidate branch: `fix/pedestal-prompt-child-grip-20260922`.

The user reports that the A-button prompt requires awkward repositioning around
the stump, and that child Link's left hand and held sword look crooked. The
existing skip/fade, selected weapon, local age swap and return sequence must be
preserved. This follow-up changes neither scene archives nor ComboShip.

## Prompt diagnosis

The current Lost Woods authoring archive, `!Lost Woods Scene - FINAL.o2r`, has
SHA-256 `988c7c0a49e43acdc941894de9ba391f32831a302205e6699251ea80d83979d6`.
Its normal and Alt collision resources are byte-identical, with SHA-256
`0466ee04dc05f19dab79ee9d9bbe42505d6cba99a5675af2840b6f0ba1da22f9`.
The pedestal is at `(-736, -63, -2395)`, yaw `15101`, params `0x4C57`.

The eleven cap triangles are flat at Y=-56 and fit within 35.17 horizontal
units of the anchor. Thus the existing 60-unit offer range already covers the
cap, and loosening the floor-height test is not supported by this geometry.
The neighboring bark descends toward Y=-76 and the ground is Y=-96.

The custom actor still registers the original sword's immovable collision
cylinder: radius 10, height 70, starting at the anchor. Together with Link's
12-unit cylinder, it excludes player centers within 22 units of the sword.
That occupies the cap's center and leaves a narrow approach rim. Saria's
separate collider further restricts one side. The correction omits
this sword-cylinder registration for the exact custom pedestal only. The
actual stump collision, grounded/top-surface checks and stock Temple of Time
sword remain in use.

## Child grip diagnosis

The custom/PAK path composes the selected Master Sword with the current age's
closed fist. The existing correction then rotates the entire left-hand limb
by a half turn, including the fist and wrist. Native child fist vertices extend
along positive local Y; this rotation puts that geometry on negative local Y.

The native child ceremonial sword also has a distinct coordinate basis from
ordinary held Master Sword geometry. Corresponding native blade vertices and
triangle topology permit a measured rigid transform. Reversing the whole limb
does not reproduce that transform. The correction keeps the animated hand
unchanged and applies the native ceremonial transform only to the selected
sword. Selection precedence, age-specific hand resources and adult rendering
remain intact. The sword matrix is allocated from the current graphics frame,
pushed after the fist is drawn, and popped after the sword is drawn.

The two native sword meshes have 136 corresponding vertices and 66 identical
triangles. Rigid registration has 0.388 model-unit RMS component error; the
production matrix differs from the integer native coordinates by at most 1.038
units per component. The former whole-limb half turn misses by up to 1,306 units.
The mapped hilt center lies inside both the ordinary child fist and native
ceremonial hand bounds. Bounds establish gross placement, not finger contact.

The native vertex resources used for the reproducible measurement are:

| Resource | SHA-256 |
| --- | --- |
| `objects/object_link_boy/object_link_boyVtx_010EE8` | `eb0e8fda39539e5859c8f9899a7f4014f00126c07dc98b3a0d1a4f8936ca344f` |
| `objects/object_link_child/object_link_childVtx_00D4E0` | `8ddb6a653f9e5a03b73478bcfb33168e01503dd07189be81abe00f7822d5ae24` |

## Evidence and acceptance

The diagnosis uses the actual authored collision and native model resources,
plus the production actor and rendering paths. Older pedestal clips are no
longer available, so this record does not claim fresh visual runtime proof.

The collision regression failed before the fix at the custom sword's center
shove; the grip regression failed before the fix at the changed joint rotation.
After the fix, these commands passed on the combined candidate:

```sh
python3 -B scripts/diagnostics/run_time_pedestal_tests.py
python3 -B scripts/diagnostics/run_pedestal_sword_selection_tests.py
python3 -B scripts/diagnostics/check_time_pedestal_syntax.py
python3 -B scripts/diagnostics/derive_pedestal_child_grip.py --archive /path/to/native/oot.o2r
```

The pedestal runner requires the nlohmann JSON headers. The geometry derivation
is optional and needs a user-supplied native archive plus the existing preview
reader dependencies; CI uses literal native landmarks and does not need an
archive. The focused CI job runs the first three commands.

The passing checks cover custom-versus-stock cylinder registration,
stump/ground/airborne interaction, both ages, NPC offer priority, unchanged hand
joint rotation, sword-only transform scope and native geometry correspondence.
The grip regression uses the production fixed-point matrix conversion and a
rotated, translated parent limb to check multiplication order and stack cleanup.
The existing independent departure/arrival skips, camera, equipment and
progression checks also pass. Full application builds and runtime acceptance
remain separate gates.

In game, walk across the cap and approach from each side, confirming that the
prompt remains available on top and absent on the ground. Test child withdrawal
with the selected sword and Alt Assets, inspecting the wrist and blade through
the handoff and hold. Repeat the adult insertion and both independently skipped
phases. These are candidate runtime checks, not claimed acceptance.

## September 24 on-stump investigation

The user reaffirmed that interaction must require standing on the stump. No
ground-level offer, larger radius, new NPC movement, or new permissive player
state is introduced by this investigation.

The supplied `Lost_Woods_Tunnel_Energy_POC4.prelude.o2r` has SHA-256
`88087fa2887948d8b6febf493659ca0f77383ed8057c156e2d624932f26d99f8`.
Its collision is byte-identical to the collision recorded above. The eleven
cap polygons (3171–3181) all have normal `(0,32767,0)` and plane distance 56.
The anchor and sampled cap approaches intersect Y=-56; the next overhead
surfaces are above Y=150, outside both the pedestal and player floor probes.
The existing 60-unit offer covers the approximately 35-unit cap.

The player consumes the previous frame's interaction before clearing it; the
PROP pedestal then evaluates the updated player floor and position, and static
Saria's later ITEMACTION update suppresses her talk offer. The custom item offer
already bypasses facing and reserves priority over carry actors. The native
grab handler does not impose another facing or proximity test. These source
and archive checks did not identify the live rejected gate. The previous
fixture's floor query returns a literal -56, so its passing result does not
prove the user's live collision state.

The candidate therefore adds observation only. With the existing
`gDeveloperTools.PreludeLoadProbe` enabled (default 1), a fresh A press within
100 horizontal units of the waiting custom pedestal writes one INFO-level
`[TimePedestalProbe]` line to the Ship log. Holding A does not repeat it. Stock
Temple parameters and active ceremonies do not generate these interaction
probes. Set the existing variable to 0 to disable the probe.

Each line reports the post-evaluation status (`offered`, `not-offered`, or
`ceremony`), player and pedestal positions, distances, both floor heights,
collision owners and polygon addresses, player ground/state flags, current
item offer, interaction and talk actor IDs/parameters, held actor, explosive
state and transition state. `offered` describes the next frame's offer, since
the player has already updated. A `ceremony` status can legitimately show
cutscene guards because that press already started the ceremony.

The hexadecimal `gates` mask records raw guard conditions:

| Bit | Condition |
| --- | --- |
| 0 | Transition active |
| 1 | Cutscene mode |
| 2 | Talking |
| 3 | Pedestal supporting polygon missing |
| 4 | Player not grounded |
| 5 | Different collision owner |
| 6 | Player floor differs from cap by at least 2 units |
| 7 | Player feet differ from cap by at least 4 units |
| 8 | Another nonempty item offer |
| 9 | Horizontal distance at least 60 |
| 10 | Anchor height difference at least 40 |
| 11 | Native blocked movement/action flags |
| 12 | Explosive held |
| 13 | Carrying actor or in cutscene |
| 14 | Play state stopped |

This is diagnostic state, not a replacement eligibility decision; native form
exceptions and player action dispatch remain authoritative. If the actor is
not updated at all, no line is emitted. Capture a short clip of an unsuccessful
on-top A press and a successful one, with the matching Ship log. Those two
samples distinguish floor/state rejection from an offered interaction that
the player's current action did not consume.

The probe regression first failed because no line was emitted, then passed
with coverage for fresh versus held A, off-stump rejection, opt-out, distance,
and exact custom-parameter scope. The full existing time-pedestal runner and
real-header syntax check pass. Prompt reliability remains unresolved pending
that runtime evidence; this candidate does not claim an interaction fix.

Separately, POC4 still places Saria at `(-710,-68,-2373)`. The previously
authorized two-unit clearance move is world Z +2 to `(-710,-68,-2371)`, with
actor ID 42, params `0x7F23`, yaw 16575 and performance unchanged. The retained
placement and both normal/Alt `custom/prelude/spot10_scene/added/room0` compiled
records must agree if that archive correction is restored.
