# NEI Hook & De-duplication Audit

**Generated:** 2026-06-19
**Base of comparison:** merge-base `adf31d5eb` (last upstream merge) → NEI working tree
**Scope:** the **169 upstream files NEI modified** (the merge-pain surface). The 562 NEI-*new* files are conflict-free and out of scope. `develop` is currently 16 commits ahead of this base — not pulled; this audit only measures *our* edits.

Goal: for every edit to an upstream file, decide whether it can become a **hook** or **helper** so the upstream file stays (nearly) untouched, making future `git pull` trivial — and find **redundant code with similar behavior that can be linked**.

---

## 0. How to read the classifications

| Class | Meaning |
|---|---|
| **HOOK-EXISTING** | A suitable hook seam (a `VB_*` point, a broad `On*` hook, or a registration API) **already exists** — register a handler, ~0 upstream edit. *Best case.* |
| **HOOK-NEW-VB** | Hookable, but a new `VB_*` point must be added at the seam first (1 small upstream line + handler in NEI). |
| **EXTRACT-HELPER** | Bespoke, but the **same shape repeats** across siblings — collapse the body into an NEI helper, leave a 1–3 line call. |
| **REGISTER-IN-NEI** | Glue/UI/init bolted into an upstream function that could move wholesale to an NEI file invoked via an existing registration API. |
| **SIDE-TABLE** | A struct/save **field addition** — move to `ObjectExtension` or an NEI side-store instead of widening the upstream struct. |
| **INEVITABLE** | Must edit upstream (canonical enum entry, logic-graph predicate, tight FSM, layout-critical). Can only be **isolated/minimized**, not hooked away. |
| **TRIVIAL** | Whitespace / reflow / comment / 1-liner — ignore functionally, but **revert reflow** to kill phantom conflicts. |

---

## 1. Executive summary — the prize

The 169-file diff is **far smaller in real content than it looks.** Three findings dominate:

### A. ~3,700 lines are *pure noise or dead code* — removable with ZERO behavior change
- **clang-format reflow** on upstream files NEI never functionally changed: `z64.h` alone is ~**1,140** churned lines that reduce to **~2 real ones**; plus `z64player.h`, `z64save.h`, the `iconNameTextures[]` repack in `z_kaleido_scope_PAL.c` (~360), and 7 comment-only files (`z_lights.c`, `z_scene.c`, `z_fbdemo_circle.c`, `z_effect_soft_sprite_dlftbls.c`, `sprintf.c`, `z_inventory.c`, `stubs.c`).
- **Dead duplicate blocks**: `shops.cpp` `#if 0 InitTrickNames` (~**1,530** lines — table now lives in `Traps.cpp`), `ItemMessages.cpp` duplicate `IceTrapMessages` (~**210**), `item_pool.cpp` dead `std::array` copies (~**270**).

> **Reverting reflow + deleting dead code makes `z64.h` and `shops.cpp` — two of your three worst files — essentially disappear from the diff.** This is the single highest value / lowest risk action in the whole audit.

### B. The hook infrastructure you need *already exists and is barely used*
- **363 `VB_*` hooks** exist upstream; NEI added only **2**, yet edited 169 files by hand.
- `RegisterMenuInitFunc` (menu self-registration) is used by 10+ sibling modules — NEI edits `SohMenu.cpp` inline instead.
- `RandomizerOnItemReceiveHandler` exists — NEI hand-wrote ~60 give-item `switch` arms.
- `ObjectExtension` exists — NEI added fields to `Player`/`SaveContext`.
- `COND_ID_HOOK`/`RegisterShipInitFunc` self-registration — the Desire-Sensor system (~258 lines in `OTRGlobals.cpp`) could be **zero** upstream lines.

