# Stable Integrated Baseline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a preserved, reproducible personal-fork baseline containing catalogue actors, weather/audio behavior, scene-edit compatibility, custom player support, and NEI, without promoting unresolved crashes.

**Architecture:** Repair and verify individual defects on topic branches, assemble them on an integration candidate, then promote the exact tested candidate to the stable line. Preserve existing NEI work and historical builds. Treat runtime evidence, build success, and metadata unit tests as different gates.

**Tech Stack:** Existing Shipwright C23/C++20 sources, CMake 3.26 or newer, CTest, GitHub Actions, and Windows runtime validation.

**Spec:** `docs/superpowers/specs/2026-09-10-stable-integrated-baseline-design.md` (approved in chat).

## Global Constraints

- No unrelated feature expansion is part of this promotion cycle.
- A feature being present on an existing integration branch does not automatically qualify it for the stable baseline.
- The existing integration branches are source material, not automatically the stable baseline.
- No exploratory fixes are made directly on the stable line.
- Logs from promotion testing should be retained so a future regression can be compared against a known-good run.
- Do not stack an unverified fix onto an already-red integration candidate.
- Do not remove scene placements, disable requested features, or change the user's scene archives to hide a code defect.
- Preserve the already-started NEI integration; do not restart it, duplicate it, reset its worktree, or overwrite its branch.
- Any exclusion from the agreed feature set requires the user's explicit acceptance. None is presumed here.
- Do not publish the user's ROM-derived OoT/MM archives, saves, full configuration, or personal filesystem paths with release evidence.

## Scope and Execution Order

The approved design spans independent subsystems. This document is the coordination and first-delivery plan, not a fabricated source-level fix for every outstanding bug. Execute the renderer readiness work below first. Weather/audio, remaining catalogue presentation, and NEI each receive a separate focused implementation plan after their exact current source and existing work are read. Those plans must implement the acceptance gates below; they must not reopen the approved architecture or add new features.

Order: renderer blocker -> remaining catalogue/custom-player checks -> weather/audio repairs -> preserved NEI integration -> complete candidate validation -> stable promotion.

Read-only inventory and preservation of NEI work can happen immediately. Integration of NEI into the release candidate waits for the pre-NEI gates. Do not cancel another session's builds or edit its working tree.

## Verified Starting State and Evidence Limits

At the planning audit, `codex/fix-static-actor-render-state` pointed to `46c909e8e179665d9981cde99e199eec18151f8b`, four commits beyond `9c90323960b6d54c6c4e2ebdf5605bf8101d46d9`.

The topic branch changes the actor registry/header, adds `static_story_segment_contract_test.c`, and adds the design document. It does NOT yet change `z_en_viewer.c`. The registry change also collapses substantial existing source into single lines; remove that unrelated formatting churn before presenting a narrow repair.

The new test checks object-ID selection. It is not registered in `soh/CMakeLists.txt` and does not execute actor drawing. It is not a reproduction of the reported crash.

Most importantly, `Actor_Draw` in `soh/src/code/z_actor.c` already calls `Actor_SetObjectDependency` and emits segment `0x06` bindings to BOTH opaque and translucent streams before calling `actor->draw`. Therefore, simply repeating a model-segment bind inside `EnViewer_Draw` is not an established fix. Check actual object-slot validity, address conversion, display-list interpretation, and resource ownership before changing this boundary.

The earlier successful `1458e81` run used a reduced mod stack. Preserve it as a successful control configuration, not proof that its executable is universally stable or that the exact introducing commit has been identified. The intermittent failures and the `0x08000001` register value do not alone prove an age-cache flip, dangling pointer, or segment-6 defect. An actor's first-draw log is not necessarily the failing renderer command.

Sources audited: the pinned branch comparison, `soh/CMakeLists.txt`, `soh/src/code/z_actor.c` around `Actor_Draw`, the registry/header/test, and `.github/workflows/generate-builds.yml` plus `pr-artifacts.yml`.

