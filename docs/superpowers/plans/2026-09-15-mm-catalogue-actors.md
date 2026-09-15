# Static MM Catalogue Actor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Safely implement the known dialogue changes and establish verified donor contracts for the approved MM catalogue expansion.

**Architecture:** Keep policy in the existing pure-C catalogue and engine calls in En_Viewer. Test the production talk function with a small extracted-function harness. New render adapters stay gated on pinned skeleton, animation, face, and resource-lifetime evidence.

**Tech Stack:** C, C++20, existing REQUIRE test macro, Python unittest, CMake/CTest, strict MM archive loading.

**Spec:** ../specs/2026-09-15-mm-catalogue-fountain-design.md.

## Global Constraints

- Pinned base: 0a4dc0641c3a40925733bae2dafebe5f5b89e7de.
- Preserve existing actor placements, parameter values, collision, asset isolation, and native material profiles.
- Keep existing Hyrule Field night music and other unrelated fork fixes unchanged.
- Generalized scene night music is outside this bundle.
- Do not automatically merge other development branches.
- Keep authored actor/world Y unchanged; do not replace this with a world-position translation, change the scale, or apply the offset twice.
- Phantom Ganon retains shapeYOffset = 1000.0f and scale = 0.01f.
- Use the normal candidate build for runtime verification; do not require a separate diagnostics-only custom build.

---

Read the release index first. A1–A3 are executable from verified source; A4 requires actual message resources. A5 resolves the asset-dependent work and produces the implementation appendices specified below. Do not enable new entries before those appendices have concrete donor data.

## File map

- Modify soh/tests/static_story_actor_test.c: release-active assertions and scoped dialogue tests.
- Preserve/extend soh/tests/static_story_ganon_test.c: model offset, scale, collision invariants.
- Modify soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c/.h: pure talk policy and verified selector changes.
- Modify soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c: early talk guard only in A3.
- Create soh/tests/static_story_viewer_talk_fixture.c: engine call spies for the real extracted talk function.
- Create scripts/diagnostics/run_static_story_viewer_test.py: extract production functions and compile fixture.
- Modify scripts/diagnostics/run_stabilization_tests.sh: run the new harness.
- Read soh/src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.c/.h and z_en_viewer.h for subsequent adapters.
- Read soh/mods/transformation_masks/assets/mm_asset_loader.cpp/.h and mm_display_list_patch.cpp/.h for asset ownership.
- Create docs/superpowers/reports/2026-09-15-mm-actor-evidence.md during execution: pinned resource contracts, dialogue decisions, and remaining texture delta.

## A1. Make actor regression checks effective in Release

**Files:** soh/tests/static_story_actor_test.c; soh/tests/static_story_ganon_test.c.
**Interfaces:** consumes the existing tests and soh/tests/test_require.h; produces tests that still evaluate under -DNDEBUG. No production API changes.

- [ ] Inspect all assertions in static_story_actor_test.c. Replace its assert include with test_require.h and mechanically change assert(...) to REQUIRE(...), preserving every expression. Do not weaken the existing catalogue checks.
- [ ] Inspect expressions for side effects; move side-effecting calls before REQUIRE checks rather than depending on macro behavior.
- [ ] Extend the Phantom Ganon test with the actual catalogue scale and collider values, rather than only multiplying by a literal:

```c
const StaticStoryActorDefinition* definition =
    StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_PHANTOM_GANON);
REQUIRE(definition != NULL);
REQUIRE(definition->scale == 0.01f);
REQUIRE(phantom->shapeYOffset == 1000.0f);
REQUIRE(phantom->shapeYOffset * definition->scale == 10.0f);
REQUIRE(definition->colliderRadius == 35);
REQUIRE(definition->colliderHeight == 100);
REQUIRE(definition->colliderYShift == 0);
```

- [ ] Run the focused tests under -DNDEBUG using a fresh temporary directory:

```bash
actor_test_build=$(mktemp -d /tmp/soh-actor-tests.XXXXXX)
cc -std=c11 -g -DNDEBUG -Isoh/include soh/tests/static_story_actor_test.c soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c -o "$actor_test_build/actor"
cc -std=c11 -g -DNDEBUG -Isoh/include soh/tests/static_story_ganon_test.c soh/src/overlays/actors/ovl_En_Viewer/static_story_ganon.c soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c -o "$actor_test_build/ganon"
"$actor_test_build/actor"
"$actor_test_build/ganon"
```

