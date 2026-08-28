/**
 * item_rod_of_seasons.c — Rod of Seasons (Skijer's NEI)
 *
 * Four seasons share ONE page-2 cell (SLOT_ROD_OF_SEASONS) behind ONE ext item id
 * (EXT_ITEM_ROD_OF_SEASONS), the Sheikah Slate idiom: ownership is the NeiSaveData.seasonsOwned
 * bitmask, one sibling pickup per season, and NeiSaveData.season is the live one. Holding the C
 * button the rod sits on opens the season wheel; releasing it confirms.
 *
 * The season is world state, not a cast: this file pushes it into envCtx every frame, which is also
 * what re-applies it after a scene load wipes the environment context.
 */

#include "global.h"
#include "mods/extended_inventory.h"
#include "mods/items/helpers/equip_helper.h"
// box_menu.c is unity-included just before this file in custom_items.c, so its BoxMenu_*
// declarations are already in scope — it has no header (see the note at its top).

// Ext-button store: which u16 item a button really holds when it shows ITEM_EXT_BUTTON.
extern u16 ExtButton_GetItem(s32 btn);

#define SEASON_WHEEL_HOLD_FRAMES 8

// Precipitation targets. Rain matches the Kakariko thunderstorm's density (without its thunder) and
// snow the vanilla flurry; blossom is deliberately thinner, since petals read as clutter at 64.
#define SEASON_RAIN_DROPS 30
#define SEASON_SNOWFLAKES 64
#define SEASON_BLOSSOM 48

static s16 sSeasonHoldTimer = 0;

// ============================================================================
// WEATHER
// ============================================================================

// The vanilla snow grey, which is also what every season that is not Spring draws with.
#define SEASON_SNOW_GREY 200

/**
 * Tint for the Object_Kankyo precipitation particles, read from its draw. Spring's blossom rides
 * that same particle system, so the colour is the only thing separating the two — every other case,
 * the rod included, keeps the vanilla snow.
 */
void Seasons_PrecipTint(u8* r, u8* g, u8* b) {
    if ((Seasons_SeasonCount() != 0) && (Seasons_GetSeason() == SEASON_SPRING)) {
        Seasons_SeasonColor(SEASON_SPRING, r, g, b);
        return;
    }
    *r = *g = *b = SEASON_SNOW_GREY;
}

// Is the rod currently the thing driving the particle count? Object_Kankyo's fairies keep THEIR
// count in that same field, so a season with no particles must hand it back instead of pinning it
// to zero — otherwise the rod deletes the fairies in every scene that has them.
static u8 sOwnsPrecip = 0;

/**
 * Snow and blossom are drawn by Object_Kankyo, never by the engine — setting the flake count with no
 * such actor in the scene draws nothing at all. Spawning while nothing is falling is the vanilla
 * "Let It Snow" idiom; the actor's own init kills the duplicate.
 *
 * KNOWN GAP: that init keeps ONE Object_Kankyo of any kind, so in the three fairy scenes (Kokiri
 * Forest, Lost Woods, Sacred Forest Meadow) the fairies hold the slot and Winter falls dry there.
 */
static void Seasons_TakePrecip(PlayState* play, u8 count) {
    play->envCtx.unk_EE[3] = count;
    sOwnsPrecip = 1;
    if (play->envCtx.unk_EE[2] == 0) {
        Actor_Spawn(&play->actorCtx, play, ACTOR_OBJECT_KANKYO, 0, 0, 0, 0, 0, 0, 3);
    }
}

// Drain what the previous season left falling, then stop touching the field at all.
static void Seasons_ReleasePrecip(PlayState* play) {
    if (!sOwnsPrecip) {
        return;
    }
    play->envCtx.unk_EE[3] = 0;
    if (play->envCtx.unk_EE[2] == 0) {
        sOwnsPrecip = 0;
    }
}

// gloomySkyMode is a two-step handshake: 1 darkens, 2 hands it to the restore branch, which zeroes
// it itself. Writing 2 unconditionally would strand it non-zero and block the weather tags.
static void Seasons_ClearSky(EnvironmentContext* env) {
    if (env->gloomySkyMode == 1) {
        env->gloomySkyMode = 2;
    }
}

// The rain and thunder tracks belong to the nature ambience sequence, which shares the main BGM
// player — starting it would replace the scene's music. A season lasts, so the rain is voiced as a
// plain looping sound effect instead.
static void Seasons_PlayRainSfx(void) {
    Sfx_PlaySfxCentered(NA_SE_EV_RAIN - SFX_FLAG);
}