## File Responsibilities

- `soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c` and `.h`: catalogue metadata; preserve all existing entries, flags, and behavior.
- `soh/tests/static_story_segment_contract_test.c`: supplementary model/animation object-ID contract, not runtime crash coverage.
- `soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c`: catalogue initialization/update/draw path; change only where a traced defect warrants it.
- `soh/src/code/z_actor.c`: inspect the caller's existing object dependency and render-stream setup before adding redundant setup.
- `soh/src/code/z_skelanime.c`: inspect the skeleton draw route and resource/skeleton replacement behavior when tracing the fault.
- `soh/soh/z_play_otr.cpp`: existing `ActorCatalogue_LogLifecycle` logging boundary; reuse its logging approach if targeted instrumentation is required.
- `soh/CMakeLists.txt`: register focused tests.
- `.github/workflows/generate-builds.yml`: existing `build-linux`, `build-windows`, and `generate-soh-otr` jobs; add actual test execution before packaging.
- `.github/workflows/pr-artifacts.yml`: existing artifact-link publication; do not replace it merely because earlier artifact discovery was incomplete.
- `docs/debug/2026-09-10-room8-renderer-evidence.md`: create during execution to separate observations, a single active hypothesis, test outcomes, and remaining unknowns.
- `docs/debug/2026-09-10-stable-integrated-validation.md`: create during execution for the candidate's exact build identity and acceptance results.

## Task 1: Preserve the Work and Establish a Reproducible Candidate Identity

**Files:** Create the two evidence documents above. Read existing worktree state, branch refs, and build metadata without changing NEI work.

**Interfaces:** Consumes the pinned topic ref and existing test logs. Produces a source/build/configuration record for every subsequent test.

- [ ] Inspect the execution workspace before creating another checkout:

```sh
git status --short
git branch --show-current
git worktree list --porcelain
git log -5 --oneline
git submodule status --recursive
```

If no Git checkout is available, report that fact and use the connected GitHub read/write tools plus Actions. Do not claim that a prior Codex worktree is available here. Do not overwrite a dirty checkout.

- [ ] Record full source SHA, relevant PR/run IDs, actual checkout SHA (including PR merge SHA), submodule SHAs, executable hash, generated `soh.o2r` hash, graphics backend, save identity, and a local mod/configuration manifest. An artifact containing only `soh.o2r` is not a new executable.
- [ ] Keep the successful control folder unchanged. Use a separate test folder and a copied save. Freeze the failing configuration as well; do not change age, model, mods, and executable simultaneously.
- [ ] Locate the existing NEI branch/worktree and record its exact head and any uncommitted work read-only. A missing matching remote branch is not proof that the local integration has been lost.
- [ ] Commit only sanitized evidence documentation. Do not commit the user's runtime data or claim unperformed tests passed.

**Acceptance:** Every observed success/failure is tied to an executable and configuration; no in-progress integration is altered.

## Task 2: Make the Partial Patch Small and Its Supplementary Test Runnable

**Files:** Modify `static_story_actor.c`, `static_story_actor.h`, `soh/CMakeLists.txt`; test with the existing catalogue suite and new segment-contract test.

**Interfaces:** Preserve all existing registry APIs. The existing new API is `int16_t StaticStoryActor_GetDrawObjectId(StaticStoryActorType type)`. Keeping this metadata helper is conditional on an actual consumer; it must not be advertised as the renderer repair.

- [ ] On the isolated topic checkout, inspect the full diff:

```sh
git diff 9c90323960b6d54c6c4e2ebdf5605bf8101d46d9 -- \
  soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c \
  soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.h \
  soh/tests/static_story_segment_contract_test.c \
  soh/CMakeLists.txt
```

