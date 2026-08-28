/**
 * gerudo_form.cpp — Gerudo Form (OOT Gerudo Mask, Garo-style hybrid).
 *
 * See gerudo_form.h for the design overview.
 *
 * What this file owns:
 *   - Mask-edge-detect: polls player->currentMask each frame; on a rising
 *     edge (mask just equipped AND cheat on), calls O2rLoader_ForceModel
 *     ("gerudo"). On a falling edge, clears the forced model.
 *   - GameInteractor VB hooks: VB_GERUDOS_BE_FRIENDLY → true while active,
 *     VB_GIVE_ITEM_GERUDO_MEMBERSHIP_CARD → false (no card autograntee).
 *   - Sandstorm-OFF enforcement in Haunted Wasteland (per-frame + on
 *     transition end).
 *   - Haunted Wasteland "cross the desert" offer: the vanilla lost-warp runs
 *     untouched, but on spawning in the wasteland a skippable Yes/No textbox
 *     offers a direct warp to the opposite side of the desert (skip the maze).
 *   - GerudoForm_GetTunicColor helper used by the hybrid render to recolor
 *     the gerudo outfit with Link's current tunic.
 *   - The hand DLs (dual scimitars / no sheath), read straight off the MHR
 *     fighter latch — this file keeps NO combat state.
 *
 * What this file does NOT own: combat. Every Gerudo move lives in
 * gerudo_mhr_combat.inc.c behind MmForm_GerudoMhrUpdate (MHR dual-blade combo,
 * charge, wirebug on R, demon mode). The pre-MHR state machine that used to sit
 * at the bottom of this file (3-slash combo on OOT sword anims, R = block +
 * Mirror Shield reflect, its own meleeWeaponQuads) was deleted 2026-08-07 along
 * with its TransformMasks_Update callsite — same cleanup Garo got in v9, and for
 * the same reason: a second state machine fighting the form for player->skelAnime.
 *
 * The gerudo look is a pure path-swap on Link's own draw — no separate
 * skeleton, null-body pass, or gerudo SkelAnime. While the "gerudo" model is
 * forced, CustomForms_OverrideLimbDraw (custom_forms.cpp) rewrites the
 * DL strings Player_DrawImpl emits from `objects/object_link_boy/...` to the
 * gerudo .o2r's `objects/forms/gerudo/object_link_boy/...` twins.
 *
 * Replaces the older skin-pack approach (alt/-pathed Link skeleton + idle
 * pose override on a sFormSkelAnime). That approach is now retired.
 */

#include "gerudo_form.h"
#include "mods/transformation_masks/transformation_masks.h" // MmPlayerTransformation, MM_PLAYER_FORM_GERUDO
#include "mods/o2r_loader/o2r_loader.h"

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/ResourceManagerHelpers.h"

#include <libultraship/bridge.h>
#include <spdlog/spdlog.h>
#include <cstring>

extern "C" {
#include "macros.h"    // GET_PLAYER
#include "variables.h" // gSaveContext
#include "functions.h" // Message_StartTextbox / Message_CloseTextbox
// ResourceMgr_LoadGfxByName is already declared by soh/ResourceManagerHelpers.h
// (included above). Don't redeclare it here — a mismatched signature breaks the
// whole TU.
}

// Fighter latch owned by gerudo_mhr_combat.inc.c: 1 while the scimitars are in
// Link's hands. This file has no say in it — it only draws what the latch says.
extern "C" u8 GerudoMhr_SwordsOut(void);

extern "C" PlayState* gPlayState;
extern "C" Color_RGB8 sTunicColors[];

#define CVAR_GERUDO_TRANSFORM "gMods.GerudoMaskTransform"

