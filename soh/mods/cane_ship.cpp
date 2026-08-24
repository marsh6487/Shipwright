/**
 * cane_ship.cpp — the Shadow Temple ferry, under Ultrahand's control.
 *
 * The ferry is the one actor in the Ultrahand trait table whose "movement" cannot be a position.
 * Nothing in BgHakaShip_Move recomputes world.pos.x, so it looks writable — but its progress down
 * the corridor is a speed that ramps 0 -> 4.0 at 0.2 a frame and is integrated by
 * Actor_MoveXZGravity, and the crash at the far end is triggered by distance from home rather
 * than by a timer. Teleporting it would skip straight past that trigger.
 *
 * So instead of moving it, the cane decides WHETHER IT MOVES. SoH already routes the speed ramp
 * through a vanilla-behavior hook for the FasterShadowShip enhancement:
 *
 *     GameInteractor_Should(VB_SHADOW_SHIP_SET_SPEED, true, this, play)   // z_bg_haka_ship.c
 *
 * Returning false there suppresses the ramp. Zeroing speedXZ at the same time makes it coast to a
 * halt rather than drift, and because the paddlewheel's spin is derived from speedXZ the wheel
 * stops with it — the boat looks moored rather than frozen. Let go and it picks up again.
 *
 * This lives in its own translation unit because it is C++ and the rest of the cane is C. The
 * only thing it asks the cane is one bool.
 */

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "functions.h"
#include "src/overlays/actors/ovl_Bg_Haka_Ship/z_bg_haka_ship.h"

/** Is Ultrahand holding this exact actor right now? Defined in cane_pacci.c. */
u8 Pacci_IsDriving(Actor* actor);
}

void RegisterCaneShadowShip() {
    // Unconditional: the hook has to be live whenever the cane might be, and it does nothing at all
    // unless Ultrahand is actually holding this ship. FasterShadowShip's own registration is gated
    // on its CVar and the two coexist - if both fire, this one runs second and the stop wins, which
    // is the right way round for a player deliberately holding the boat still.
    COND_VB_SHOULD(VB_SHADOW_SHIP_SET_SPEED, true, {
        BgHakaShip* ship = va_arg(args, BgHakaShip*);
        PlayState* play = va_arg(args, PlayState*);

        (void)play;
        if (!Pacci_IsDriving(&ship->dyna.actor)) {
            // Not being driven: no ramp, and shed whatever speed it still had.
            *should = false;
            ship->dyna.actor.speedXZ = 0.0f;
        }
    });
}

static RegisterShipInitFunc initCaneShadowShip(RegisterCaneShadowShip, {});