### C. A few "whole-file rewrite" anti-patterns guarantee *eternal* conflicts
- **`z_bg_ice_shelter.c` is the worst:** NEI **reverted the entire file to pre-documentation `func_8089XXXX` names + magic numbers** and **dropped two registered VBs** (`VB_BG_ICE_SHELTER_HIT/MELT`). Every upstream line will conflict forever. Must be re-ported on top of current upstream.

**Rough split of the ~real diff:** after removing noise/dead code, the remainder is roughly **40–50% reducible** (hookable or de-dupable) and **50–60% irreducible floor** (canonical enums, logic predicates, kaleido FSM, save schema — see §6).

---

## 2. Cross-cutting wins — do these first (high value, low risk)

### 2.1 Revert clang-format reflow (🟢 trivial, do immediately)
Restore upstream formatting on: `z64.h`, `z64player.h`, `z64save.h`, `soh_assets.h` (4 comment lines), `z_kaleido_scope_PAL.c` (`iconNameTextures[]`), `z_lights.c`, `z_scene.c`, `z_fbdemo_circle.c`, `z_effect_soft_sprite_dlftbls.c`, `sprintf.c`, `z_inventory.c`, `stubs.c`, plus the stray `lusprintf` debug line in `z_message_PAL.c`. **~1,700 lines of guaranteed future conflict, zero functional cost.**

### 2.2 Delete dead duplicate code (🟢 trivial)
- `shops.cpp`: delete the `#if 0 InitTrickNames/GetIceTrapName` block (~1,530 L) → **file drops to zero NEI delta.** Put NEI fake names in `Traps.cpp::InitTrickNames()`.
- `ItemMessages.cpp`: delete duplicate `IceTrapMessages` arrays (~210 L) → call existing `Rando::Traps::BuildIceTrapMessage`.
- `item_pool.cpp`: delete unreferenced `std::array` copies of `Rando::StaticData::*` (~270 L).

### 2.3 Upstream the genuine bugfixes (🟢 makes the diff *disappear*, not just move)
These are not NEI features — submit as upstream PRs and the conflict vanishes:
- `ObjectExtension.h` — `Set(const T&&)` → `const T&` (rvalue→lvalue fix).
- `ResourceManagerHelpers.cpp` — null guards in `LoadTexOrDList`/`LoadIfDList`.
- `SaveManager.cpp` — `InitFileDebug/Maxed` `item < sItems.size()` bounds guard.
- `AudioSampleFactory.cpp` — libogg/Vorbis NULL guards + log-instead-of-throw (3 sites).
- `GbiWrap.cpp` — `gSPInvalidateTexCache` null-keep guard.
- `z_lib.c` — `Math_Vec3f_Yaw` NULL guard.
- `code_800F7260.c` — `bankId` bounds guard.

### 2.4 Stop the whole-file rewrites
- **`z_bg_ice_shelter.c/.h`**: re-port only the *additions* (Blue-Fire-arrow collider via `OnActorInit`, `MeltOnIceArrowHit`, append-only `*_Instantly`/`ShatterMelt` public fns) on top of the **current** upstream file; keep its modern names/enums and restore the 2 dropped VBs.

