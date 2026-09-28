# SoH performance port candidate

Baseline: `marsh6487/Shipwright`, `integration/nei-weather-static-actors`, `628deea3cf851af19a18f915e0665d772294c383`. Engine remains pinned to `c57da1b4afa775b24b58b2adf93d63d3b561bb65`. Candidate: `poc/soh-performance-port-20260928`. The integration branch is the recovery point and is not promoted by this candidate.

Donors: ComboShip `909b41f8` (warm render lookup), `35adbdcc` (triangle state reuse), `dd11be2d`/`7f2aea65` (packed attributes/direct dispatch), and `43d8443c` (synchronous load and directional normal reuse). PR #21 was accepted by the user; PR #22 had build/static verification but runtime acceptance was not established in the retrieved evidence. Porting either is a new runtime candidate.

## Scope and preservation

- Reuse ResourceManager's existing Alt/owner/archive/dirty decisions for warm render lookups. Return warm synchronous loads without allocating a ready promise/future; move owned cached references instead of extra retain/release operations.
- Reuse prepared triangle state only during consecutive pure triangle commands, with command/frame barriers. Reuse packed vertex attributes only within the current submission buffer; flushes and state boundaries invalidate them.
- Pass the active interpreter directly through command dispatch. Retain SoH command bodies and resource ownership; do not bring ComboShip routing into SoH.
- Reuse adjacent equal normals' directional-lighting RGB/generated UV results within one vertex command. Position transforms, positional lights, fog, alpha and clipping retain baseline calculations.
- Preserve SoH's existing metadata-only resource fallback, native material scrolling, textures, geometry, collision, actor behavior, progression, sound and weather. No assets are changed. No RenderFlight instrumentation is added.
- Engine changes are a reviewed patch stored in the parent repository, applied before engine configuration and the cumulative test gate. Exact before/after SHA-256 hashes guard all affected files. Repeated application is a no-op; unknown edits fail instead of being overwritten. The submodule revision is unchanged.

## PAK and voice audit

SoH's equipment loader and menu were byte-identical to the donor's OoT counterparts before the port. All 27 slot definitions and widgets remain. No equipment loader/menu production files are changed. Existing production selection tests cover reorder, deletion, category checks, migration and saved path identity.

The user clarified that voice `.pak` files were not discovered. Both branches share a voice scanner which only inspects immediate `mods` children and accepts only the exact `.pak` suffix. A separate candidate change adds recursive, case-insensitive discovery with the same `soh`/`2ship` sibling exclusion as the model loader. Archive parsing and audio playback remain unchanged. A production-init fixture failed on nested and uppercase files before the change and passes after it. The user's exact archive/location was not available: this proves a shared discovery omission, not the cause of every reported missing voice pack. ComboShip itself is not modified here.

## Verification

- Baseline resource fixture passed before changes. A new zero-allocation warm-load requirement failed before the port and passed after it.
- 16,384 triangle variants pass bitwise vertex/backend comparison; their output checksums match the original pinned SoH engine. Packed offsets, buffer flushes, rectangles, texture sampler changes, LOD, frame resets and direct dispatch are exercised.
- 10,240 vertex batches compare against the exact original SoH engine function. ASan/UBSan/float-cast-overflow pass 8,192 valid-input batches; the original null-member-address case is excluded only for sanitizer execution. LeakSanitizer is disabled locally because the execution environment cannot inspect `/proc`; this is not a leak check.
- ASan/UBSan triangle checks pass with the same local LeakSanitizer limitation.
- Resource tests exercise direct Alt display lists, metadata-only fallback, toggle, dirty reload, missing/null/error entries, external replacements, manager/owner/archive-parent changes and OTR prefix/initData behavior.
- Clean-copy patch application, repeat application and refusal to overwrite unrelated edits pass.
- Two existing fixtures calling engine handlers directly are updated for the new explicit interpreter argument.
- Full local cumulative regression gate passed, including the existing feature, equipment selection, Epona cosmetics, medallion, pedestal and stabilization suites. Independent read-only review found no blocking issue. Full-platform CI is pending; source fixtures do not establish a full game build or runtime performance.

## Runtime acceptance

Use the same packs/order, settings, model and resolution as the running SoH baseline. Compare Hyrule Field first, then Lost Woods Rooms 9/10, Sacred Forest Meadow and Kakariko. Check vanilla/Alt toggles, room re-entry, pause/unpause, scrolling materials, reflective lighting and PAK equipment. For voice discovery, restart with the real pack in its intended directory and check listing, selection and playback. No FPS improvement or runtime acceptance is claimed before these observations.
