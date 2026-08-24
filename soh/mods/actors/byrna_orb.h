/**
 * byrna_orb.h — Cane of Byrna (ext sword 1) light orb / Insect Glaive Kinsect.
 *
 * ONE object, two polarities. The Cane of Byrna in A Link to the Past spirals a
 * barrier around Link that blocks hits and damages what it touches; the Insect
 * Glaive's Kinsect flies OUT to a monster and comes back carrying an extract.
 * They are the same orb with the sign flipped, so this is a single actor with
 * two travelling states:
 *
 *   ORBIT    — circles Link. Damages on contact. Absorbs exactly ONE incoming
 *              hit and shatters (no drain, no invulnerability: see the design
 *              decision in MHR_EXT_SWORD_PORT_SPEC.md §12.4).
 *   OUTBOUND — launched at a target with R+B. Deals one sword hit and harvests
 *              an extract. While it is out, Link is NOT protected — that is the
 *              risk half of the trade.
 *   RETURN   — flies back to Link and resumes orbiting, applying the extract.
 *
 * SUPER DAMAGE: only the OUTBOUND orb (plus its post-impact grace) claims
 * Fierce-Deity-class damage against bosses. An orbiting orb must NOT, or simply
 * standing next to a boss with the orb up would paralyse it every frame — the
 * same trap trident_charge_ball.h documents. The claim lives on the projectile,
 * never on Link's state.
 *
 * The implementation (byrna_orb.c) is TEXT-INCLUDED from extended_equipment.c —
 * it is not a standalone translation unit and is not in the vcxproj. Only the
 * accessors below are exported.
 *
 * Skijer's NEI
 */

#ifndef BYRNA_ORB_H
#define BYRNA_ORB_H

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Kinsect extract colours. Which one an enemy yields is a static table keyed by
 *  actor id (byrna_orb.c), because OoT enemies have no per-part hitboxes to
 *  harvest the way MHR monsters do. */
typedef enum {
    BYRNA_EX_NONE = 0,
    BYRNA_EX_RED,    // attack up
    BYRNA_EX_WHITE,  // movement speed + vault height
    BYRNA_EX_ORANGE, // defence up
    BYRNA_EX_GREEN,  // heal (instant, does not linger)
    BYRNA_EX_MAX
} ByrnaExtract;

/**
 * True while a LAUNCHED orb is in flight or inside the post-impact grace window.
 *
 * The grace window is NOT optional. Actors update in category order (PLAYER = 2
 * before BOSS = 9). The orb is driven from the player-side dispatch, so on the
 * frame after its AT lands it sees its own AT_HIT and dies BEFORE the boss gets
 * to read BUMP_HIT — the boss would then find this predicate already false and
 * drop the super hit in silence. Same bug already chased down for Mario's
 * fireball (sm64_mario_items.c, sFireGraceTimer / MARIO_FB_GRACE).
 */
u8 ByrnaOrb_IsActive(void);

/** Summon the orb into ORBIT. Pays BORB_MAGIC_COST. Returns 0 if there is not
 *  enough magic or an orb already exists. */
u8 ByrnaOrb_Summon(PlayState* play);

/** Send the orbiting orb at a target (R+B). Returns 0 if no orb is orbiting. */
u8 ByrnaOrb_Launch(PlayState* play);

/** Called from the player damage path. If an orb is ORBITing it shatters and
 *  eats the hit; returns 1 to tell the caller the damage was consumed. */
u8 ByrnaOrb_TryAbsorb(PlayState* play);

/** Per-frame tick for the grace window and the buff timers. Driven from
 *  ExtEquip_Update so both still expire when no orb is alive. */
void ByrnaOrb_Tick(void);

/** Drop the orb and clear every buff — called when the slot is unequipped. */
void ByrnaOrb_Cleanup(void);

/**
 * Forget the orb WITHOUT touching the actor. Call on scene load: every spawned
 * actor has already been freed by then, so Cleanup's Actor_Kill would dereference
 * a dangling pointer — and doing nothing at all would leave sOrb.actor non-NULL
 * forever, which makes Summon refuse for the rest of the session. Extract buffs
 * are timed and deliberately survive the transition.
 */
void ByrnaOrb_Forget(void);

/** True while that extract's buff is running. */
u8 ByrnaOrb_HasBuff(u8 extract);

/** True while the ORANGE (defence) buff is up. Its own accessor so the damage
 *  chokepoint in z_player.c does not have to know the extract enum. */
u8 ByrnaOrb_DefenseActive(void);

/** Multiplier the damage chokepoint should apply to incoming damage: 0.5 while
 *  ORANGE is up, 1.0 otherwise. */
f32 ByrnaOrb_IncomingDamageMul(void);

/** Multipliers for the moveset to apply. All return 1.0f with no buff up. */
f32 ByrnaOrb_AttackMul(void);
f32 ByrnaOrb_SpeedMul(void);
f32 ByrnaOrb_VaultMul(void);

/** True while RED + WHITE + ORANGE are all up — MHR's "final form": incoming
 *  knockback is ignored. */
u8 ByrnaOrb_NoFlinch(void);

#ifdef __cplusplus
}
#endif

#endif // BYRNA_ORB_H