### 2.5 Use the registration APIs that already exist
- **Menus:** convert `AddMenuNEI()`/`AddMenuSw97()` from `SohMenu::` members to **free functions self-registered via `static RegisterMenuInitFunc`** in `SohMenuNEI.cpp`. Eliminates ALL edits to `SohMenu.cpp`, `SohMenu.h`, `SohMenuEnhancements.cpp`, `SohMenuDevTools.cpp`, `SohMenuNetwork.cpp` (Anchor already self-registers — that edit is redundant, just revert it).
- **GUI windows:** self-register `AnimationViewerWindow` + Harpoon `RemoteSaveEditorWindow` → no `SohGui.cpp`/`SohGui.hpp` edits.
- **Give-item effects:** move `randomizer.cpp` give arms into the existing `RandomizerOnItemReceiveHandler`, table-driven.
- **Desire Sensor:** move the 6 functions to `DesireSensorHints.cpp`; it self-registers via `COND_ID_HOOK(OnOpenText)` → **zero** upstream call. (~330 of `OTRGlobals.cpp`'s ~532 NEI lines leave the file.)
- **Save editor pages:** move the 3 `CollapsingHeader` blocks (~388 L) in `debugSaveEditor.cpp` to `NeiSaveEditor.cpp`, leaving 2 one-line calls.

---

## 3. The seam toolbox (mechanisms available)

| Mechanism | Where | Use for |
|---|---|---|
| `GameInteractor_Should(VB_X, default, optActor)` | `vanilla-behavior/GIVanillaBehavior.h` (363 points) | Conditionally wrap / skip / replace vanilla behavior |
| Broad hooks `OnActorInit/Update`, `OnPlayerUpdate`, `OnSceneInit`, `ShouldActorInit`, `OnItemReceive`, `OnKaleidoUpdate`, `OnOpenText` | `game-interactor/GameInteractor_Hooks.h` | Bolt NEI behavior onto an actor/system, filtered by id |
| `RegisterMenuInitFunc` + `MenuInit::GetInitFuncs()` | `SohGui/MenuTypes.h`, drained in `SohMenu.cpp:102` | Add menu pages/widgets from an NEI file |
| `RandomizerOnItemReceiveHandler` | `randomizer/hook_handlers.cpp` | Give-item side effects |
| `RegisterShipInitFunc` + `COND_ID_HOOK` | ship init system | Self-registering subsystems, zero upstream call |
| `ObjectExtension` | `ObjectExtension/ObjectExtension.h` | Per-actor data without struct fields |
| NEI save-section blob | `SaveManager` section API | Save fields without widening `SaveContext` |
| `PakLoader_GetDLOverride` / a `VB_OVERRIDE_GFX_DL` | `GbiWrap.cpp` | DL swap for pak/skin/harpoon (3 consumers today) |

---

## 4. Redundancy clusters — "link similar behavior"

These are the highest-value de-duplication targets (behavior is similar → one source of truth):

1. **11 boss super-damage blocks → one `BossSuperDamage_TryHit()` + `DrawGlowFromSpheres()`.** Each boss's ~35–60-line CollisionCheck block and ~15-line Draw block collapse to ~12 + ~1 lines; only death-sequence stays boss-specific. (Va and Tw don't fit the damage path — they keep custom mechanics.)

2. **Randomizer `RG → {ITEM, RSK, RAND_INF, drawFn}` mapping, hand-written 4×:** `logic.cpp HasItem` (~115 arms), `logic.cpp ApplyItemEffect` (24 mask arms), `randomizer.cpp Randomizer_Item_Give` (~60 arms), pool/obtainability. → **one NEI `constexpr` table** drives ownership, give-effects, draw registration, pool insertion.

3. **`draw.cpp` — 59 near-identical custom-item draw functions** (differ only by DL ptr + scale + tint) → one generic `DrawCustomItem(DL, scale, rgb)` + a `{RG→DL,scale,rgb}` table. (The 24-mask `DrawMmMask` already proves it.) Also: the whole 749-line block is *appended* — move to `draw_nei.cpp`.

4. **`location_access` — 372 insertions / 372 deletions, all 1-for-1 substitutions, zero new data.** Two shapes: (a) strength/boulder checks (already routed to `HasStrength()`/`CanBreak*Boulder()` — good), (b) **open-coded fire/light/ice/shield OR-chains** (`CanUse(RG_FIRE_ARROWS) || CanUse(RG_SW97_FIRE) || CanUse(RG_FIRE_ROD)`) repeated dozens of times though the custom RGs are *already* inside `CanUse`/`HasFireSource`. → route through semantic helpers `HasFireSource()/HasLightSource()/HasIceSource()/CanShield()`.

