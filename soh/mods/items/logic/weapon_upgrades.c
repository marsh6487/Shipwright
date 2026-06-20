/**
 * weapon_upgrades.c - NEI Weapon Upgrade bit accessors.
 *
 * This translation unit just owns the gSaveContext.ship.weaponUpgrades bit
 * accessors so other code (randomizer give/logic, the menu, the IK Axe hammer
 * behavior) doesn't need to know the field layout.
 *
 * #included into mods/items/logic/custom_items.c (the host TU pulled in by
 * z_player.c) — the CMake mods glob only compiles *.cpp/*.h, not *.c.
 */
#include "weapon_upgrades.h"

u8 WeaponUpgrade_HasHammerAxe(void) {
    return (gSaveContext.ship.weaponUpgrades & WEAPON_UPGRADE_HAMMER_AXE) != 0;
}

u8 WeaponUpgrade_HasRazor(void) {
    return (gSaveContext.ship.weaponUpgrades & WEAPON_UPGRADE_KOKIRI_RAZOR) != 0;
}

u8 WeaponUpgrade_HasGilded(void) {
    return (gSaveContext.ship.weaponUpgrades & WEAPON_UPGRADE_KOKIRI_GILDED) != 0;
}

u8 WeaponUpgrade_HasTrueMaster(void) {
    return (gSaveContext.ship.weaponUpgrades & WEAPON_UPGRADE_MASTER_TRUE) != 0;
}

u8 WeaponUpgrade_HasGreatFairy(void) {
    return (gSaveContext.ship.weaponUpgrades & WEAPON_UPGRADE_BGS_GREAT_FAIRY) != 0;
}

u8 WeaponUpgrade_KokiriLevel(void) {
    if (gSaveContext.ship.weaponUpgrades & WEAPON_UPGRADE_KOKIRI_GILDED) {
        return 2;
    }
    if (gSaveContext.ship.weaponUpgrades & WEAPON_UPGRADE_KOKIRI_RAZOR) {
        return 1;
    }
    return 0;
}

static void WeaponUpgrade_SetBit(u8 bit, u8 on) {
    if (on) {
        gSaveContext.ship.weaponUpgrades |= bit;
    } else {
        gSaveContext.ship.weaponUpgrades &= ~bit;
    }
}

void WeaponUpgrade_SetHammerAxe(u8 on) {
    WeaponUpgrade_SetBit(WEAPON_UPGRADE_HAMMER_AXE, on);
}

void WeaponUpgrade_SetRazor(u8 on) {
    WeaponUpgrade_SetBit(WEAPON_UPGRADE_KOKIRI_RAZOR, on);
}

void WeaponUpgrade_SetGilded(u8 on) {
    WeaponUpgrade_SetBit(WEAPON_UPGRADE_KOKIRI_GILDED, on);
}

void WeaponUpgrade_SetTrueMaster(u8 on) {
    WeaponUpgrade_SetBit(WEAPON_UPGRADE_MASTER_TRUE, on);
}

void WeaponUpgrade_SetGreatFairy(u8 on) {
    WeaponUpgrade_SetBit(WEAPON_UPGRADE_BGS_GREAT_FAIRY, on);
}

void WeaponUpgrade_GiveProgressiveKokiri(void) {
    // First give → Razor, second give → Gilded. Gilded implies Razor was earned.
    if (!(gSaveContext.ship.weaponUpgrades & WEAPON_UPGRADE_KOKIRI_RAZOR)) {
        gSaveContext.ship.weaponUpgrades |= WEAPON_UPGRADE_KOKIRI_RAZOR;
    } else {
        gSaveContext.ship.weaponUpgrades |= WEAPON_UPGRADE_KOKIRI_GILDED;
    }
}

void WeaponUpgrade_GrantAll(void) {
    gSaveContext.ship.weaponUpgrades |= WEAPON_UPGRADE_ALL;
}