namespace {

bool IsWearingGerudoMask() {
    if (gPlayState == nullptr) {
        return false;
    }
    Player* player = GET_PLAYER(gPlayState);
    return player != nullptr && player->currentMask == PLAYER_MASK_GERUDO;
}

// Edge-detection state for the mask toggle. Rising edge → ForceModel,
// falling edge → ClearForcedModel. Polled in OnPlayerUpdate.
bool sPrevWantGerudo = false;

// --- Haunted Wasteland "cross the desert" offer ----------------------------
// Gerudo desert skill. Rather than fighting the vanilla lost-warp (which loops
// you back to the entrance), we let it run completely — it works and never
// softlocks. THEN, once you've spawned in the wasteland (whether you entered
// legitimately or got looped back), we present a skippable Yes/No textbox
// offering to warp straight across to the OTHER side of the desert, skipping
// the maze. Pick "No" (or press B) to walk it yourself.
//
// Detecting "you're back at the entrance" is just "a transition into the
// wasteland finished" (OnTransitionEnd) — getting lost respawns you via a full
// transition, so the offer re-appears each time you end up back here.
constexpr uint16_t kWastelandWarpTextId = 0x9FB0; // free slot above spiritual_stones' 0x9FA0-0x9FA2
bool sWastelandOfferPending = false;              // entered the wasteland; show the offer once
bool sWastelandOfferOpen = false;                 // offer textbox currently on screen
bool sWastelandOfferToFortress = false;           // message/dest target: true = Fortress, false = Colossus
s16 sWastelandOfferDest = -1;                     // entrance to warp to if accepted
bool sWastelandWarpChosen = false;                // our own warp transition is running

void GerudoForm_ResetWastelandOffer() {
    sWastelandOfferPending = false;
    sWastelandOfferOpen = false;
    sWastelandOfferDest = -1;
    sWastelandWarpChosen = false;
}

void GerudoForm_TickWastelandWarp(PlayState* play) {
    Player* player = GET_PLAYER(play);

    // 1. Offer textbox is up → watch for B (skip) or the choice closing.
    if (sWastelandOfferOpen) {
        Input* input = &play->state.input[0];
        bool skip = (input != nullptr) && CHECK_BTN_ALL(input->press.button, BTN_B);

        if (skip) {
            Message_CloseTextbox(play);
        }
        if (skip || play->msgCtx.msgMode == MSGMODE_NONE) {
            bool warp = !skip && (play->msgCtx.choiceIndex == 0); // choice 0 == "Yes"
            sWastelandOfferOpen = false;
            if (player != nullptr) {
                player->stateFlags1 &= ~PLAYER_STATE1_IN_CUTSCENE;
            }
            if (warp && sWastelandOfferDest >= 0) {
                sWastelandWarpChosen = true;
                play->nextEntranceIndex = sWastelandOfferDest;
                play->transitionType = TRANS_TYPE_FADE_BLACK_FAST;
                play->transitionTrigger = TRANS_TRIGGER_START;
            }
        }
        return;
    }

    // 2. Open the offer once we're in normal control (post-load, no textbox,
    //    no transition, player not already frozen).
    if (sWastelandOfferPending && !sWastelandWarpChosen && play->msgCtx.msgMode == MSGMODE_NONE &&
        play->transitionTrigger == TRANS_TRIGGER_OFF && play->transitionMode == TRANS_MODE_OFF && player != nullptr &&
        !(player->stateFlags1 & (PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING))) {
        sWastelandOfferPending = false;
        sWastelandOfferOpen = true;
        player->stateFlags1 |= PLAYER_STATE1_IN_CUTSCENE; // freeze while choosing
        Message_StartTextbox(play, kWastelandWarpTextId, nullptr);
    }
}

// Defined in mm_player_form.cpp.
extern "C" MmPlayerTransformation MmForm_GetCurrentForm(void);

void GerudoForm_OnPlayerUpdate() {
    if (gPlayState == nullptr) {
        return;
    }

    // ForceModel("gerudo") / ClearForcedModel are driven by the MM form
    // pipeline now (MmForm_LoadFormSkeleton on flash peak, MmForm_RestoreOotState
    // on detransform). Polling here is only a safety net — if the user pulls
    // the mask off via UI (not via re-press), make sure the skin clears.
    bool cheatOn = CVarGetInteger(CVAR_GERUDO_TRANSFORM, 0) != 0;
    bool want = cheatOn && IsWearingGerudoMask();
    if (!want && sPrevWantGerudo) {
        const char* cur = O2rLoader_GetForcedName();
        if (cur != nullptr && std::strcmp(cur, "gerudo") == 0 && MmForm_GetCurrentForm() != MM_PLAYER_FORM_GERUDO) {
            O2rLoader_ClearForcedModel();
            SPDLOG_INFO("[GerudoForm] mask removed via UI — ClearForcedModel (safety net)");
        }
    }
    sPrevWantGerudo = want;

    // Sandstorm OFF in Haunted Wasteland (per-frame, so toggling the mask
    // mid-scene clears the sandstorm without re-entering the area), plus the
    // "cross the desert" offer.
    //
    // CRITICAL: never force OFF while a transition is running. Every wasteland
    // exit/entry uses a TRANS_TYPE_SANDSTORM_* wipe whose completion check
    // waits for sandstormPrimA/EnvA to fill (z_play.c TRANS_MODE_SANDSTORM) —
    // and those alphas only advance inside Environment_DrawSandstorm, which is
    // skipped entirely while sandstormState == SANDSTORM_OFF. Forcing OFF
    // mid-wipe therefore hangs the transition forever: the player freezes
    // (LOADING|IN_CUTSCENE) on the exit plane and can never leave the desert.
    if (GerudoForm_IsActive() && gPlayState->sceneNum == SCENE_HAUNTED_WASTELAND) {
        if (gPlayState->transitionTrigger == TRANS_TRIGGER_OFF && gPlayState->transitionMode == TRANS_MODE_OFF) {
            gPlayState->envCtx.sandstormState = SANDSTORM_OFF;
        }
        GerudoForm_TickWastelandWarp(gPlayState);
    } else if (sWastelandOfferOpen || sWastelandOfferPending) {
        // Form deactivated (or left the scene) with the offer pending — drop it
        // so the player isn't left frozen.
        Player* p = GET_PLAYER(gPlayState);
        if (p != nullptr) {
            p->stateFlags1 &= ~PLAYER_STATE1_IN_CUTSCENE;
        }
        GerudoForm_ResetWastelandOffer();
    }
}

void GerudoForm_OnTransitionEnd(int16_t sceneNum) {
    // A completed transition clears the in-flight offer/warp state. If we just
    // arrived in the wasteland (legit entry OR looped back from getting lost)
    // while wearing the mask, arm the "cross the desert" offer for the side
    // opposite the one we spawned at.
    GerudoForm_ResetWastelandOffer();

    if (sceneNum == SCENE_HAUNTED_WASTELAND && GerudoForm_IsActive() && gPlayState != nullptr) {
        gPlayState->envCtx.sandstormState = SANDSTORM_OFF;

        // Spawn 0 = entered from Gerudo Fortress (east) → offer Desert Colossus.
        // Spawn 1 = entered from Desert Colossus (west) → offer Gerudo Fortress.
        sWastelandOfferToFortress = (gPlayState->curSpawn != 0);
        sWastelandOfferDest =
            sWastelandOfferToFortress ? ENTR_GERUDOS_FORTRESS_GATE_EXIT : ENTR_DESERT_COLOSSUS_EAST_EXIT;
        sWastelandOfferPending = true;
    }
}

} // namespace