5. **Icon indirection `gItemIcons[x]` → `ExtInv_GetItemIcon(x)` — 25–30×** across `z_kaleido_*`, `z_parameter.c`, `z_message_PAL.c`, `ImGuiUtils.cpp`. Since `ExtInv_GetItemIcon` already falls back to `gItemIcons`, replace all call-site edits with **one draw-time icon-resolver seam**.

6. **5 Roc's-Feather-style kaleido selectors** (Lantern, Clawshot, Gale, GustJar, ArrowWheel) — identical A-toggle / stick-LR / `KaleidoCycle_DrawRocStyle` shape → move all to `mods/kaleido_selectors/`, hang off one `OnKaleidoItemCycles` hook (2 call sites touch upstream).

7. **`z_player.c` magic-number form checks** — `MmForm_GetCurrentForm() == N` copy-pasted ~30+× with raw constants (and an FD `0` vs `4` inconsistency = latent bug) → named predicates `MmForm_IsZora()` etc. Plus the **`Player_IsFloating()`** condition duplicated verbatim 4×, and the **anim-override idiom** (`MhrMoveset_Get*Anim → if(anim) play`) in ~7 sites → one `VB_PLAYER_ANIM_OVERRIDE` seam absorbing both MHR and MM-form overrides.

8. **MM-DL-or-fallback** (`dl = vanilla; if (mode){ mm=Load(); if(mm) dl=mm; } draw(dl)`) in `z_arms_hook.c` (×2), `z_en_boom.c`, `z_en_m_thunder.c` → one `MmDL_Or(vanillaDL, loaderFn)` helper.

9. **Ivan/SM64 damage multiplier** block duplicated 3× (`z_collision_check.c` ×2, `z_actor.c`) → `NEI_GetDamageMultiplier(play)`. **MM-form SFX suppression** 2× in `z_actor.c` → `NEI_ShouldSuppressPlayerSfx()`.

10. **Gerudo "friendly-on-mask"** (Ge1/Ge2/Ge3 + En_Cow), **MM-mask "keep on sale"** (En_Mm + En_Heishi2, byte-identical), **All-Night-Mask night-GS** (En_Sw + En_Wood02, identical `||` append) → shared predicates + `OnActorUpdate(id)` handlers.

11. **SW97 6-element ordering** (fire/ice/light/dark/soul/wind ↔ medallion ↔ `ITEM_`/`ARROW_`/`PLAYER_IA_`) duplicated 5+ places → one canonical SW97 descriptor table.

12. **`extern "C"` compat shims** (SwitchAge.h, mod_menu.h, GbiWrap pak fwd-decl) → one `soh/soh/NEI/nei_exports.h`.

---

## 5. Per-cluster detail tables (the full map)

### 5.1 Core headers
| File | Theme | Class | Note |
|---|---|---|---|
| z64.h | clang-format reflow (~1140 L) | TRIVIAL | revert → ~2 real lines (`lensFromLantern`) |
| z64.h | `lensFromLantern` field | SIDE-TABLE | ObjectExtension / actorCtx side-store |
| z64player.h | full-file reflow | TRIVIAL | revert |
| z64player.h | 27 `PLAYER_IA_*` (0x43–0x5C) | INEVITABLE | append-only; 0x5D+ already in `mods/extended_player.h` |
| z64player.h | `ivanFloating`, `ivanDamageMultiplier` | SIDE-TABLE | ObjectExtension on Player |
| z64save.h | macro/comment reflow (~half) | TRIVIAL | revert |
| z64save.h | `items[24]→[72]`, `equipment u16→u32` | INEVITABLE | CRC/offset-tied save layout |
| z64save.h | 14 `ShipSaveContextData` fields | SIDE-TABLE | NEI save-section blob |
| z64item.h | 21 `SLOT_*` **reordered**, SHIELD 0x1E→0x33 | INEVITABLE (High risk) | **switch to append-only** to drop the reorder |
| z64item.h | ~70 `ITEM_*` (0x9E–0xDE) | INEVITABLE | append-only — fine |
| z64audio.h | 5 MM-BGM fn decls + `resolvedFont` | EXTRACT-HELPER / INEVITABLE | decls → NEI header; `resolvedFont` layout-tied |
| z64collision_check.h | `DMG_FIXED_DAMAGE` macro | TRIVIAL | → NEI header if desired |
| soh_assets.h | ~170 custom asset decls | EXTRACT-HELPER | move to NEI-owned `nei_assets.h` |

