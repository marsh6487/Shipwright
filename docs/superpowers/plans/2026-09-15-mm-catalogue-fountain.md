# MM Catalogue and Fountain Release Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver the approved static MM actor expansion, scoped dialogue cleanup, Skull Kid texture delta, and portable MM fountain material without losing proven fork fixes.

**Architecture:** Execute actor and material work as separate reviewable tracks sharing one pinned baseline and release checklist. Implement the source-grounded foundation first. Resolve the approved evidence gates before specifying or enabling adapters that depend on missing asset data.

**Tech Stack:** C, C++20, CMake/CTest, existing Python and shell diagnostic runners, archive-backed SoH resources, Prelude O2R exports.

**Spec:** ../specs/2026-09-15-mm-catalogue-fountain-design.md (approved by cor on 2026-09-15).

## Global Constraints

- Pinned base: 0a4dc0641c3a40925733bae2dafebe5f5b89e7de.
- Repository: marsh6487/Shipwright; source branch: probe/prelude-native-materials.
- Preserve existing actor placements, parameter values, collision, asset isolation, and native material profiles.
- Keep existing Hyrule Field night music and other unrelated fork fixes unchanged.
- Generalized scene night music is outside this bundle.
- Do not automatically merge other development branches.
- Keep authored actor/world Y unchanged; do not replace this with a world-position translation, change the scale, or apply the offset twice.
- Phantom Ganon: shapeYOffset = 1000.0f, scale = 0.01f, model-only lift = 10 world units.
- Lulu: four approved poses; singing is animation only, never an audio trigger.
- Happy Mask Salesman: complete the existing disabled entry, not a new identity.
- Four user fountain placements are distinct from the native actor's three material draw calls.
- Use the normal candidate build for runtime verification; do not require a separate diagnostics-only custom build.
- Missing game assets or a runnable environment are explicit verification limits, not passing results.

---

## Planning status and execution order

This is a staged implementation packet, not a claim that unresolved donor details have been solved. Actor Tasks A1–A3 are implementation-ready. A4 is ready once actual message contents are checked. A5 and Material Tasks M1–M2 are concrete evidence tasks. The remaining rendering implementation must receive an evidence-derived appendix before it is executed; inventing constants would violate the approved spec.

Read both this index and the relevant track:

1. [Actor foundation and compatibility plan](2026-09-15-mm-catalogue-actors.md).
2. [Fountain evidence and binding plan](2026-09-15-mm-fountain-material.md).

Recommended order: A1 → A2 → A3 → A4 → A5 and M1/M2 → evidence-derived adapter/profile appendices → ordinary MM actors → Kafei → remaining Skull Kid delta → fountain → integrated candidate verification.

The actor and fountain tracks can be reviewed independently. They need not be implemented concurrently; shared loader changes must be integrated serially. Do not delegate until the chosen execution workflow authorizes it.

## Baseline and workspace gate

**Files:** read AGENTS.md if present at execution time, docs/BUILDING.md, the approved spec, and both track plans.

**Interfaces:** consumes the pinned source commit; produces an isolated, identified working branch and baseline test results.

- [ ] Use the using-git-worktrees skill before implementation. Preserve unrelated local work. If the local checkout or network access is unavailable, report the blocker instead of reconstructing a partial project and claiming it builds.
- [ ] Check the actual checkout and ancestry:

```bash
git status --short
git rev-parse HEAD
git merge-base --is-ancestor 0a4dc0641c3a40925733bae2dafebe5f5b89e7de HEAD
git submodule status
```

- [ ] Review every source delta after the pinned base. The planning branch initially contains only the design/plans; do not assume another branch is equivalent.
- [ ] Run existing lightweight checks before edits:

```bash
bash scripts/diagnostics/run_stabilization_tests.sh
```

Record toolchain versions and complete output. Current actor assertions are partially disabled by the runner's -DNDEBUG; A1 repairs that coverage before it can be trusted.
- [ ] With initialized submodules and dependencies, configure a normal Debug build, not a diagnostics-only game variant:

```bash
cmake -S . -B build-mm-catalogue -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
```

Use the existing supported Windows generator instead when working on Windows, as documented in docs/BUILDING.md. A configure failure is a prerequisite failure, not a feature regression.
- [ ] Record read-only hashes of the user's test archives and saved placement coordinates. Never overwrite the user's master exports, textures, or saves.

## File ownership map

| Area | Existing integration points | New file responsibility |
| --- | --- | --- |
| Actor policy | static_story_actor.c/.h | No broad registry redesign |
| Runtime talk/init/draw | z_en_viewer.c/.h | Extracted-function test harness; narrow wiring only |
| Phantom presentation | static_story_ganon.c/.h | No offset change; extend existing test |
| Ordinary MM presentations | static_story_mm_actor.c/.h | Data and verified facial/draw contracts |
| Kafei | mm_asset_loader.cpp/.h, viewer dispatch | Dedicated presentation adapter after compatibility evidence |
| Skull Kid | mm_display_list_patch.cpp, strict graph loader | Only a demonstrated remaining texture delta |
| Fountain identity/math | NativeMaterialProfile.cpp/.h | Bounded donor-specific profile after evidence |
| Fountain runtime | PreludeNativeMaterialScroll.cpp | Owning-archive lookup and stable per-profile buffers |
| Verification | soh/tests, scripts/diagnostics | Existing harnesses plus focused tests |

