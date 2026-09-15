# Portable MM Fountain Material Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish the exact fountain donor mapping and then implement only its verified native material behavior through the existing portable binding system.

**Architecture:** Reuse the owning-archive profile resolver, safe command insertion and retained runtime command buffers. Perform donor extraction before choosing a one-layer/two-layer or animated-color implementation; those are materially different contracts and must not be guessed.

**Tech Stack:** C++20, nlohmann/json, existing Python read-only export probe, native GBI command generation, archive resource metadata.

**Spec:** ../specs/2026-09-15-mm-catalogue-fountain-design.md.

## Global Constraints

- Pinned base: 0a4dc0641c3a40925733bae2dafebe5f5b89e7de.
- Preserve existing actor placements, parameter values, collision, asset isolation, and native material profiles.
- Keep the existing Lake Hylia, Kakariko pool, and Lost Woods sheet profiles behaviorally unchanged.
- Generalized scene night music is outside this bundle.
- Do not automatically merge other development branches.
- Four user fountain placements are not the native actor's three material draw calls.
- Use the normal candidate build for runtime verification; do not require a separate diagnostics-only custom build.
- Missing game assets or a runnable environment are explicit verification limits, not passing results.

---

M1 and M2 are executable evidence tasks. M3 is the explicit implementation-readiness gate: it produces a concrete profile appendix only after M2 determines the correct material contract. This plan does not claim exact-MM constants are already known.

## File map

| Path | Responsibility |
| --- | --- |
| soh/soh/Enhancements/Graphics/NativeMaterialProfile.h | Existing profile enum and parameter API |
| soh/soh/Enhancements/Graphics/NativeMaterialProfile.cpp | Provenance recognition, command validation and scroll math |
| soh/soh/Enhancements/Graphics/PreludeNativeMaterialScroll.cpp | Owning-archive lookup, injected calls, stable buffers and update |
| soh/soh/Enhancements/Graphics/PreludeNativeMaterialScroll.h | Existing runtime bridge |
| soh/src/code/z_rcp.c | Reference native command generators; preserve existing functions |
| soh/tests/native_material_scroll_test.cpp | Identity/math/safety tests |
| soh/tests/native_material_runtime_test.cpp | Production bridge buffer and generated-command tests |
| soh/tests/native_material_export_probe.cpp | Read-only real-export classification |
| scripts/diagnostics/run_native_material_probe.py | Existing compiler and archive-fixture runner |
| scripts/diagnostics/report_mm_fountain_provenance.py | New read-only evidence extractor defined below |
| docs/superpowers/reports/2026-09-15-mm-fountain-evidence.md | Resource hashes, donor mapping and verified constants |

## M1. Establish native-material regression baseline

**Files:** existing profile/bridge/tests and runner listed above.
**Interfaces:** consumes initialized libultraship, C++20 compiler, nlohmann/json and spdlog include roots, baseline and retextured O2R exports. Produces CPU test results and archive classifications; no game patch.

- [ ] Review ResolveNativeMaterial, FindNativeScrollInsertion, NativeScrollParameters, ProfileFor, the resource factory and runtime Update together. Do not alter fixed buffer counts before enumerating every index consumer.
- [ ] Locate installed headers with read-only checks. Define JSON_INCLUDE_ROOT and SPDLOG_INCLUDE_ROOT as the directories containing nlohmann/json.hpp and spdlog/spdlog.h respectively. Do not install dependencies or bypass network restrictions as an incidental planning action.
- [ ] Run the existing probe with no archives first:

```bash
python3 -B scripts/diagnostics/run_native_material_probe.py --json-include "${JSON_INCLUDE_ROOT:?set the existing JSON include root}" --spdlog-include "${SPDLOG_INCLUDE_ROOT:?set the existing spdlog include root}"
```

Expected: profile identity/command safety/math checks and production runtime command/lifetime checks pass. The runner uses GNU-style linker options; run it on a compatible environment, not unchanged under MSVC.
- [ ] Run against the supplied exports using explicit validated absolute paths assigned to task-specific variables:

```bash
python3 -B scripts/diagnostics/run_native_material_probe.py --json-include "$JSON_INCLUDE_ROOT" --spdlog-include "$SPDLOG_INCLUDE_ROOT" "${FOUNTAIN_BASE_EXPORT:?set the existing baseline export path}" "${FOUNTAIN_RETEX_EXPORT:?set the existing retextured export path}"
```