### 5.2 z_player.c (decomposed — biggest single file)
Highest-leverage seams (full per-site table omitted for brevity — see agent notes): a **`VB_PLAYER_ANIM_OVERRIDE`** seam absorbs ~15 scattered anim overrides (MHR + MM form); a **`Player_Draw` override / post-draw seam** (complementing existing `VB_PLAYER_DRAW`) removes the ~130-line Pak/O2r/Harpoon/SM64/MM/SSBB draw fork — the most merge-fragile block; the per-frame **update/init/input plumbing** (`*_Update`, `*_Tick`, `*_Init`, `*_Filter`) should register on the already-firing `GameInteractor_ExecuteOnPlayerUpdate` / `OnActorInit` instead of being inlined (~25 call-site edits removed); the 25-line mod-include header block → one `nei_player_includes.h` shim. Classes span HOOK-EXISTING (item-action lookup, per-frame updates, mask draw via `VB_DRAW_PLAYER_MASK`), HOOK-NEW-VB (use-item, incoming-damage, shield-burn, floor-damage, z-target source, reset-scale), EXTRACT-HELPER (voice/SFX/speed/anim/boomerang/roll), and a few INEVITABLE (mod includes, de-static of item tables). Two literal-swap sites (`true`→`0`) look accidental → revert.

### 5.3 Engine `code/`
| File | Theme | Class |
|---|---|---|
| z_player_lib.c | `OverrideLimbDrawGameplayDefault` / `PostLimbDrawGameplay` (gerudo/pak/ext-equip/clawshot/rods) | INEVITABLE (High) — **#1 recurring hotspot per `upstream_merge_risks.md`**; wrap behind a per-limb dispatch hook (`VB_PLAYER_POST_LIMB_DRAW`) |
| z_player_lib.c | boot data / strength / face tex / gauntlets / shield-coltype | HOOK-NEW-VB ×several |
| z_parameter.c | `Interface_Draw` SM64/Pika HUD + B/C/D-pad hide | HOOK-NEW-VB (`VB_DRAW_INTERFACE`) — largest non-player diff |
| z_parameter.c | icon routing `gItemIcons`→`ExtInv` (×12), ext-equip button enable (×3), mask buttons | EXTRACT-HELPER / icon seam |
| z_parameter.c | `Health_ChangeBy` breastplate/SM64 | HOOK-NEW-VB (`VB_HEALTH_CHANGE_BY`) |
| z_actor.c | `Player_PlaySfx` MM/voice intercept | HOOK-NEW-VB (`VB_PLAYER_PLAY_SFX`) |
| z_actor.c | Champion slow-factor, height, lens-from-lantern, stealth-dist | HOOK-EXISTING / SIDE-TABLE / HOOK-NEW-VB |
| z_collision_check.c | UNBLOCKABLE/FIXED_DAMAGE + Ivan/SM64 mult + FD sword | HOOK-NEW-VB + shared `NEI_GetDamageMultiplier` |
| z_bgcheck.c | noclip (Vanish/HGrace), Mogma climb | HOOK-EXISTING |
| z_camera.c | Minish view adjust, skip crawlspace cam | HOOK-NEW-VB |
| z_play.c | SM64 surface refresh, custom kaleido dispatch, overlays | HOOK-EXISTING / HOOK-NEW-VB |
| z_frame_advance.c / main.c | FleetShip freeze / host bootstrap | HOOK-NEW-VB / INEVITABLE (1 line) |
| code_800E4FE0.c | 5 custom audio mixers | EXTRACT-HELPER → one `NEI_AudioMixInto()` |
| audio_load.c | `resolvedFont` >256 fix + MM register helpers | INEVITABLE / EXTRACT-HELPER |
| z_lib.c, code_800F7260.c | NULL/bounds guards | INEVITABLE → **upstream as bugfix** |
| z_effect_soft_sprite_dlftbls.c, z_fbdemo_circle.c, z_lights.c, z_scene.c, sprintf.c, z_inventory.c | comment reflow | TRIVIAL → revert |