extern "C" u8 GerudoForm_IsActive(void) {
    // Primary signal: MM form pipeline says we're Gerudo (ACTIVE / TRANSFORMING /
    // DETRANSFORMING). Fallback to O2rLoader for legacy code paths that fire
    // before the MM state updates (e.g. mid-cutscene draw hooks).
    if (MmForm_GetCurrentForm() == MM_PLAYER_FORM_GERUDO) {
        return 1;
    }
    if (!CVarGetInteger(CVAR_GERUDO_TRANSFORM, 0)) {
        return 0;
    }
    const char* cur = O2rLoader_GetForcedName();
    return (cur != nullptr && std::strcmp(cur, "gerudo") == 0) ? 1 : 0;
}

// No-op now — an earlier architecture rendered the gerudo body through a
// separate skel/SkelAnime. Current pipeline uses a Link-rigged gerudo skin
// (DL redirection in CustomForms_OverrideLimbDraw) so Player_DrawImpl handles
// everything. Kept so z_player.c's older callsite (if any survives) still
// links cleanly.
extern "C" s32 GerudoForm_TryDrawSmoothSkin(PlayState* play, Player* player) {
    (void)play;
    (void)player;
    return 0;
}

// Dual-wield sword DLs sourced from soh.o2r. Same DL for both hands and
// both ages — the adult Master Sword DL is used universally. The right-hand
// bone matrix mirrors it naturally so the second sword orients correctly,
// and the child skel's smaller bone scale shrinks the sword proportionally
// so it doesn't look oversized on child Link.
namespace {
constexpr const char* kGerudoSword =
    "__OTR__objects/forms/gerudo/object_link_boy/gLinkAdultLeftHandHoldingMasterSwordNearDL";

// The gerudo-skinned R-hand shield DLs used to be declared here
// (gLinkAdultRightHandHoldingHylianShieldNearDL / ...MirrorShield... /
// gLinkChildRightFistAndDekuShieldNearDL, all present in soh.o2r from the repack).
// Dropped 2026-07-28 with the shield decoupling: Gerudo never shields (R = wirebug) and
// no form may pick a model from player->currentShield. See GerudoForm_GetSwordDL_R.

// Sword visibility has exactly one owner: the MHR fighter latch
// (gerudo_mhr_combat.inc.c, GerudoMhr_SwordsOut). Latch up = scimitars in both
// hands, latch down = empty hands, and the unsheath/sheathe SFX fire on its
// edges. The local FREE/COMBAT machine that used to live here was a SECOND
// latch keyed on gerudoQuadsActive — it only lit during a damage window, so the
// charge stance, both wirebugs and the dodge all rendered with empty hands.
// Removed 2026-08-07 (see also the deleted shield-mode tracking: with
// MmForm_GetShieldMode() == MMFORM_SHIELD_BLOCK, PLAYER_STATE1_SHIELDING can
// never set for Gerudo, so every branch keyed on it was dead).
} // namespace

