#include "SeasonWeather.h"
#include "GlobalOutdoorRain.h"
#include "GlobalOutdoorRainBridge.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "libultraship/bridge/consolevariablebridge.h"
#include "z64.h"
#include "macros.h"
#include "mods/extended_inventory.h"
#include "overlays/actors/ovl_En_Weather_Tag/z_en_weather_tag.h"
#include <cstdint>

extern "C" {
#include "variables.h"
extern PlayState* gPlayState;
}

namespace {
constexpr int kLogicHz = 20;
constexpr int kRainDensity = 30;
PlayState* sPlay = nullptr;
int sScene = -1;
int sRoom = -1;
int sSeason = -1;
int sRainMode = -2;
uint32_t sLastFrame = UINT32_MAX;
// Presentation randomness is private: seasonal showers must not advance the
// game's actor/loot RNG, time of day, story progress or saved Rod state.
uint32_t sRandom = 0x534F4857;
GlobalOutdoorRainCycle sRain = { GlobalOutdoorRainPhase::Dry, 0, 0.0f };

struct Shade {
    s16 light[3];
    s16 ambient[3];
};
constexpr Shade kAutumnShade = { { -34, -32, -26 }, { -42, -40, -34 } };
constexpr Shade kWinterShade = { { -46, -34, -14 }, { -54, -42, -20 } };

int RandomFrames(int base, int range) {
    sRandom = sRandom * 1664525u + 1013904223u;
    return base + static_cast<int>((sRandom >> 8) % static_cast<uint32_t>(range));
}

int RainMode(int season) {
    int mode = season == SEASON_SPRING ? CVarGetInteger(SOH_SEASON_WEATHER_CVAR("Spring.RainMode"), -1)
                                       : CVarGetInteger(SOH_SEASON_WEATHER_CVAR("Autumn.RainMode"), -1);
    return mode >= -1 && mode <= 2 ? mode : -1;
}

bool HasNativeSkyWeather(const PlayState* play) {
    for (const Actor* actor = play->actorCtx.actorLists[ACTORCAT_PROP].head; actor != nullptr; actor = actor->next) {
        // Cloud, snow, Lake Hylia rain and Kakariko storms all gate their native
        // activation/restoration on a clear overcast handshake. Sandstorm and
        // graveyard proximity rain use separate native paths. Retired/killed
        // actors are intentionally excluded, preserving saved Rod gameplay.
        if (actor->id == ACTOR_EN_WEATHER_TAG && actor->update != nullptr &&
            (actor->params & 0xF) <= EN_WEATHER_TAG_TYPE_THUNDERSTORM_KAKARIKO) {
            return true;
        }
    }
    return false;
}

} // namespace

extern "C" int32_t SeasonWeather_SeasonForPlay(const PlayState* play) {
    if (play == nullptr || play->envCtx.indoors || play->envCtx.skyboxDisabled || play->envCtx.customSkyboxFilter ||
        play->csCtx.state != CS_STATE_IDLE || play->gameOverCtx.state != GAMEOVER_INACTIVE ||
        gSaveContext.gameMode != GAMEMODE_NORMAL) {
        return -1;
    }
    const Camera* camera = GET_ACTIVE_CAM(play);
    if (camera == nullptr || (camera->unk_14C & 0x100)) {
        return -1;
    }
    const int selection = CVarGetInteger(SOH_SEASON_WEATHER_CVAR("Season"), -2);
    if (selection != -2) {
        // In particular, do not call the self-healing gameplay getter for a
        // manual selection: it can write the saved Rod season.
        return selection >= 0 && selection < SEASON_COUNT ? selection : -1;
    }
    if (Seasons_SeasonCount() == 0) {
        return -1;
    }
    const uint8_t season = Seasons_GetSeason();
    return season < SEASON_COUNT ? season : -1;
}

extern "C" int32_t SeasonWeather_LightOffset(const PlayState* play, int32_t channel, int32_t ambient) {
    if (channel < 0 || channel >= 3 || gLightningStrike.state != LIGHTNING_STRIKE_WAIT) {
        return 0;
    }
    const int season = SeasonWeather_SeasonForPlay(play);
    const Shade* shade = season == SEASON_AUTUMN ? &kAutumnShade : season == SEASON_WINTER ? &kWinterShade : nullptr;
    return shade == nullptr ? 0 : (ambient ? shade->ambient[channel] : shade->light[channel]);
}

extern "C" int32_t SeasonWeather_ParticlesEnabled(int32_t season) {
    switch (season) {
        case SEASON_SPRING:
            return CVarGetInteger(SOH_SEASON_WEATHER_CVAR("Spring.Blossoms"), 1) != 0;
        case SEASON_AUTUMN:
            return CVarGetInteger(SOH_SEASON_WEATHER_CVAR("Autumn.Leaves"), 1) != 0;
        case SEASON_WINTER:
            return CVarGetInteger(SOH_SEASON_WEATHER_CVAR("Winter.Snow"), 1) != 0;
        default:
            return false;
    }
}