Do not pass the same file under two names and call retexture coverage complete. If only one export exists, record the missing variant.
- [ ] Preserve baseline output for Lake, Pool and Lost Woods sheet at frame values 0, 1, 127, 128, 2047, 2048 and UINT32_MAX. Existing runtime checks intentionally use distinct state/gameplay clocks.
- [ ] Record any baseline failure before proceeding. No production material change belongs in M1.

## M2. Resolve donor identity and material constants

**Files:** create report_mm_fountain_provenance.py and the evidence report.
**Interfaces:** the script accepts one O2R path and writes a JSON report to stdout. It does not modify the export. It produces candidate material rows, not a claim that each row is a fountain placement.

- [ ] Create this read-only script with apply_patch:

```python
import hashlib
import json
from pathlib import Path
import sys
import zipfile

LABELS = {"Z2_00KEIKOKUTex_031B98", "Z2_00KEIKOKUTex_02A850"}

def report(path):
    path = Path(path)
    with path.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    result = {"archive_sha256": digest, "materials": []}
    with zipfile.ZipFile(path) as archive:
        project = json.loads(archive.read("prelude/project/edits.json"))
        for scene, edits in project["edits"].items():
            if not isinstance(edits, list):
                continue
            for edit_index, edit in enumerate(edits):
                for kind in ("pastes", "shapes"):
                    for index, item in enumerate(edit.get("data", {}).get(kind, [])):
                        textures = item.get("stored", {}).get("textures", [])
                        labels = {t.get("label") for t in textures if isinstance(t, dict)}
                        if not labels.intersection(LABELS):
                            continue
                        dl_path = item.get("newDlPath")
                        dl = archive.read(dl_path) if dl_path in archive.namelist() else None
                        result["materials"].append({
                            "scene": scene, "edit": edit_index, "kind": kind,
                            "item_index": index, "newDlPath": dl_path,
                            "placed": item.get("placed"),
                            "chain": item.get("chain"),
                            "textures": [
                                {k: t.get(k) for k in ("label", "w", "h")}
                                for t in textures if isinstance(t, dict)],
                            "texPaths": item.get("stored", {}).get("texPaths"),
                            "display_list_sha256":
                                hashlib.sha256(dl).hexdigest() if dl is not None else None,
                        })
    return result

if __name__ == "__main__":
    print(json.dumps(report(sys.argv[1]), indent=2))
```

The runtime label search is only an audit filter. Do not reuse this label-only matching algorithm as production material identity.
- [ ] Run the script against each provided export:

```bash
python3 -B scripts/diagnostics/report_mm_fountain_provenance.py "$FOUNTAIN_BASE_EXPORT"
python3 -B scripts/diagnostics/report_mm_fountain_provenance.py "$FOUNTAIN_RETEX_EXPORT"
```

Use the existing runner's fixture(path) to inspect the candidate display-list command streams. Distinguish command payloads from opcodes; do not scan raw words for apparent segment commands.
- [ ] Map every candidate material row back to its placement using recipe/transform/scene data. Check the user's four recipe references; repeated material portions and day/night representations must not be counted as extra or missing fountains.
- [ ] Pin the MM donor source revision. Read Bg_Keikoku_Spr draw logic and object_keikoku_obj material data, then separately inspect scene-owned Z2_00KEIKOKU geometry/material definitions associated with the candidate labels.
- [ ] Prove a geometry/material match. The actor's three AnimatedMat_Draw calls are leads, not proof the imported static scene recipes use those three animated materials.
- [ ] Read the actual matching animation resources from the available MM archive or a verified source definition. Record each layer's segment/tile, signed S/T speed, width/height, phase/modulus, frame clock, interpolation behavior, and any animated color or texture switching. Record native resource paths, hashes and source revision. Header offsets such as 0001F8/0003F8/0005F8 alone do not satisfy this step.
- [ ] If the exported part has no native animated material, report that finding and identify which donor surface would need to be included for an exact port. Do not silently assign movement to decorative geometry.
- [ ] If donor resources are missing, stop this track and request the relevant archive/data. Keep M1 and actor work available independently.
- [ ] Commit only the read-only script and evidence report: "docs: establish MM fountain donor provenance". Do not commit game resources.

## M3. Produce the evidence-derived profile implementation appendix