### 5.4 Boss super-damage (11 bosses + FHG flash)
All follow the shared pattern. **One new "post-boss-collision" VB seam + `BossSuperDamage_TryHit(boss, acFlags, hitBit, pos, slack, health, isOpen, cooldown)` → `{NONE,STUN,DAMAGE,KILL}`** collapses Dodongo/Fd/Fd2/Ganon/Ganon2/Ganondrof/Goma/Mo/Sst from ~35–60 lines each to ~12–16. `DrawGlowFromSpheres(actor, play, jntSph, n, scale)` collapses the Draw glow blocks (~13–15 L) to **1**.
**Outliers (stay custom):** Va (global frame-flag "super break", 5 sites — SIDE-TABLE), Tw (two reflect models + **physically relocates upstream lines = highest conflict risk**), Fd↔Fd2 `FD2_SIGNAL_AIRKILL` handoff, and `z_eff_ss_fhg_flash.c/.h` `FHGFLASH_SHOCK_ANY_ACTOR` (INEVITABLE VFX backend, self-contained). ~927 added → realistic floor ~300–350 boss-specific.

### 5.5 Harpoon / arrows / projectiles
| File | Theme | Class |
|---|---|---|
| z_en_arrow.c | 6× boundary widening inside vanilla conditionals | INEVITABLE (High) — shrink via one `EnArrow_IsCustom(params)` |
| z_en_arrow.c | SW97/seed init, 3× child-spawn loops, SFX switch | HOOK-EXISTING via `OnActorInit/Update(EN_ARROW)` + `sSw97Arrows[6]` table |
| z_en_arrow.h | 12 `ARROW_*` enums | INEVITABLE (append-only) |
| z_en_boom.c | tomahawk/zora trail-color/collider/DL by params; `EnBoom_FlyTomahawk` | EXTRACT-HELPER (table) + INEVITABLE (new action) via `OnActorInit/Update(EN_BOOM)` |
| z_arms_hook.c | clawshot bullet-time + MM tip/chain DL swap | HOOK-EXISTING (`VB_DRAW_HOOKSHOT_TIP/CHAIN`) / HOOK-NEW-VB |
| z_en_m_thunder.c/.h | FD sword-beam subtype + action; `homingTarget` field | INEVITABLE (verbatim MM port) / SIDE-TABLE |
| ArrowCycle/BlueFire/Sunlight.cpp | already in enhancement TUs | TRIVIAL / EXTRACT-HELPER |

