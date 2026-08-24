/**
 * item_elemental_wand.c — Elemental Wand (Skijer's NEI)
 *
 * Six rods share ONE page-2 cell, ONE item id (ITEM_ELEMENTAL_WAND) and ONE item action. Which rod
 * is live is NeiSaveData.wandMode, cycled by the kaleido wheel; this file is where that mode turns
 * into behavior.
 *
 * WHY ONE ACTION FOR SIX RODS
 * ---------------------------
 * Not a shortcut — a hard constraint. SoH's PlayerItemAction space is 0x00-0x7F and every value is
 * taken, and `heldItemAction` is s8 so there is nothing above 0x7F either. The wand therefore
 * shares PLAYER_IA_UNUSED_5B with the Mario Mask, and extended_player.c's Nei_SharedIA_* trampolines
 * split the two apart by held ITEM before reaching here. Inside this file the split continues by
 * MODE. The same shape as the SW97 bow: one action, one flag, behavior chosen at dispatch.
 *
 * STATUS
 * ------
 * The six rod behaviors are deliberately unimplemented — that is its own task, one rod at a time,
 * with a spec per rod. What IS live: the wand equips, holds, aims, draws its per-rod icon and name,
 * cycles its modes, and is randomizer-placeable. Each rod below has a named home to drop its
 * implementation into, so adding one never has to touch the dispatch again.
 */

#include "global.h"
#include "mods/extended_inventory.h" // Wand_GetMode / WAND_MODE_*

extern s32 func_8083485C(Player* this, PlayState* play); // generic "held item" upper action

/**
 * Per-rod upper action. Runs every frame while the wand is the held item.
 *
 * Returning func_8083485C keeps the vanilla hold/aim handling, which is what every rod wants as its
 * base — the rod-specific work goes in its own case before that.
 */
s32 Player_UpperAction_ElementalWand(Player* player, PlayState* play) {
    switch (Wand_GetMode()) {
        case WAND_MODE_SAND: // Spirit Medallion
            // TODO(rod): Sand Rod — raise/lower sand pillars along the aimed line.
            break;
        case WAND_MODE_TORNADO: // Forest Medallion
            // TODO(rod): Tornado Rod — updraft that lifts Link and light objects.
            break;
        case WAND_MODE_WATER: // Water Medallion
            // TODO(rod): Water Rod — spawn a water column / raise water level locally.
            break;
        case WAND_MODE_METEOR: // Fire Medallion
            // TODO(rod): Meteor Rod — call down a fire impact at the aimed point.
            break;
        case WAND_MODE_STORM: // Light Medallion
            // TODO(rod): Storm Rod — thunderstorm strike on the aimed target.
            break;
        case WAND_MODE_SCEPTER: // Shadow Medallion
            // TODO(rod): Shadow Scepter — shadow clone / darkness field.
            break;
        default:
            break;
    }

    return func_8083485C(player, play);
}

/**
 * Runs once when the wand becomes the held item. Per-rod setup (charge timers, spawned helper
 * actors, aim reticles) belongs here, keyed the same way as the upper action above.
 */
void Player_InitElementalWandIA(PlayState* play, Player* player) {
    // No per-rod init needed while the behaviors are stubs. Kept as the named entry point so adding
    // a rod is a local change instead of a dispatch change.
    (void)play;
    (void)player;
}
