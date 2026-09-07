#pragma once
#include <cstdint>
#include "libultraship/bridge/consolevariablebridge.h"

struct Color_RGB8 {
    uint8_t r, g, b;
};
struct PlayState {
    struct {
        uint8_t indoors;
        uint8_t unk_EE[4];
    } envCtx;
};
extern "C" {
Color_RGB8 CVarGetColor24(const char*, Color_RGB8);
int16_t Rand_S16Offset(int16_t base, int16_t range);
uint8_t Audio_IsNatureRainEnabled(void);
}

// Hook registration is the external boundary; tests drive Update/Reset directly.
struct GameInteractor {
    struct OnGameFrameUpdate {};
    struct OnPlayDestroy {};
    static GameInteractor* Instance;
    template <class Hook, class Callback> void RegisterGameHook(Callback) {
    }
};