### 5.6 Misc actors
| File | Theme | Class |
|---|---|---|
| **z_bg_ice_shelter.c/.h** | **whole-file reverted to legacy names + dropped 2 VBs** | INEVITABLE (**Very High**) — **re-port additions onto current upstream** (§2.4) |
| z_obj_lightswitch.c | SW97 light-arrow collider + 5 melt edits + dropped `VB_LIGHTSWITCH_OFF` | INEVITABLE (High) — extract to mods/ + restore VB |
| z_en_ge1/ge2/ge3.c | Gerudo friendly-on-mask | HOOK-EXISTING (`VB_GERUDOS_BE_FRIENDLY`) + GTG `VB_GTG_GUARD_ALLOW` |
| z_en_cow.c | Romani Mask → Chateau | HOOK-EXISTING (`VB_GIVE_ITEM_FROM_COW`) |
| z_en_mm.c + z_en_heishi2.c | keep MM-mask on sale (identical) | HOOK-NEW-VB (`VB_TAKE_MASK_ON_SALE`, shared) |
| z_en_rd.c, z_en_du.c/.h | Gibdo/Kamaro friendly dance | SIDE-TABLE (append-only helpers) + HOOK-EXISTING |
| z_en_sw.c + z_en_wood02.c | All-Night Mask night-GS (identical `||`) | TRIVIAL → shared `ShouldSpawnNightGS()` |
| z_en_skb.c | Captain's Hat rupee drop (sentinel) | HOOK-NEW-VB |
| z_en_encount1.c | Stone Mask suppresses Leevers | HOOK-EXISTING |
| z_bg_jya_ironobj.c/.h | Ball&Chain instant destroy | EXTRACT-HELPER (append-only) |
| z_bg_hidan_dalm.c | Goron-punch as hammer; SM64 CS-halt skip | HOOK-EXISTING |

### 5.7 Kaleido (pause menu)
Largest reducible: (1) **icon-override seam** replacing 25–30 `gItemIcons→ExtInv` edits, (2) move ~1,100 lines of per-item selectors to `mods/kaleido_selectors/` behind one `OnKaleidoItemCycles` hook, (3) revert `iconNameTextures[]` reflow (~360 L). Reuse existing VBs: `VB_EQUIP_ITEM_TO_C_BUTTON` (SM64 equip block), `VB_DRAW_CUSTOM_ITEM_NAME` (name textures — also dedupe the twice-pasted Twilight swap), C-Up description branch.
**Honest floor (INEVITABLE):** `equipment.c` extended-equipment page and `ExtInv_GetInventorySlot` page-2 remap (~18×) are woven into kaleido's cursor FSM — these stay high-conflict.

### 5.8 Randomizer core
Dominated by §2.2 dead-code deletion (shops.cpp→0, ItemMessages −45%, item_pool −270) and §4.2 the `RG→{...}` table. Move give-effects to `RandomizerOnItemReceiveHandler`; move `customItemMessages[]` (~462 L) + `draw.cpp` block (749 L) to NEI files. **Irreducible:** +89 enum lines (RG_/RAND_INF_/RSK_, append-only), ~75 `item_list.cpp Item(...)` rows, and the **~30 interleaved `|| CanUse(RG_X)` insertions in `logic.cpp` predicate bodies** (the real recurring hotspot, no clean seam).

### 5.9 Randomizer location_access (24 files)
**372/372 pure substitutions, zero new rows.** Strength/boulder half already routed to helpers. The reducible half: open-coded fire/light/ice/shield OR-chains → semantic helpers `HasFireSource()/HasLightSource()/HasIceSource()/CanShield()`. Adopt a **"no literal gauntlet/arrow checks in location files" convention** so post-merge re-apply is mechanical. 2 outliers: `fire_temple` (`RG_DEMISE_DESTRUCTION`), `graveyard` (`RG_SHOVEL`, already `//TODO`).

### 5.10 C++ infra
Top moves: Desire-Sensor (~258 L) + scene-hint + `nei/` archive scan → `DesireSensorHints.cpp` (self-registers, **zero** upstream call, ~330/532 of `OTRGlobals.cpp` leaves); `debugSaveEditor.cpp` 3 UI blocks (~388 L) → `NeiSaveEditor.cpp` (2 one-liners); upstream the bugfixes (§2.3). The GameInteractor extensions (2 VB enums, GIScheme, 3 CC effects/RawActions) are **idiomatic clean appends — keep them**. `SaveManager` 9-field ×3 schema is INEVITABLE.