- [ ] Configure with `-DBUILD_TESTING=ON` using the existing platform setup. Attempt to build `static_story_segment_contract_test` before registration and retain the expected missing-target failure. This proves the test wiring gap, NOT the runtime crash.
- [ ] Restore original formatting and comments from the pinned base in the registry/header, retaining only the explicit helper declaration/definition if retained. The helper body is:

```c
int16_t StaticStoryActor_GetDrawObjectId(StaticStoryActorType type) {
    const StaticStoryActorDefinition* definition = StaticStoryActor_GetDefinition(type);

    return definition != NULL ? definition->objectId : OBJECT_INVALID;
}
```

Do not change actor availability, parameter mapping, tracking, root-lock flags, animations, dialogue, or poses in this cleanup.

- [ ] Add this block within the existing `if(BUILD_TESTING)` section, beside `static_story_actor_test`:

```cmake
add_executable(static_story_segment_contract_test
    tests/static_story_segment_contract_test.c
    src/overlays/actors/ovl_En_Viewer/static_story_actor.c
)
target_include_directories(static_story_segment_contract_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
add_test(NAME static_story_segment_contract_test COMMAND static_story_segment_contract_test)
```

- [ ] Build all five focused targets and run CTest from the `soh` build subdirectory, where the existing tests are registered:

```sh
cmake --build build-cmake --config Release --target \
  static_story_actor_test static_story_segment_contract_test \
  static_story_ruto_water_test static_story_mm_actor_test static_story_ganon_test
ctest --test-dir build-cmake/soh -C Release --output-on-failure --no-tests=error \
  -R '^static_story_(actor|segment_contract|ruto_water|mm_actor|ganon)_test$'
```

Use `build-windows` in place of `build-cmake` for the existing Windows job. Require all five named tests to be discovered and executed. Keep the existing `if(NOT WIN32)` guard around `libm`; do not introduce `m.lib` on Windows.

- [ ] Review the resulting narrow diff and commit only the registry/test-wiring cleanup. Report precisely which tests passed; none establishes Room 8 safety.

**Acceptance:** No unrelated source rewrite; five focused tests actually execute; no runtime-fix claim.

## Task 3: Repair the Actual Failing Draw/Resource Boundary

**Files:** Inspect `z_actor.c`, `z_skelanime.c`, `z_en_viewer.c`, and the pinned renderer submodule. Record findings in `2026-09-10-room8-renderer-evidence.md`. Production edits are limited to the boundary established by the trace.

**Interfaces:** Consumes a preserved failing build/configuration and the caller's actual draw contract. Produces one fault-specific regression test and one minimal repair, or a clearly labeled diagnostic build when the fault cannot yet be reproduced with available assets.

- [ ] Trace `Actor_Draw -> EnViewer_Draw -> EnViewerStatic_Draw -> Malon skeleton/accessory draw`, including resource overrides and deferred interpreter execution. Inspect `Actor_SetObjectDependency`, object-slot checks, and both opaque/translucent segment commands.
- [ ] Inspect how the actual pinned renderer represents segmented/tagged addresses before interpreting `0x08000001` as a native pointer. Do not infer a function from unsymbolized stack labels.
- [ ] Use the existing logs first. If the failing command is not identifiable, add bounded instrumentation that records the scene/room, actor parameters, model/animation slots and object IDs, resolved resource path/source archive, active renderer segment binding, and the failing Gfx command/opcode. A last-draw marker or a repeated equipment-cache count alone is insufficient. Preserve the first fault before opcode spam obscures it.
- [ ] Write down one hypothesis with its falsifier. Example: if a supposedly missing segment is already correct immediately before the failing command, reject that hypothesis rather than adding another bind.
- [ ] Create the smallest failing regression around the observed production path. Exercise the actual segment/address conversion, command emission, or resource lifetime involved; do not replace it with another registry-getter test. Run it against unpatched source and retain the failure. If proprietary scene/mod assets prevent an automated reproduction, retain the exact in-game reproduction and mark that portion runtime-only.
- [ ] Apply only the change supported by that failing test/trace. Do not simultaneously modify PakLoader, MM loading, actor allocation, scene data, and weather. Re-run the same reproduction and the focused suite.
- [ ] Revert the candidate change if the predicted fault persists. Keep the trace and update the hypothesis; do not accumulate speculative guards or suppress invalid-opcode warnings as a cure.