Expected: both exit 0 with real checks active. Investigate any baseline failure before changing production behavior.
- [ ] Prove the harness detects failure: in this task's test edit only, temporarily change the expected offset from 1000.0f to 999.0f with apply_patch, rebuild/run ganon, observe nonzero exit, then restore the correct expectation and rerun. Do not change production offset for this check.
- [ ] Run the full stabilization script. Commit only the two test files with message "test: keep actor and anchor assertions active in release".

## A2. Explicit Phantom Ganon no-talk policy

**Files:** static_story_actor.c/.h and static_story_actor_test.c, at the full paths in the file map.
**Interfaces:** consumes StaticStoryActorType and existing definition/selector APIs; produces:
```c
bool StaticStoryActor_CanTalk(StaticStoryActorType type);
```
No collision or render API changes.

- [ ] Add failing tests, keeping other actors' existing dialogue requirements:

```c
REQUIRE(!StaticStoryActor_CanTalk(STATIC_STORY_ACTOR_NONE));
REQUIRE(!StaticStoryActor_CanTalk(STATIC_STORY_ACTOR_MAX));
REQUIRE(!StaticStoryActor_CanTalk(STATIC_STORY_ACTOR_PHANTOM_GANON));
REQUIRE(StaticStoryActor_CanTalk(STATIC_STORY_ACTOR_GREAT_FAIRY));
REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_PHANTOM_GANON, &early) == 0);
REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_PHANTOM_GANON, &complete) == 0);
```

In the existing type loop, require text != 0 only when CanTalk(type) is true; otherwise require text == 0. Do not simply delete the two nonzero assertions.
- [ ] Build/run the focused actor test; expect an absent-function build failure first, then policy assertions to fail if temporarily stubbed.
- [ ] Add the declaration and minimal implementation:

```c
bool StaticStoryActor_CanTalk(StaticStoryActorType type) {
    return StaticStoryActor_GetDefinition(type) != NULL &&
           type != STATIC_STORY_ACTOR_PHANTOM_GANON;
}
```

Move Phantom Ganon out of the Great Fairy fallback group in SelectTextId and give it its own case returning 0. Leave all other mappings unchanged in this task.
- [ ] Run actor, ganon, MM actor, and stabilization tests. This task changes pure policy only; the prompt is not considered removed until A3.
- [ ] Commit the three policy/test files: "feat: define static actor talk eligibility".

## A3. Apply no-talk policy to the actual viewer path

**Files:** z_en_viewer.c; the new fixture and Python runner; run_stabilization_tests.sh.
**Interfaces:** consumes CanTalk(type); production EnViewerStatic_OfferTalk(EnViewer*, PlayState*) remains unchanged externally. Test runner returns nonzero for extraction, compilation, or behavioral failure.

- [ ] Create the C fixture below through apply_patch. It includes the actual catalogue header and production function bodies generated by the runner, not a rewritten talk implementation:

```c
#include <stdbool.h>
#include <stdint.h>
#include "test_require.h"
#include "../src/overlays/actors/ovl_En_Viewer/static_story_actor.h"

enum { TEXT_STATE_EVENT = 1, TEXT_STATE_CLOSING = 2,
       NPC_TALK_STATE_IDLE = 0, NPC_TALK_STATE_TALKING = 1 };
typedef struct { uint16_t textId; float authoredY; } Actor;
typedef struct {
    Actor actor;
    struct {
        uint8_t type;
        bool talking, tracking;
        struct { int talkState; } interactInfo;
    } staticState;
} EnViewer;
typedef struct { int msgCtx; } PlayState;
static int offers, requests, closes;
static bool acceptRequest, advance;
static StaticStoryProgression savedProgression;
static StaticStoryProgression EnViewerStatic_ReadProgression(void) { return savedProgression; }
static int Message_GetState(int* state) { return *state; }
static bool Message_ShouldAdvance(PlayState* play) { (void)play; return advance; }
static void Message_CloseTextbox(PlayState* play) { (void)play; ++closes; }
static bool Actor_ProcessTalkRequest(Actor* actor, PlayState* play) {
    (void)actor; (void)play; ++requests; return acceptRequest;
}
static bool Actor_OfferTalk(Actor* actor, PlayState* play, float distance) {
    (void)actor; (void)play; (void)distance; ++offers; return true;
}
#include "viewer_talk.inc"

int main(void) {
    PlayState play = {0};
    EnViewer phantom = {0};
    phantom.staticState.type = STATIC_STORY_ACTOR_PHANTOM_GANON;
    phantom.actor.authoredY = 123.0f;
    acceptRequest = true;
    EnViewerStatic_OfferTalk(&phantom, &play);
    REQUIRE(offers == 0 && requests == 0 && closes == 0);
    REQUIRE(!phantom.staticState.talking && phantom.actor.textId == 0);
    REQUIRE(phantom.actor.authoredY == 123.0f);

    phantom.staticState.talking = true;
    phantom.staticState.tracking = true;
    EnViewerStatic_OfferTalk(&phantom, &play);
    REQUIRE(!phantom.staticState.talking && !phantom.staticState.tracking);
    REQUIRE(offers == 0 && requests == 0 && closes == 0);

    EnViewer fairy = {0};
    fairy.staticState.type = STATIC_STORY_ACTOR_GREAT_FAIRY;
    acceptRequest = false;
    EnViewerStatic_OfferTalk(&fairy, &play);
    REQUIRE(offers == 1 && requests == 1);
    REQUIRE(fairy.actor.textId == 0x00DB);
    acceptRequest = true;
    EnViewerStatic_OfferTalk(&fairy, &play);
    REQUIRE(fairy.staticState.talking && requests == 2);

    play.msgCtx = TEXT_STATE_EVENT;
    advance = false;
    EnViewerStatic_OfferTalk(&fairy, &play);
    REQUIRE(closes == 0 && fairy.staticState.talking);
    advance = true;
    EnViewerStatic_OfferTalk(&fairy, &play);
    REQUIRE(closes == 1 && !fairy.staticState.talking);
    REQUIRE(fairy.staticState.interactInfo.talkState == NPC_TALK_STATE_IDLE);
    REQUIRE(offers == 1 && requests == 2);

    EnViewer invalid = {0};
    EnViewerStatic_OfferTalk(&invalid, &play);
    REQUIRE(offers == 1 && requests == 2);
    return 0;
}
```

- [ ] Create the runner below using apply_patch. Its extraction is intentionally strict: a function-layout change must fail instead of testing a stale copy.

```python
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
VIEWER = ROOT / "soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c"

def extract(source, name):
    pattern = r"^(?:static )?void " + re.escape(name) + r"\([^\n]*\) \{.*?^\}"
    matches = re.findall(pattern, source, re.M | re.S)
    if len(matches) != 1:
        raise RuntimeError("Expected one production body for " + name)
    return matches[0]

def main():
    source = VIEWER.read_text()
    bodies = "\n\n".join(extract(source, name) for name in (
        "EnViewerStatic_RestorePlacementPose", "EnViewerStatic_OfferTalk"))
    # Structural wiring check, not a substitute for rendered placement testing.
    anchor = re.search(
        r"ActorShape_Init\(&this->actor.shape,\s*"
        r"ganonPresentation != NULL \? ganonPresentation->shapeYOffset : 0.0f,",
        source)
    if anchor is None:
        raise RuntimeError("Re-review Phantom Ganon shape-offset integration")
    with tempfile.TemporaryDirectory(prefix="soh-viewer-talk-") as temp_name:
        temp = Path(temp_name)
        (temp / "viewer_talk.inc").write_text(bodies)
        binary = temp / "viewer_talk"
        subprocess.run([
            os.environ.get("CC", "cc"), "-std=c11", "-g", "-DNDEBUG",
            "-I" + str(ROOT / "soh/include"), "-I" + str(temp),
            str(ROOT / "soh/tests/static_story_viewer_talk_fixture.c"),
            str(ROOT / "soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c"),
            "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)

if __name__ == "__main__":
    main()
```

- [ ] Run `python3 -B scripts/diagnostics/run_static_story_viewer_test.py`. Before runtime wiring, expect a failed call-count assertion for Phantom Ganon.
- [ ] Insert this guard after the declaration of progression and before the existing talking-state branch:

```c
if (!StaticStoryActor_CanTalk((StaticStoryActorType)this->staticState.type)) {
    this->actor.textId = 0;
    EnViewerStatic_RestorePlacementPose(this);
    return;
}
```

After SelectTextId, make the existing null-definition check also reject textId == 0 before Actor_ProcessTalkRequest. Preserve event-close behavior for allowed actors. Do not clear attention flags, remove the collider, alter the Ganon descriptor, or move actor.world.pos.
- [ ] Rerun the harness and complete actor/stabilization tests. Add the runner invocation to run_stabilization_tests.sh using `python3 -B scripts/diagnostics/run_static_story_viewer_test.py`.
- [ ] Runtime-check an existing Phantom placement at the same saved coordinates: same model lift, collider and targeting, no talk prompt. The fixture's authoredY is a mutation sentinel; it is not a graphics integration test.
- [ ] Commit only the viewer change, fixture, runner and script wiring: "fix: suppress Phantom Ganon talk without changing placement".