extern "C" int32_t SeasonWeather_FlurriesEnabled() {
    return SeasonWeather_ParticlesEnabled(SEASON_WINTER) &&
           CVarGetInteger(SOH_SEASON_WEATHER_CVAR("Winter.Flurries"), 0) != 0;
}

extern "C" int32_t SeasonWeather_SummerEffectEnabled(int32_t effect) {
    if (!CVarGetInteger(SOH_SEASON_WEATHER_CVAR("Summer.Enabled"), 1)) {
        return false;
    }
    switch (effect) {
        case 0:
            return CVarGetInteger(SOH_SEASON_WEATHER_CVAR("Summer.Fluff"), 1) != 0;
        case 1:
            return CVarGetInteger(SOH_SEASON_WEATHER_CVAR("Summer.Fireflies"), 1) != 0;
        case 2:
            return CVarGetInteger(SOH_SEASON_WEATHER_CVAR("Summer.Sunbeams"), 0) != 0;
        default:
            return false;
    }
}

extern "C" int32_t SeasonWeather_WinterEffectEnabled(int32_t effect) {
    switch (effect) {
        case 0:
            return CVarGetInteger(SOH_SEASON_WEATHER_CVAR("Winter.Mist"), 1) != 0;
        case 1:
            return CVarGetInteger(SOH_SEASON_WEATHER_CVAR("Winter.Aurora"), 1) != 0;
        default:
            return false;
    }
}

extern "C" void SeasonWeather_Reset() {
    sPlay = nullptr;
    sScene = sRoom = sSeason = -1;
    sRainMode = -2;
    sLastFrame = UINT32_MAX;
    sRandom = 0x534F4857;
    sRain = { GlobalOutdoorRainPhase::Dry, 0, 0.0f };
    GlobalOutdoorRain_SetSeasonRequest(nullptr, 0, 0.0f, false);
}

extern "C" void SeasonWeather_Update(PlayState* play) {
    if (play == nullptr) {
        SeasonWeather_Reset();
        return;
    }
    if (sPlay != play || sScene != play->sceneNum || sRoom != play->roomCtx.curRoom.num) {
        SeasonWeather_Reset();
        sPlay = play;
        sScene = play->sceneNum;
        sRoom = play->roomCtx.curRoom.num;
    }
    const int season = SeasonWeather_SeasonForPlay(play);
    const bool rainySeason = season == SEASON_SPRING || season == SEASON_AUTUMN;
    const int mode = rainySeason ? RainMode(season) : 0;
    if (season != sSeason || mode != sRainMode) {
        sSeason = season;
        sRainMode = mode;
        sRain = { GlobalOutdoorRainPhase::Dry, mode == 2 ? RandomFrames(2 * kLogicHz, 3 * kLogicHz) : 0, 0.0f };
        // SoH's original Autumn weather is thirty steady drops immediately.
        // Spring's original default is dry; these defaults are independent of MM's days.
        if (season == SEASON_AUTUMN && mode == -1) {
            sRain = { GlobalOutdoorRainPhase::Sustain, 0, 1.0f };
        }
    }
    const bool activeRain = rainySeason && (mode > 0 || (season == SEASON_AUTUMN && mode == -1));
    if (!activeRain) {
        sRain = { GlobalOutdoorRainPhase::Dry, 0, 0.0f };
    } else if (sLastFrame != play->state.frames && play->pauseCtx.state == 0 && play->pauseCtx.debugState == 0) {
        const auto oldPhase = sRain.phase;
        GlobalOutdoorRain_AdvanceCycle(
            sRain, mode == 2 ? GlobalOutdoorRainMode::Intermittent : GlobalOutdoorRainMode::Persistent, true,
            20 * kLogicHz, 8 * kLogicHz, 1.0f / kLogicHz);
        if (mode == 2 && oldPhase != sRain.phase) {
            if (sRain.phase == GlobalOutdoorRainPhase::Dry) {
                sRain.framesRemaining = RandomFrames(20 * kLogicHz, 6 * kLogicHz);
            } else if (sRain.phase == GlobalOutdoorRainPhase::Sustain) {
                sRain.framesRemaining = RandomFrames(8 * kLogicHz, 7 * kLogicHz);
            }
        }
    }
    sLastFrame = play->state.frames;
    const bool contributesSky = season == SEASON_AUTUMN || season == SEASON_WINTER || activeRain;
    GlobalOutdoorRain_SetSeasonSkyAllowed(play, !contributesSky || !HasNativeSkyWeather(play));
    GlobalOutdoorRain_SetSeasonRequest(play, GlobalOutdoorRain_ScaleDensity(kRainDensity, sRain.intensity),
                                       sRain.intensity, season == SEASON_AUTUMN || season == SEASON_WINTER);
}

static void RegisterSeasonWeather() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(
        []() { SeasonWeather_Update(gPlayState); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDestroy>([]() { SeasonWeather_Reset(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnExitGame>([](int32_t) { SeasonWeather_Reset(); });
}

static RegisterShipInitFunc initSeasonWeather(RegisterSeasonWeather);