This task intentionally does not prescribe an unverified segment-6 patch. The source-level repair must follow the actual failing instruction/resource; the current evidence does not justify inventing that code in a plan.

**Acceptance:** A trace-supported repair, a failing-before/passing-after reproduction, and no loss of required actor/model behavior. If only instrumentation is complete, label the artifact diagnostic and keep the blocker open.

## Task 4: Build and Deliver One Clearly Identified Windows Test Package

**Files:** Modify the existing build workflow only as needed for explicit tests and build identity; update the evidence documents.

**Interfaces:** Consumes the topic repair commit. Produces Linux/Windows job results plus a Windows package containing its matching executable, generated `soh.o2r`, and build identity.

- [ ] In `build-linux` and `build-windows`, explicitly enable `BUILD_TESTING` and insert the Task 2 CTest invocation after compilation and BEFORE CPack/upload. Do not accept 'zero tests found'. Keep dependencies and platform setup from the existing workflow.
- [ ] Include the actual checkout SHA and submodule revisions in the package manifest. Record SHA-256 hashes of the executable and generated `soh.o2r`. Retain matching Windows symbols where available for crash symbolization.
- [ ] Commit/push one reviewed batch rather than many tiny transport writes that repeatedly cancel the same workflow. Verify the resulting run's actual head/checkout SHA and each relevant job conclusion.
- [ ] Inspect the artifact list for the correct run. The existing names include `soh-windows`, `soh-linux`, and `soh.o2r`. Never assume a cancelled run has no artifacts; never treat a lone resource artifact as a patched executable.
- [ ] Give the user a direct, verified download link with a readable test-build label. Explain exactly which two build files belong together. Do not ask the user to navigate raw commit hashes.

**Acceptance:** Both platform builds and focused tests pass, the Windows package is actually downloadable, and its contents are identified. This is still a candidate, not stable.

## Task 5: Runtime Gate for the Room 8 Repair

**Files:** Update `2026-09-10-room8-renderer-evidence.md` with results; retain full logs outside public source control.

**Interfaces:** Consumes the unchanged failing mod/configuration stack and identified test package. Produces recorded runtime evidence for promotion.

- [ ] Test Sacred Forest Meadow -> Lost Woods Room 8 -> Meadow -> Room 8 with default Adult Link. Repeat five cycles after each of two cold starts.
- [ ] Repeat the same gate with Malon Rose as the selected adult player model; keep basket Malon `0x7F1A` present. Retain the same gameplay age, save, scene archive, mod load order, and equipment configuration.
- [ ] Test the Kokiri-side approach and a direct debug-menu spawn separately, so a successful route does not conceal a failing one.
- [ ] Check the Child Malon `0x7F02` reproduction and the Child Link control in a test copy without rewriting the master scene archive.
- [ ] Confirm Malon, her basket, blinking, animation, and Link's equipment still render, rather than accepting a crash avoided by silently skipping a draw.
- [ ] Treat no crash as necessary but not sufficient: investigate new invalid resource names/opcodes, missing geometry, and transition corruption. Existing opcode spam is an unresolved issue, not automatically harmless merely because a run survived.

Counts above are operational acceptance thresholds, not mathematical proof that an intermittent defect can never recur. Minimal-stack success alone does not qualify the intended larger stack.

**Acceptance:** The actual failing route/configuration passes repeatedly and no known unresolved render-corruption condition is promoted. Record any remaining condition as blocked, not passed.

## Remaining Independent Work Packages and Acceptance Gates

These are separate implementation packages, not permission to bundle fixes into the renderer patch.