## A4. Verified dialogue selection and content checkpoint

**Files:** static_story_actor.c; static_story_actor_test.c; docs/superpowers/reports/2026-09-15-mm-actor-evidence.md.
**Interfaces:** consumes read-only StaticStoryProgression; produces verified selector mappings with no new progression or native actor callbacks.

- [ ] Read the actual message table resources from the fork's active OOT archive/version. Record archive hash, selected IDs, rendered text, final control code, chained IDs/choices if any, and corresponding message-system close behavior. If unavailable, request the relevant archive; an external dump alone cannot close this gate.
- [ ] Validate the user-approved existing-text candidates: Impa 0x708D before Zelda and 0x708E afterward; Adult Zelda 0x70FF before and 0x70FE afterward; retain Sheik 0x700F/0x7010 if verified ordinary text. If a candidate differs, present the actual text before substituting.
- [ ] Once validated, add these failing tests:

```c
REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_IMPA, &early) == 0x708D);
REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_IMPA, &complete) == 0x708E);
REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_ADULT_ZELDA, &early) == 0x70FF);
REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_ADULT_ZELDA, &complete) == 0x70FE);
REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_SHEIK, &early) == 0x700F);
REQUIRE(StaticStoryActor_SelectTextId(STATIC_STORY_ACTOR_SHEIK, &complete) == 0x7010);
```

- [ ] Run the actor test, observe old-selector failures, then change only these two switch arms:

```c
case STATIC_STORY_ACTOR_IMPA:
    return progression->metZelda ? 0x708E : 0x708D;
case STATIC_STORY_ACTOR_ADULT_ZELDA:
    return progression->metZelda ? 0x70FE : 0x70FF;
```

- [ ] Add a before/after mapping table for every existing actor and all 16 combinations of the four progression booleans. Only Phantom Ganon, Impa and Adult Zelda may differ in this commit. Test selectors with NULL progression too.
- [ ] Run the tests and runtime-repeat both progression branches; prove no song, reward, quest flag or source-actor follow-up action. Commit: "fix: select verified static Impa and Zelda dialogue".
- [ ] Prepare a single content review for Treasure Chest Shop Gal, Skull Kid, Keaton, HMS, Kafei and Lulu. Show exact proposed wording or verified reused messages, never raw MM IDs presented as OOT-compatible.
- [ ] Record cor's chosen text and explicit no-talk choices, if any. Before implementing new custom messages, inspect the existing custom-message manager and write a concrete appendix containing the collision-checked IDs, registration call site, languages/fallback, exact text, and tests. Do not silently make the new actors mute or give them Fairy text while calling dialogue complete.

## A5. MM donor and Skull Kid evidence gate

**Files to inspect:**

- soh/src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.c/.h
- soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c/.h
- soh/mods/transformation_masks/assets/mm_asset_loader.cpp/.h
- soh/mods/transformation_masks/assets/mm_display_list_patch.cpp/.h
- soh/soh/resource/type/Animation.h and PlayerAnimation.h
- scripts/diagnostics/audit_skull_kid.py and test_skull_cull_archive.py
- soh/tests/static_story_mm_actor_test.c and mm_display_list_patch_test.cpp

**Interfaces:** consumes privately supplied MM archive plus pinned donor source; produces evidence report and executable adapter appendices, not an enabled speculative actor.

- [ ] Obtain the already-authorized MM archive from an accessible user source. The current scratch uploads include logs, a music archive and the Market runtime export, not mm.o2r. Ask for it if unavailable; do not obtain a ROM from an unrelated source.
- [ ] Use a task-specific environment variable for its absolute path. Run the existing read-only audit and strict graph tests:

```bash
python3 -B scripts/diagnostics/audit_skull_kid.py "$MM_ACTOR_ARCHIVE"
bash scripts/diagnostics/run_stabilization_tests.sh "$MM_ACTOR_ARCHIVE"
```