// ResourceMgr_LoadGfxByName crashes (null deref on `res->Instructions[0]`) if
// the path doesn't exist in any loaded .o2r. Gate every call with FileExists.
static Gfx* SafeLoadGfx(const char* path) {
    if (path == nullptr || !ResourceMgr_FileExists(path))
        return nullptr;
    return ResourceMgr_LoadGfxByName(path);
}

// Both hands draw the same scimitar DL and both follow the one latch.
extern "C" Gfx* GerudoForm_GetSwordDL_L(void) {
    if (!GerudoForm_IsActive() || !GerudoMhr_SwordsOut())
        return nullptr;
    // Demon mode puts the IK Axe in her hands instead. It cannot come back from here —
    // this returns a bare display list and the axe needs its own matrix — so both
    // scimitars go away and MmForm_PostLimbDraw draws the axe. Skijer's NEI
    if (GerudoMhr_RageActive())
        return nullptr;
    return SafeLoadGfx(kGerudoSword);
}

extern "C" Gfx* GerudoForm_GetSwordDL_R(void) {
    if (!GerudoForm_IsActive() || !GerudoMhr_SwordsOut())
        return nullptr;
    // Demon mode is a two-handed axe: the right hand holds nothing. Skijer's NEI
    if (GerudoMhr_RageActive())
        return nullptr;
    // The old gerudo-skinned SHIELD branch lived here: while the vanilla Mirror Shield
    // action was up, the right hand drew kGerudoShieldAdultHylian/Mirror/ChildDeku picked
    // from player->currentShield. Removed 2026-07-28 — no form reads the equipped shield.
    // It was already dead after the MHR rework (R is the wirebug; OOT's shield actions are
    // gated off for Gerudo by MmForm_GetShieldMode() == MMFORM_SHIELD_BLOCK, so
    // PLAYER_STATE1_SHIELDING never sets). The right hand is now always scimitar-or-nothing.
    //
    // Same DL as the left hand — the right-hand bone matrix mirrors it, so the
    // second blade orients correctly on its own.
    return SafeLoadGfx(kGerudoSword);
}

