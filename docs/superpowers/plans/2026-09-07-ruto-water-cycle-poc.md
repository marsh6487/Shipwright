# Adult Ruto Water Cycle POC Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add one opt-in static Adult Ruto swimmer pose that reuses Adult Ruto's own swimming animation with En_Zo-inspired water-aware surface/tread/dive behavior, without modifying native En_Ru2 or any weather/audio code.

**Architecture:** Keep the existing `0x7Fxx` static actor encoding unchanged for this POC. Adult Ruto pose 3 is marked with a water-cycle flag. Pure policy helpers in `static_story_actor.{c,h}` own the state-transition decisions, while `En_Viewer` translates actor/water/player state into movement and animation changes. Prelude `home.pos` remains the submerged return point; the active waterbox determines the surface dynamically through the normal actor BG-water query.

**Tech Stack:** Shipwright C/C++, En_Viewer static-story actor layer, Actor background checks, SkelAnime, CMake/CTest, GitHub Actions.

**Spec:** This plan is the POC spec.

## Global Constraints

- Do not modify native `ovl_En_Ru2` behavior.
- Do not modify `ovl_En_Zo`; borrow policy only.
- Do not touch weather, rain, thunder, Hyrule Field music, or Audio Editor files.
- Existing Adult Ruto poses 0-2 and all existing `0x7Fxx` placements must retain their current meaning.
- Pose 3 is the only water-cycle entry point.
- Use the real waterbox surface, not a hard-coded +300 Y target.
- No cutscene, switch flag, kill-self, medallion, or native Ruto dialogue state-machine calls.
- No pathfinding or horizontal patrol behavior in this POC.

---

### Task 1: Water-cycle policy and pose registration

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c`
- Test: `soh/tests/static_story_actor_test.c`

**Interfaces:**
- Produces: `STATIC_POSE_FLAG_WATER_CYCLE`, `STATIC_ANIM_ADULT_RUTO_SWIM_UP`, `StaticStoryWaterCycleState`, and `StaticStoryActor_WaterCycleNextState(...)`.

- [ ] **Step 1: Write failing tests** asserting Adult Ruto pose 3 resolves to the swim-up animation with the water-cycle/no-tracking flags and that the pure state policy performs `SUBMERGED -> SURFACING -> TREADING -> DIVING -> SUBMERGED` under the expected conditions.
- [ ] **Step 2: Run `static_story_actor_test` and verify failure** because the new pose/enum/helper do not exist.
- [ ] **Step 3: Add the minimal registry and policy implementation**. Preserve poses 0-2 unchanged.
- [ ] **Step 4: Run `static_story_actor_test` and verify pass**.

### Task 2: Runtime water-cycle adapter

**Files:**
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.h`
- Modify: `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`

**Interfaces:**
- Consumes: the Task 1 water-cycle pose flag/state policy.
- Produces: isolated runtime state in `EnViewerStaticState` for water state, dive timer, and movement.

- [ ] **Step 1: Add runtime fields** for water-cycle state and dive timer; initialize them only for water-cycle poses.
- [ ] **Step 2: Map `STATIC_ANIM_ADULT_RUTO_SWIM_UP` to `gAdultRutoSwimmingUpAnim`** and continue using Adult Ruto's existing skeleton/draw adapter.
- [ ] **Step 3: Add a water-cycle update path** that uses the normal actor BG-water query to refresh water depth/surface data, applies vertical motion patterned after `En_Zo`, and never calls native En_Ru2/En_Zo action functions.
- [ ] **Step 4: Use Prelude `home.pos` as the submerged return point**. On surfacing, rise toward the actual water surface; while treading, bob around the surface; on dive completion, return cleanly to the submerged home point.
- [ ] **Step 5: Gate talk/tracking while submerged/surfacing/diving** and allow ordinary static Adult Ruto interaction only while treading at the surface.

### Task 3: Scope and build verification

**Files:**
- No additional production files.

- [ ] **Step 1: Run focused static-story actor tests**.
- [ ] **Step 2: Run `git diff --check` or equivalent whitespace validation**.
- [ ] **Step 3: Inspect the final diff and verify no files outside the static En_Viewer actor layer, its focused test, and this plan changed**.
- [ ] **Step 4: Push the POC branch and open a draft PR against `codex/poc5-stabilization`** with a runtime acceptance checklist for Work.
