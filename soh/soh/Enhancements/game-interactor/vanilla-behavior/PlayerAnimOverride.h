#ifndef PLAYER_ANIM_OVERRIDE_H
#define PLAYER_ANIM_OVERRIDE_H

// Site identifiers for the VB_PLAYER_ANIM_OVERRIDE positioned hook.
//
// NEI moved its player animation-override blocks out of the vanilla file
// `soh/src/overlays/actors/ovl_player_actor/z_player.c` into a single
// GameInteractor handler (see RegisterPlayerAnimOverride in
// soh/soh/Enhancements/customequipment.cpp). Each enumerator below names one
// converted call site. The vanilla code sets a local anim pointer to the
// stock animation, then calls:
//
//     GameInteractor_Should(VB_PLAYER_ANIM_OVERRIDE, true, <siteId>, <siteArg>, &anim, this);
//
// and plays whatever `anim` points to afterwards. The handler reproduces the
// exact same override decision the inline code made (including the original
// TransformMasks_IsTransformed() / animation-range gating), so with the
// handler unregistered (or returning no override) the played animation is
// identical to stock OOT.
//
// This header is included from both C (z_player.c) and C++ (customequipment.cpp).
typedef enum {
    // func_80835884: Zora boomerang throw-wait loop.
    // vanilla: &gPlayerAnim_link_boom_throw_waitR  (LinkAnimation_PlayLoop)
    // override: MmForm_GetZoraBoomerangAnim(0) when TransformMasks_IsTransformed()
    // siteArg: unused (0)
    VB_PLAYER_ANIM_SITE_ZORA_BOOMERANG_WAIT,

    // func_80835B60: Zora boomerang catch.
    // vanilla: &gPlayerAnim_link_boom_catch  (LinkAnimation_PlayOnce)
    // override: MmForm_GetZoraBoomerangAnim(2) when TransformMasks_IsTransformed()
    // siteArg: unused (0)
    VB_PLAYER_ANIM_SITE_ZORA_BOOMERANG_CATCH,

    // Player_SetupRoll: landing/roll.
    // vanilla: GET_PLAYER_ANIM(PLAYER_ANIMGROUP_landing_roll, modelAnimType)
    //          (LinkAnimation_PlayOnceSetSpeed)
    // override: MhrMoveset_GetMoveAnim(4 /* MHR_MOVE_ROLL */)
    // siteArg: unused (0)
    VB_PLAYER_ANIM_SITE_ROLL,

    // func_8083BCD0: directional dodge hop (main hop anim only).
    // vanilla: D_80853D4C[controlStickDirection][0]  (func_80838940)
    // override: MhrMoveset_GetMoveAnim(controlStickDirection)
    // siteArg: controlStickDirection (0..3 = fwd/right/backflip/left)
    VB_PLAYER_ANIM_SITE_DODGE_HOP,

    // func_80836980 (Player_SetupAction -> Player_Action_80843188): shield raise.
    // vanilla: GET_PLAYER_ANIM(PLAYER_ANIMGROUP_defense, modelAnimType)
    //          or &gPlayerAnim_clink_normal_defense_ALL  (LinkAnimation_Change)
    // override: MhrMoveset_GetShieldAnim(0)
    // siteArg: unused (0)
    VB_PLAYER_ANIM_SITE_SHIELD_RAISE,

    // Player_Action_80843188: shield hold loop.
    // vanilla: GET_PLAYER_ANIM(PLAYER_ANIMGROUP_defense_wait, modelAnimType)
    //          (Player_AnimPlayLoop)
    // override: MhrMoveset_GetShieldAnim(1)
    // siteArg: unused (0)
    VB_PLAYER_ANIM_SITE_SHIELD_LOOP,

    // func_808502D0 region: jump-slash recovery (post-attack end animation).
    // vanilla: locally computed sp3C end animation  (func_8083A098)
    // override: MmForm_GetJumpSlashAnim(meleeWeaponAnimation) when
    //           TransformMasks_IsTransformed() and meleeWeaponAnimation is in
    //           [PLAYER_MWA_FLIPSLASH_FINISH, PLAYER_MWA_JUMPSLASH_FINISH]
    // siteArg: unused (0) - meleeWeaponAnimation is read from player
    VB_PLAYER_ANIM_SITE_JUMPSLASH_RECOVERY,
} VBPlayerAnimOverrideSite;

#endif // PLAYER_ANIM_OVERRIDE_H
