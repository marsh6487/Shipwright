/**
 * weapon_upgrades.h - NEI Weapon Upgrades (per-weapon "upgrade" items)
 *
 * Each upgrade requires the BASE weapon to be owned (a true upgrade), and once
 * obtained replaces the base weapon with a stronger variant:
 *
 *   Hammer  → Iron Knuckle's Axe   (double damage/reach + tomahawk throw)
 *   Kokiri  → Razor Sword          (progressive level 1)
 *   Kokiri  → Gilded Sword         (progressive level 2)
 *   Master  → True Master Sword
 *   Biggoron→ Great Fairy's Sword
 *
 * Persistence: gSaveContext.ship.weaponUpgrades is a u8 bitfield where each bit
 * is one upgrade. The Kokiri chain uses two bits (Razor then Gilded) so the
 * progressive item can be shuffled / granted step by step.
 *
 * Only the Hammer upgrade has gameplay behavior for now (lives in
 * mods/equipment/behaviors/equip_ikaxe.c, driven by WeaponUpgrade_HasHammerAxe).
 * The sword upgrades are reachable plumbing (give/logic/menu) with behavior TBD.
 */
#ifndef WEAPON_UPGRADES_H
#define WEAPON_UPGRADES_H

#include "z64.h"

#define WEAPON_UPGRADE_HAMMER_AXE      (1 << 0) // Hammer  → Iron Knuckle's Axe
#define WEAPON_UPGRADE_KOKIRI_RAZOR    (1 << 1) // Kokiri  → Razor Sword
#define WEAPON_UPGRADE_KOKIRI_GILDED   (1 << 2) // Kokiri  → Gilded Sword
#define WEAPON_UPGRADE_MASTER_TRUE     (1 << 3) // Master  → True Master Sword
#define WEAPON_UPGRADE_BGS_GREAT_FAIRY (1 << 4) // Biggoron→ Great Fairy's Sword
#define WEAPON_UPGRADE_ALL                                                                       \
    (WEAPON_UPGRADE_HAMMER_AXE | WEAPON_UPGRADE_KOKIRI_RAZOR | WEAPON_UPGRADE_KOKIRI_GILDED |    \
     WEAPON_UPGRADE_MASTER_TRUE | WEAPON_UPGRADE_BGS_GREAT_FAIRY)

#ifdef __cplusplus
extern "C" {
#endif

// Bit-level queries — return 1 if the specific upgrade is owned.
u8 WeaponUpgrade_HasHammerAxe(void);
u8 WeaponUpgrade_HasRazor(void);
u8 WeaponUpgrade_HasGilded(void);
u8 WeaponUpgrade_HasTrueMaster(void);
u8 WeaponUpgrade_HasGreatFairy(void);

// Returns the highest Kokiri Sword upgrade level: 0 = none, 1 = Razor, 2 = Gilded.
u8 WeaponUpgrade_KokiriLevel(void);

// Per-bit setters — set or clear a single upgrade (used by the save-flag UI).
void WeaponUpgrade_SetHammerAxe(u8 on);
void WeaponUpgrade_SetRazor(u8 on);
void WeaponUpgrade_SetGilded(u8 on);
void WeaponUpgrade_SetTrueMaster(u8 on);
void WeaponUpgrade_SetGreatFairy(u8 on);

// Progressive Kokiri Sword give: first call grants Razor, second grants Gilded.
void WeaponUpgrade_GiveProgressiveKokiri(void);

// Grant the full set at once (debug convenience). Idempotent.
void WeaponUpgrade_GrantAll(void);

#ifdef __cplusplus
}
#endif

#endif // WEAPON_UPGRADES_H