// Gerudo Form dual-wield: hand = scimitar DL, sheath hidden. Returns 1 if it claimed limbIndex. Skijer's NEI
extern "C" u8 GerudoForm_ResolveLimbDL(s32 limbIndex, Gfx** dList) {
    if (!GerudoForm_IsActive()) {
        return 0;
    }
    if (limbIndex == PLAYER_LIMB_L_HAND) {
        Gfx* swordL = GerudoForm_GetSwordDL_L();
        if (swordL != nullptr) {
            *dList = swordL;
            return 1;
        }
    } else if (limbIndex == PLAYER_LIMB_R_HAND) {
        Gfx* swordR = GerudoForm_GetSwordDL_R();
        if (swordR != nullptr) {
            *dList = swordR;
            return 1;
        }
    } else if (limbIndex == PLAYER_LIMB_SHEATH) {
        // Hide the scabbard only while the scimitars are actually in her hands.
        // With the blades stowed Gerudo is meant to look like plain Link — vanilla
        // animations, vanilla back sheath — so blanking this unconditionally is
        // what made "put the swords away" produce no visible change at all.
        if (GerudoMhr_SwordsOut()) {
            *dList = nullptr;
            return 1;
        }
    }
    return 0;
}

extern "C" void GerudoForm_GetTunicColor(s32 tunic, Color_RGB8* out) {
    if (out == nullptr) {
        return;
    }
    if (tunic < PLAYER_TUNIC_KOKIRI || tunic > PLAYER_TUNIC_ZORA) {
        tunic = PLAYER_TUNIC_KOKIRI;
    }
    Color_RGB8 c = sTunicColors[tunic];

    if (tunic == PLAYER_TUNIC_KOKIRI && CVarGetInteger(CVAR_COSMETIC("Link.KokiriTunic.Changed"), 0)) {
        c = CVarGetColor24(CVAR_COSMETIC("Link.KokiriTunic.Value"), sTunicColors[PLAYER_TUNIC_KOKIRI]);
    } else if (tunic == PLAYER_TUNIC_GORON && CVarGetInteger(CVAR_COSMETIC("Link.GoronTunic.Changed"), 0)) {
        c = CVarGetColor24(CVAR_COSMETIC("Link.GoronTunic.Value"), sTunicColors[PLAYER_TUNIC_GORON]);
    } else if (tunic == PLAYER_TUNIC_ZORA && CVarGetInteger(CVAR_COSMETIC("Link.ZoraTunic.Changed"), 0)) {
        c = CVarGetColor24(CVAR_COSMETIC("Link.ZoraTunic.Value"), sTunicColors[PLAYER_TUNIC_ZORA]);
    }
    *out = c;
}

