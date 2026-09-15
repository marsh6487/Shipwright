# MM static actor catalogue and fountain material design

Date: 2026-09-15
Status: Design approved by cor on 2026-09-15; implementation planning authorized. No implementation or runtime verification claimed.

## Baseline and release boundary

Repository: marsh6487/Shipwright.
Base branch: probe/prelude-native-materials.
Pinned base: 0a4dc0641c3a40925733bae2dafebe5f5b89e7de.

This bundle expands the existing static En_Viewer catalogue and adds one narrowly identified MM fountain material animation. It does not port native actor gameplay, introduce a general material editor, or change unrelated audio behavior.

Preserve existing actor placements, parameter values, collision, asset isolation, and native material profiles. Keep existing Hyrule Field night music and other unrelated fork fixes unchanged. Generalized scene night music is outside this bundle. Do not automatically merge other development branches.

## 1. Catalogue additions

Reuse the existing static actor registry and archive-backed presentation adapters. Append new identities without renumbering existing entries. Happy Mask Salesman already has a disabled reservation: complete that entry instead of allocating another.

| Actor | Parameter allocation to validate | Presentation |
| --- | --- | --- |
| Happy Mask Salesman | Existing 0x7E09 / 0x7E19 / 0x7E29 | Idle, hands clasped, arms out |
| Keaton | Proposed 0x7E0A / 0x7E1A / 0x7E2A | Idle, chuckle, celebrate |
| Child Kafei | Proposed 0x7E0B / 0x7E1B | Idle and expressive idle, subject to compatible clip verification |
| Lulu | Proposed 0x7E0C / 0x7E1C / 0x7E2C / 0x7E3C | Quiet idle, side-looking idle, singing, looking around |

Validate allocation against the complete decoder before reserving new values. Unsupported pose values must follow the existing documented fallback policy and never address invalid resources.

Lulu's four approved choices are presentation animations. Singing must not start music, Malon's singing path, ocarina interactions, or cutscenes. Use authored facial behavior and disable tracking where it would distort a performance. Verify seamless looping, especially looking around; do not silently substitute a different fourth pose if a clip needs a transition strategy.

Keaton and Happy Mask Salesman use ordinary NPC presentation paths where verified. No quiz, mask requirements, rewards, schedules, native actor singleton rules, or story progression side effects are imported.

### Kafei compatibility gate

Native MM Kafei is player-style: Player animation sampling and an LOD skeleton, not a drop-in AnimationHeader NPC. The current loader can return both ordinary and player animation resources; an unchecked cast is not a valid solution.

Before enabling his catalogue entry, prove a dedicated presentation-only path can load his compatible skeleton and clips, sample animation with the correct resource type, draw the required LOD limbs, and select face textures with per-instance state. Reuse existing player-animation infrastructure only where it does not initialize or mutate the live player.

Do not call native Kafei init/update or inherit shared player, Tatl, inventory, schedule, singleton, or cutscene dependencies. Test multiple Kafei instances alongside Link. If the narrow adapter requires a wider player-system port, stop and report that scope increase rather than silently dropping Kafei or shipping an unsafe approximation.

## 2. Dialogue and interaction

Separate talk eligibility from placement, collision, targeting, and drawing. Returning text ID zero is insufficient: the existing offer-talk path has no zero-text guard and can still offer conversation. A no-talk actor must bypass both talk offering and request processing.

| Actor | Intended dialogue policy |
| --- | --- |
| Impa | Replace pre-Zelda sleeping-Talon text with verified Impa introduction; retain current post-Zelda line |
| Adult Zelda | Use child Zelda's princess introduction before meeting Zelda and carried-away apology afterward |
| Sheik | Retain current 0x700F / 0x7010 if the safety audit confirms ordinary static conversation |
| Treasure Chest Shop Gal | Replace inherited Great Fairy line with actor-appropriate, self-contained text |
| Skull Kid | Replace inherited Great Fairy line with actor-appropriate, self-contained text |
| Phantom Ganon | Remove talk prompt/dialogue only; preserve physical presentation and placement |
| Other existing entries | Leave dialogue unchanged |
| New entries | Never inherit the Great Fairy default or unverified MM message IDs |

Candidate IDs for validation against the fork's actual message resources are Impa 0x708D/0x708E and Zelda 0x70FF/0x70FE. These are not permission to assume an external message dump matches the shipped archive. The princess introduction and “What is your name?” are not assumed to be one message.

Before coding new dialogue, prepare the exact Treasure Shop Gal, Skull Kid, and new-actor text choices for user approval. New authored messages, if selected, must occupy collision-checked IDs and use an isolated static conversation path. This content checkpoint does not reopen the agreed actor roster.

Audit selected messages for chained text, choices, event controls, and close behavior. A message classified EVENT may need closing logic, but must not dispatch the donor actor's action state or song cutscene. Test repeat conversations and both relevant progression branches without granting songs, items, or quest flags.

### Phantom Ganon Y-anchor invariant

Explicit user requirement: preserve the already implemented Y-anchor fix.