### 5.11 Audio / network / misc infra
- `AudioSequence.h::resolvedFont` (**High risk**): struct is `reinterpret_cast` against `SequenceData` (z64audio.h) — move seq→font mapping to an NEI side-table, restore both struct layouts.
- `GbiWrap.cpp` `gSPDisplayList`→`PakLoader_GetDLOverride` → `VB_OVERRIDE_GFX_DL` (serves pak + harpoon-skin + the 3rd consumer).
- `Anchor/GiveItem.cpp` custom-item fork → `VB_ANCHOR_GIVE_ITEM`.
- `CustomMessageTypes.h` +93 `TEXT_*` → NEI text-ID block ≥0x9300.
- `extern "C"` shims (SwitchAge, mod_menu) → `nei_exports.h`.

### 5.12 SohGui + build/meta
All menu/window inline edits → **`RegisterMenuInitFunc` self-registration** (§2.5) — collapses ~5 menu files to zero. INEVITABLE: `soh/CMakeLists.txt` (globs to compile NEI sources, `/guard:cf`), `MenuTypes.h` COREAUDIO comment-out (LUS enum missing). README/.gitignore/tasks.json — keep (TRIVIAL, accept conflicts).

---

## 6. The irreducible floor (be honest — these stay edited)

Even after everything above, these *must* keep touching upstream; goal is to **isolate/minimize**, not eliminate:
- **Canonical enum appends** — `RG_*`, `ITEM_*`, `SLOT_*`, `PLAYER_IA_*`, `RAND_INF_*`, `RSK_*` (keep **append-only**; the `SLOT_SHIELD` *reorder* is the one to fix).
- **`logic.cpp` capability predicates** — ~30 `|| CanUse(RG_X)` woven into `CanUseSword/CanKillEnemy/HasProjectile/...`. No clean seam.
- **Kaleido inventory-nav FSM** — `ExtInv_GetInventorySlot` remaps + extended-equipment page in `equipment.c`/`item.c`/`z_kaleido_scope_PAL.c`.
- **Save schema** — `SaveManager` init/Load/Save triples + the `items[72]`/`equipment u32` widening.
- **`item_list.cpp`** — one `Item(...)` row of real metadata per item.
- **A few 1-line splice points** — FleetShip render-gating/audio-mute in `OTRGlobals.cpp`, FleetShip host bootstrap in `main.c`, the fhg-flash VFX backend.

---

## 7. Proposed sequencing (poco a poco)

| Phase | Action | Risk | Lines removed from diff | Touches behavior? |
|---|---|---|---|---|
| **1** | Revert all clang-format reflow (§2.1) | 🟢 None | ~1,700 | No |
| **2** | Delete dead duplicate code (§2.2) | 🟢 None | ~2,010 | No |
| **3** | Upstream the 7 bugfixes (§2.3) | 🟢 Low | (vanishes on merge) | No |
| **4** | Menu/window self-registration (§2.5) | 🟢 Low | ~5 files → 0 | No |
| **5** | Re-port `z_bg_ice_shelter.c` onto upstream (§2.4) | 🟡 Med | restores names + 2 VBs | Equivalent |
| **6** | Boss `TryHit` + `DrawGlow` helpers (§4.1) | 🟡 Med | ~580 → ~150 | Equivalent |
| **7** | Randomizer `RG` table + give-handler + `draw_nei.cpp` (§4.2-3) | 🟡 Med | ~1,000+ | Equivalent |
| **8** | Icon-override seam (§4.5) + kaleido selectors to mods/ | 🟡 Med | ~1,100+ | Equivalent |
| **9** | `z_player.c` anim-override + draw seams (§5.2) | 🔴 High | ~400+ | Equivalent (careful) |
| **10** | location_access semantic helpers + convention (§4.4) | 🟡 Med | mechanical re-merge | Equivalent |

**Phases 1–4 are pure cleanup** (no behavior change, near-zero risk) and remove the majority of the *noise*. Do them first, verify the build, then tackle the redundancy refactors one cluster at a time, each behind its own commit + manual verification.