**Files:** append an executable task sequence to this plan once M2 succeeds, using the profile/bridge/test paths above.
**Interfaces:** retain the current public entry points unless verified donor structure requires a narrowly documented extension:

```cpp
NativeMaterialProfile ResolveNativeMaterial(const nlohmann::json& item, bool pasted);
std::optional<size_t> FindNativeScrollInsertion(
    const std::vector<NativeMaterialCommand>& commands);
ScrollParameters NativeScrollParameters(
    NativeMaterialProfile profile, uint32_t stateFrames, uint32_t gameplayFrames);
```

The namespace is Prelude. These are existing signatures, not proposed new APIs.

- [ ] Choose the smallest verified representation. If the donor matches two equal-size layers, extend the existing enum and parameter switch. If it requires one layer, unequal dimensions, multiple material roles or animation beyond scrolling, explicitly define the corresponding bounded descriptor and validation interface before coding.
- [ ] Include literal verified donor constants and complete test fixtures in the appendix. Do not add placeholder speeds or reuse Lake values as stand-ins. Specify frame wrap arithmetic with unsigned intermediate operations where needed to avoid signed overflow.
- [ ] Define how matching survives a fresh copy, retexture, destination scene change and generated path change. Use stable verified donor provenance plus material command shape, not the current user's UUID, recipe label or one hard-coded scene.
- [ ] If the available export does not retain sufficient provenance after retexture, document the exact missing field and stop for a scoped design decision. Do not promise portability based on a one-export texture-name match.
- [ ] Add red/green tests for identity, rejected ambiguity, wrong material role, malformed/truncated commands, payload handling and the exact native clock/layer math. Each code task must include the complete new fixture and minimal implementation.
- [ ] Preserve the original two-tile safety checks for existing profiles. New valid single-layer input, if required, gets its own verified path; arbitrary one-tile display lists do not become eligible.
- [ ] Specify every runtime buffer/index change. Keep previously injected pointers stable for their required lifetime; a resizing vector that invalidates old pointers is unacceptable.
- [ ] Extend native_material_runtime_test.cpp to compare the production bridge's output with the actual donor-equivalent native generator, including phase boundaries and recycled transient allocations. Preserve the existing 12-command comparisons for old profiles.
- [ ] Update run_native_material_probe.py only if a new generator must be extracted. Its extraction must fail loudly if source structure changes. Do not replace the production generator with a test reimplementation.
- [ ] Include exact-path commits per independently tested change: recognition, parameter/command generation, and runtime binding if they can be reviewed separately. Run M1's baseline checks after each.

### Required negative and portability fixtures

| Input difference | Required result |
| --- | --- |
| Same donor material, new scene/path/recipe UUID | Same fountain profile |
| Same donor material, 4K replacement artwork | Same native animation, verified native tile dimensions |
| “Fountain” label on unrelated geometry | Unpatched |
| Same texture reused on a decorative portion | Unpatched unless its own donor animation is proven |
| Conflicting profile claims for one generated DL | Unpatched with diagnostic |
| Nested/control-flow/segment mutation outside supported contract | Rejected, not partially patched |
| Hash payload resembling G_DL or end marker | Treated as data |
| Missing/truncated DL or unsupported microcode | Safe unpatched result |
| Same generated path in different owning archives | Each uses only its owner's provenance |
| Frame reset, disable/re-enable, scene re-entry | Stable command lifetime and correct reset |

## M4. Verify all four placements and hand off

**Files:** the evidence report and release validation report.
**Interfaces:** consumes the evidence-backed profile commit and normal candidate build; produces runtime evidence and an explicit pass/fail/not-run matrix.

- [ ] Run all profile/runtime/export tests on unchanged and retextured exports.
- [ ] In the normal candidate, inspect each of the four user placements in its relevant day/night scene, from multiple camera positions and after scene re-entry. Record the placement-to-material map, not just one successful fountain.
- [ ] Verify motion direction, speed, layer alignment and loop/phase reset against the donor reference. A plausible animation is not evidence of 1:1 behavior.
- [ ] Verify a newly copied/re-exported recipe and a retexture inherit the binding.
- [ ] Draw Lake water, Pool water and Lost Woods light sheet alongside the new material and inspect neighboring geometry for segment state leakage.
- [ ] Record missing runtime evidence honestly and return to the release index. No standalone diagnostics-only game build or automatic merge.