At the pinned base, static_story_ganon.c sets shapeYOffset to 1000.0f. At actor scale 0.01, that raises the rendered model by exactly 10 world units. EnViewer initialization passes the presentation offset to ActorShape_Init.

Keep authored actor/world Y unchanged; do not replace this with a world-position translation, change the scale, or apply the offset twice. Dialogue cleanup must not alter this presentation descriptor, collision anchoring, or saved placement values. Preserve soh/tests/static_story_ganon_test.c, including its offset assertion, and add integration coverage that the viewer still consumes the offset. Runtime-check an existing placement at the same saved coordinates before and after the change.

## 3. Skull Kid texture delta audit

The base already contains archive texture-routing changes, including strict private-archive texture binding. First compare the intended outstanding texture work with that implementation and the supplied assets. Do not reapply an old patch blindly.

Retain strict graph ownership, nested display-list and vertex handling, texture storage lifetime, mask/head/eye bindings, and the existing companion crash safeguards. Verify the intended texture override behavior explicitly: strict base-archive loading does not itself prove replacement texture support.

Test both existing poses, multiple instances, scene re-entry, and sustained rendering across graphics-pool reuse. Only claim the dusty texture work completed after identifying and verifying its actual remaining delta.

## 4. Portable MM fountain material

Extend the current bounded native material profile system, not a global segment substitution. Recognize the fountain's proven donor material from stable export provenance, so future copies and retextures can inherit the profile without an exposed Prelude material UI. Do not identify it solely by a user-facing “Fountain” label or replacement texture name.

### Donor evidence gate

The supplied runtime archive contains fountain recipes with scene-texture labels including Z2_00KEIKOKUTex_031B98 and Z2_00KEIKOKUTex_02A850. MM Bg_Keikoku_Spr draws three animated object materials, but their relationship to these exported recipes is not yet proven.

Before implementing an exact-MM profile, resolve which donor display list/material each animated exported surface represents. Obtain actual animation parameters: segment, layer count, signed scroll increments, tile dimensions, frame clock, and any phase or color behavior. XML header addresses alone are not scroll values. Do not claim the three native draw calls equal three user placements: the user's scene has four fountains.

If the needed donor data is absent from accessible resources, ask for the relevant archive/export rather than inventing constants or substituting the Lake Hylia profile.

### Binding and runtime behavior

Keep resolution scoped to the exact archive owning the generated display list. Match verified provenance and command structure; an ambiguous or conflicting match remains unpatched with a useful diagnostic.

Represent the verified fountain parameters in a bounded profile. If the donor requires a single scrolling layer or unequal layer dimensions, extend the profile and insertion validation specifically for that structure. Do not simply remove existing two-tile or control-flow safeguards.

Preserve hash-payload parsing, command boundaries, segment isolation, persistent display-list lifetime, and frame interpolation. Keep the existing Lake Hylia, Kakariko pool, and Lost Woods sheet profiles behaviorally unchanged. Retextures may change appearance, but must not silently change verified native tile geometry or animation timing.

## 5. Verification and delivery

Implementation work should be reviewable as dialogue policy, ordinary MM presentations, Kafei adapter, Skull Kid delta, and fountain profile changes.

Required automated coverage:

- Catalogue identity/pose decoding, disabled-entry handling, and missing-resource failure behavior.
- Talk eligibility independent of collision and presentation; Phantom Ganon offset and initialization integration.
- Correct animation resource typing and independent actor instance state.
- Profile recognition, rejection of ambiguous/unsafe command streams, signed scroll math, tile dimensions, and old-profile regression cases.

Required runtime checks:

- All new poses, face bindings, authored yaw, repeated scene entry, and multiple instances.
- Lulu singing remains audio-neutral; Kafei never mutates Link.
- Dialogue closes correctly with no unintended cutscene, reward, or flag changes.
- Phantom Ganon remains at the same saved placement with the same model-only lift.
- Skull Kid textures and companion remain stable.
- All four fountain placements, relevant day/night exports, a retexture, and the three existing native profiles.
- No segment or material state leaking into adjacent geometry.

Use the normal candidate build for runtime verification; do not require a separate diagnostics-only custom build. Record which checks actually ran. Missing game assets or a runnable environment are explicit verification limits, not passing results.

Release acceptance requires both compatibility gates to be resolved with source/asset evidence and the runtime checklist completed. A documentation commit alone does not establish feature completion.

## Source anchors

Fork files at the pinned base:

- soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c
- soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c
- soh/src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.c
- soh/src/overlays/actors/ovl_En_Viewer/static_story_ganon.c
- soh/tests/static_story_ganon_test.c
- soh/mods/transformation_masks/assets/mm_asset_loader.cpp
- soh/soh/Enhancements/Graphics/NativeMaterialProfile.cpp
- soh/soh/Enhancements/Graphics/PreludeNativeMaterialScroll.cpp

MM donor inspection: zeldaret/mm actor sources En_Test3, En_Zov, and Bg_Keikoku_Spr, plus object_kitan/object_zov/object_test3 XML definitions. Pin the donor revision when extracting implementation constants; the upstream main branch is not a reproducible resource version.