/**
 * Pushes the active season into the environment context. Every write is idempotent, so this runs
 * unconditionally each frame and needs no "what changed" bookkeeping — which is exactly what makes
 * the season survive a scene load, where Environment_Init zeroes all of it.
 */
static void Seasons_ApplyWeather(PlayState* play, u8 season) {
    EnvironmentContext* env = &play->envCtx;

    // A season is sky weather: only scenes that actually have a sky get one. Without this the rod
    // rains inside every dungeon and shop.
    if ((play->skyboxId != SKYBOX_NORMAL_SKY) || env->indoors) {
        env->unk_EE[0] = 0;
        Seasons_ReleasePrecip(play);
        return;
    }

    switch (season) {
        case SEASON_SPRING:
            env->unk_EE[0] = 0;
            Seasons_TakePrecip(play, SEASON_BLOSSOM);
            Seasons_ClearSky(env);
            break;
        case SEASON_SUMMER:
            env->unk_EE[0] = 0;
            Seasons_ReleasePrecip(play);
            Seasons_ClearSky(env);
            break;
        case SEASON_AUTUMN:
            // Particles on screen suppress the engine's whole rain pass, so the rain can only start
            // once the previous season's flakes have drained out.
            Seasons_ReleasePrecip(play);
            env->unk_EE[0] = SEASON_RAIN_DROPS;
            env->gloomySkyMode = 1;
            Seasons_PlayRainSfx();
            break;
        case SEASON_WINTER:
            env->unk_EE[0] = 0;
            Seasons_TakePrecip(play, SEASON_SNOWFLAKES);
            env->gloomySkyMode = 1;
            break;
        default:
            break;
    }
}

// ============================================================================
// INPUT TICK — hold the rod's C button to pick a season
// ============================================================================

// The box-menu confirm: the highlighted season becomes the active one.
static void Seasons_OnWheelConfirm(s32 index) {
    Seasons_SetSeason((u8)index); // no-op if that season is not owned
}

// Fills the row with ALL seasons — locked ones included, drawn grayed and unselectable, so the wheel
// doubles as a reminder of what is still missing (the slate's rune row does the same).
static s32 Seasons_BuildWheel(BoxMenuEntry* out) {
    for (s32 s = 0; s < SEASON_COUNT; s++) {
        out[s].iconPath = (const char*)Seasons_SeasonIcon((u8)s);
        out[s].iconSize = 32;
        out[s].enabled = Seasons_SeasonOwned((u8)s);
    }
    return SEASON_COUNT;
}

// Every C button currently holding the rod. C items live in the flat button array at 1..3.
static u16 Seasons_EquippedButtonMask(void) {
    u16 mask = 0;

    if (ExtButton_GetItem(1) == EXT_ITEM_ROD_OF_SEASONS) {
        mask |= BTN_CLEFT;
    }
    if (ExtButton_GetItem(2) == EXT_ITEM_ROD_OF_SEASONS) {
        mask |= BTN_CDOWN;
    }
    if (ExtButton_GetItem(3) == EXT_ITEM_ROD_OF_SEASONS) {
        mask |= BTN_CRIGHT;
    }
    return mask;
}

/**
 * Per-frame rod input, called from Player_UpdateCommon. The weather runs off ownership alone: once a
 * season is owned the world is in it, whether or not the rod sits on a button.
 */
void Seasons_TickInput(PlayState* play, Player* player) {
    BoxMenuEntry entries[SEASON_COUNT];
    u16 btnMask;

    if (Seasons_SeasonCount() == 0) {
        sSeasonHoldTimer = 0;
        return;
    }

    Seasons_ApplyWeather(play, Seasons_GetSeason());

    if (BoxMenu_IsOpen()) {
        return; // the menu owns the frame (and the game is paused anyway)
    }

    btnMask = Seasons_EquippedButtonMask();
    if ((btnMask == 0) || ItemInput_IsBlocked(player, play)) {
        sSeasonHoldTimer = 0;
        return;
    }

    // Read from cur.button, never press: the wheel opens on a HOLD, and the release edge is the
    // menu's own business once it is up.
    if (play->state.input[0].cur.button & btnMask) {
        if (sSeasonHoldTimer < (SEASON_WHEEL_HOLD_FRAMES + 1)) {
            sSeasonHoldTimer++;
        }
        if (sSeasonHoldTimer == SEASON_WHEEL_HOLD_FRAMES) {
            s32 count = Seasons_BuildWheel(entries);

            BoxMenu_Open(play, entries, count, Seasons_GetSeason(), btnMask, Seasons_OnWheelConfirm);
        }
        return;
    }
    sSeasonHoldTimer = 0;
}