Full paths are specified in the track plans. Do not unilaterally split or reformat the large viewer implementation.

## Spec coverage and release gates

| Approved requirement | Owner/checkpoint | Status before execution |
| --- | --- | --- |
| Preserve Phantom Ganon anchor and collision | A1–A3; runtime placement check | Source verified, tests not run this planning turn |
| Remove Phantom talk only | A2–A3 | Implementation-ready |
| Impa/Zelda messages; keep Sheik safe | A4 | Message-resource verification required |
| Gal/Skull Kid/new actor dialogue | A4 content checkpoint | Exact new lines require approval |
| Keaton/HMS/Lulu catalogue | A5, ordinary-adapter appendix | Donor contract verification required |
| Four Lulu poses, silent singing | A5 and release matrix | Roster fixed; clip looping/faces require validation |
| Kafei isolated player-style presentation | A5, Kafei appendix | Compatibility gate |
| Skull Kid missing texture work | A5 texture audit | Current base already contains texture patches |
| Portable exact-MM fountain | M1–M2, profile appendix | Donor mapping and constants gate |
| Existing material profiles unaffected | M1 and integrated test run | Existing tests available |
| Weather/audio/Navi and other actor fixes | Baseline + release regression matrix | No unrelated code changes authorized |

A gate means stop the dependent change only. Continue independent authorized tasks when safe. If a gate reveals a wider subsystem port, report the scope increase before implementing it.

## Integrated release verification task

**Files:** create docs/superpowers/reports/2026-09-15-mm-catalogue-fountain-validation.md during execution. Do not pre-fill it with passing claims.

**Interfaces:** consumes tested track commits and archive hashes; produces a candidate commit/build identifier and an honest validation report.

- [ ] Run the complete standalone stabilization suite after all source changes.
- [ ] Run the native material runner against both baseline and retextured exports with all donor fixtures required by M2.
- [ ] Build the normal candidate and CTest targets:

```bash
cmake --build build-mm-catalogue --target soh static_story_actor_test static_story_mm_actor_test static_story_ganon_test static_story_ruto_water_test mm_display_list_patch_test
ctest --test-dir build-mm-catalogue --output-on-failure
git diff --check
```

GenerateSohOtr may also be needed using the repository's ordinary asset generation workflow. Do not package copyrighted source archives in the git repository.
- [ ] Run the runtime matrix below in the normal candidate. Ask cor for runtime testing only where this environment cannot execute the game; provide candidate SHA, archive hashes, and a short route.
- [ ] Record pass/fail/not-run separately. Do not label an untested Alt Assets path fixed.
- [ ] Request code review using the required review skill, address findings, rerun affected tests, and preserve a reviewable commit per component.
- [ ] Use verification-before-completion and finishing-a-development-branch at handoff. Publication, merging, or building on a protected workflow must follow the user's authority and any platform approval requirements; no automatic Comboship merge.

### Runtime matrix

| Test | Expected observation |
| --- | --- |
| Existing Phantom placement, same authored XYZ | Same 10-unit rendered lift; collision unchanged; no talk prompt |
| Existing Impa, Zelda, Sheik before/after relevant progression | Intended ordinary text; no song/item/flag side effect |
| Skull Kid both poses, multiple instances, re-entry | Correct intended textures, stable companion, no stale graph crash |
| HMS/Keaton every pose | Correct silhouette/materials; no donor gameplay |
| Lulu all four poses | Correct face segments and loops; singing leaves music unchanged |
| Two Kafeis plus Link | Independent animation/face state; no live-player/schedule mutation |
| Existing Saria/Malon/Kokiri/Fairy/Ruto/Darunia | Baseline pose, eye, tracking, root and water behavior preserved |
| Four fountain placements, day/night exports | Correct matched material animation; no mistaken placement count |
| Fountain retexture and fresh copied export | Same verified native movement without UUID/scene-specific binding |
| Lake/Pool/Lost Woods sheet together | Their existing scroll behavior unchanged |
| Adjacent geometry and repeated scene entry | No material/segment leak or lifetime failure |
| HF dawn/dusk, enemy handoff, rain cycles, Navi in busy hub | No regression relative to baseline; no new weather/audio feature |

## Execution handoff

Choose subagent-driven execution with review between tasks, or inline execution with checkpoints. This turn creates planning documents only. Both paths start with baseline verification and A1; neither bypasses missing-asset or user-dialogue gates.