| Package | Required result before integration |
| --- | --- |
| Catalogue/MM and custom player support | Bomb Shop Lady variants and both Skull Kid variants render/animate through unload and re-entry; no cross-actor corruption; custom-model on/off switching remains sound. Preserve each actor's actual available status rather than assuming reserved catalogue entries are implemented. |
| Catalogue presentation | Correct Zant anchor; Ruto knee axis and intended fade without placement/root-lock regressions; Great Fairy appearance/blinking where the asset supports it; retain prior working catalogue actors. The precise fade curve is not established by this spec and must be recovered from the earlier approved work or confirmed, not invented. |
| Weather ownership/timing | Lost Woods rooms 4/7 do not spuriously activate intermittent rain when disabled. Preserve deliberately placed/native weather ownership. Use the user's stated approximately five-second activation and 8-14-second phases as the timing target, then verify the exact intended dry/rain semantics before changing constants. Test leave/re-entry and toggling. |
| Music | Hyrule Field day music does not persist incorrectly from dusk into night; normal BGM/SFX and rain audio coexist. Test repeated transitions, including with weather enabled and disabled. |
| Scene compatibility | Preserve the actual edited scene assets and verified asset-only repairs. Test Lost Woods, the edited Market after reload, and previously affected geometry; don't replace these with unrelated stock scenes for release qualification. |
| NEI | Resume the existing integration, pin its upstream/input commits, review the merge delta, pass Linux/Windows tests, and smoke-test its intended item/gameplay paths alongside the catalogue/weather stack. Do not import unrelated upstream changes just because a newer branch exists. |

For each package: read the current source and previous accepted patch, make a separate focused plan, run failing-before/passing-after tests for the reproduced regression, retain a rollback point, and only then integrate. Never silently mark an unavailable test or an excluded feature as passing.

## Task 6: Promote and Preserve the Exact Tested Integrated Master

**Files:** Complete `2026-09-10-stable-integrated-validation.md`; create the stable ref/tag and preserved release package only after all gates pass.

**Interfaces:** Consumes accepted topic commits and preserved NEI integration. Produces the single stable baseline used for subsequent work.

- [ ] Assemble accepted changes on a fresh integration candidate without resetting or renaming existing source branches. Document each input commit and regression it resolves.
- [ ] Re-run Linux build/tests, Windows build/tests, Room 8 checks, catalogue/MM/custom-player checks, scene reloads, weather timing/ownership, night music, and NEI smoke tests on the COMPLETE combined candidate. Topic-branch success does not transfer automatically through a merge.
- [ ] If a gate fails, freeze promotion, isolate the responsible change, and repair/revert it on its topic branch. Do not add unrelated features while the candidate is red.
- [ ] Review the candidate diff for removed catalogue entries, reverted weather controls, unintended dependency/submodule upgrades, and unrelated scene changes.
- [ ] Obtain the user's acceptance of the tested candidate's runtime results. Create the stable pointer and an immutable, readable release tag referencing the exact tested commit. A new merge commit or rebuild requires validation of that new output before calling it stable.
- [ ] Preserve the tested Windows/Linux packages, hashes, build metadata, sanitized results, and rollback commit. Do not rely on temporary Actions artifacts as the only retained copy.
- [ ] Treat branch protection as a policy until actual GitHub rules are configured and verified; do not claim technical enforcement from a branch name alone.

**Definition of done:** The agreed integrated feature set is present; every required gate is recorded as passed or explicitly accepted by the user as an exclusion; the exact tested source/build is preserved and designated stable. No stable baseline, completed fix, or passing runtime result is claimed merely by completing this plan.

## Execution Handoff

Use inline execution with checkpoints in this conversation when no coding subagent tool is available. Use the existing connected GitHub tools and Actions for builds when the local container cannot access the repository network. First implementation batch: preserve identities, make the partial registry change reviewable, wire/run its supplementary test, and trace the actual draw/resource boundary. Do not merge or promote during that batch.