extern "C" void GerudoForm_Init(void) {
    static bool initialized = false;
    if (initialized) {
        return;
    }
    initialized = true;

    // Gerudo NPCs treat the player as a friendly Gerudo while the mask is worn.
    REGISTER_VB_SHOULD(VB_GERUDOS_BE_FRIENDLY, {
        if (GerudoForm_IsActive()) {
            *should = true;
        }
    });

    // Skip the forced card-giving path while the mask is worn — access is
    // temporary, no QUEST_GERUDO_CARD is granted.
    REGISTER_VB_SHOULD(VB_GIVE_ITEM_GERUDO_MEMBERSHIP_CARD, {
        if (GerudoForm_IsActive()) {
            *should = false;
        }
    });

    // En_GeldB (Gerudo Fighter miniboss) — don't throw the player in jail
    // while the mask is worn. The miniboss is a duel test, not a fortress
    // patrol; Gerudo lore says they wouldn't capture one of their own.
    REGISTER_VB_SHOULD(VB_GERUDO_FIGHTER_THROW_LINK_TO_JAIL, {
        if (GerudoForm_IsActive()) {
            *should = false;
        }
    });

    // Desert skill — Haunted Wasteland "cross the desert" offer.
    // The vanilla lost-warp runs untouched (no interception → no softlock).
    // Once we spawn in the wasteland (legit entry or looped back from getting
    // lost), GerudoForm_OnTransitionEnd arms a skippable Yes/No offer to warp
    // straight to the opposite side of the desert; GerudoForm_TickWastelandWarp
    // opens it in normal control and performs the warp (or B/No to walk it).

    // Offer message — Yes/No, injected on demand for our custom textId. The
    // destination side is chosen from the spawn point in OnTransitionEnd.
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnOpenText>(
        [](uint16_t* textId, bool* loadFromMessageTable) {
            if (*textId != kWastelandWarpTextId) {
                return;
            }
            const char* body = sWastelandOfferToFortress ? "Cross the desert to the&Gerudo Fortress?\x1B%gYes&No%w"
                                                         : "Cross the desert to the&Desert Colossus?\x1B%gYes&No%w";
            CustomMessage msg(body);
            msg.AutoFormat();
            msg.LoadIntoFont();
            *loadFromMessageTable = false;
        });

    COND_HOOK(OnTransitionEnd, true, GerudoForm_OnTransitionEnd);
    COND_HOOK(OnPlayerUpdate, true, GerudoForm_OnPlayerUpdate);
}

// ShipInit-registered entry point. Hooks check the cheat CVar at call time,
// so no need to register/unregister on CVar changes.
static void GerudoForm_RegisterShipInit() {
    GerudoForm_Init();
}
static RegisterShipInitFunc sGerudoFormInitFunc(GerudoForm_RegisterShipInit, {});

// ============================================================================
// Combat lives in gerudo_mhr_combat.inc.c — NOT here.
//
// Deleted 2026-08-07: the pre-MHR state machine (GCS_IDLE/SLASH_1..3/JUMP_ATTACK/
// BLOCK/RECOVER, its own meleeWeaponQuads[0] geometry, the OOT sword anims
// link_normal_light_bom / Lnormal_kiru / Wrolling_kiru / jump_rollkiru, the
// kf_hanare_loop block stance and its ReflectProjectiles Mirror-Shield bounce),
// plus GerudoForm_Update and the heldItemAction/itemAction pinning that ran on
// top of it.
//
// Why it had to go:
//   * It was the design the MHR Dual Blades rework replaced (R is the wirebug,
//     the moveset is the mhr_db clips, damage comes from
//     gFormState.gerudoQuadsActive quads built in PostLimbDraw).
//   * TransformMasks_Update called it unconditionally every frame — the exact
//     shape of the Garo double-tick bug fixed in v9.
//   * The heldItemAction pinning existed ONLY to satisfy OOT's Mirror Shield
//     pipeline, which Gerudo no longer has (MMFORM_SHIELD_BLOCK). It rewrote
//     heldItemAction + itemAction without heldItemId / func_8008EC70, i.e. the
//     mismatch that Player_UpperAction_ChangeHeldItem re-detects every frame —
//     the historical equip/unequip loop documented in mm_player_form.cpp's
//     MmForm_FDKeepSwordInHand. Nothing restored it on detransform either.
//     (Zora keeps an equivalent pin, and legitimately so: its shield mode is
//     MMFORM_SHIELD_FORM_GUARD, so the vanilla pipeline still has to run.)
// ============================================================================