The first tool intentionally accepts standard-limb NPC animation resources only. Its rejection of Kafei's player-style data is not evidence that Kafei's asset is corrupt.
- [ ] Pin the zeldaret/mm source revision and inspect object_kitan, object_osn, object_zov, object_test3 plus En_Kitan, En_Osn, En_Zov and En_Test3. Record exact resource paths, skeleton kind, limb/matrix counts, clip type/count, draw setup, face segment IDs, tracking limb indices and animation facial rules.
- [ ] Compare current Skull Kid strict graph behavior against the intended outstanding texture change. Record the precise missing resource/override behavior, if any. A clean structural audit is not proof of full texture rendering. Preserve same-archive nested DLs/vertices/textures, retained buffers, hash payload consumption, cull-list patching, and the native-fairy fallback.
- [ ] Validate the expanded parameter decoder: A/B/C are currently free; 9 belongs to HMS. Preserve all existing 0x7E and 0x7F mappings. Proposed appended identities are Keaton=20, child Kafei=21, Lulu=22; confirm no intervening source change has claimed them.
- [ ] Produce the following evidence-derived appendices before rendering changes. These are bounded planning outputs required by this gate, not permission for an unplanned subsystem rewrite.

### Ordinary-adapter appendix contract

Target files: static_story_actor.c/.h, static_story_mm_actor.c/.h, z_en_viewer.c/.h, existing actor/MM tests, and docs/static_story_actor_poc6_params.md.

Keep existing signatures:
```c
const StaticStoryMmPresentation* StaticStoryMm_GetPresentation(StaticStoryActorType type, uint8_t pose);
const StaticStoryPoseDescriptor* StaticStoryActor_ResolvePose(StaticStoryActorType type, uint8_t pose);
```

The appendix must contain exact descriptor rows for HMS, Keaton and Lulu, test rows for every parameter, verified per-pose facial bindings, resource type checks, and draw/init/destroy call sites. Do not pass player animations through an AnimationHeader cast.

Test code for Lulu's required policy, once its new enum exists:
```c
REQUIRE(StaticStoryActor_GetType(0x7E0C) == STATIC_STORY_ACTOR_LULU);
REQUIRE(StaticStoryActor_GetDefinition(STATIC_STORY_ACTOR_LULU)->maxPose == 3);
REQUIRE(StaticStoryActor_SanitizePose(STATIC_STORY_ACTOR_LULU, 15) == 0);
for (uint8_t pose = 0; pose < 4; ++pose) {
    const StaticStoryPoseDescriptor* p =
        StaticStoryActor_ResolvePose(STATIC_STORY_ACTOR_LULU, pose);
    REQUIRE(p != NULL);
    REQUIRE((p->flags & STATIC_POSE_FLAG_VOCAL) == 0);
    REQUIRE((p->flags & STATIC_POSE_FLAG_OCARINA) == 0);
    REQUIRE(StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_LULU, pose) != NULL);
}
```

Use the same explicit complete parameter tables for HMS 09/19/29, Keaton 0A/1A/2A, Kafei 0B/1B and Lulu 0C/1C/2C/3C when their implementation steps are written. Preserve invalid-pose fallback to pose 0. Enable HMS only with a real presentation; keep Ganondorf disabled.

### Kafei appendix contract

A dedicated local adapter may be added as static_story_kafei.c/.h beside the existing presentation helpers after feasibility is proven. Its exact types and lifecycle signatures must be derived from verified sampling/LOD support, not assumed interchangeable with SkelAnime_InitFlex.

Required evidence: exact two compatible clips, bounded frame data and frame count, joint/root/face metadata layout, retained archive resources, independent joint/morph/face state, and LOD display-list ownership. Trace init/update/draw/free and list every engine dependency. No live Player initialization, shared Link buffers, native schedule, singleton or Tatl/inventory pointer reuse.

The appendix must include failing tests for wrong resource type, missing/truncated frames, two instances advancing independently, loop boundaries, root placement and face indices, then minimal sampling/drawing implementation. If this requires porting the native player subsystem, stop and report that expansion.

### Skull Kid delta appendix contract

If the base already satisfies the intended texture work, retain it and report runtime evidence without a gratuitous loader rewrite. Otherwise provide a minimal reproduced failure, exact resource binding/lifetime fix, and red/green tests in mm_display_list_patch_test.cpp or a loader-level fixture that executes the real resolver. Preserve both existing poses and companion safeguards. Do not broaden global texture override behavior as an incidental change.

## Track completion

- [ ] Complete only evidence-backed appendices and their tests; record unmet gates explicitly.
- [ ] Update the user-facing parameter catalogue with verified IDs/poses.
- [ ] Run the release index's actor runtime matrix in vanilla and the user's Alt Assets stack.
- [ ] Hand over track commits, test commands/results, archive/source hashes and any remaining limitation for integrated verification. An evidence report alone is not completion of the actor expansion.
