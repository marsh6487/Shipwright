#include "HyruleFieldNightMusic.h"

HyruleFieldNightMusicDecision HyruleFieldNightMusic_Select(const HyruleFieldNightMusicState& state) {
    if (!state.inHyruleField) {
        return state.ownsNightBgm ? HyruleFieldNightMusicDecision::StopNight
                                : HyruleFieldNightMusicDecision::NoChange;
    }
    if (state.explicitAudioOverride) {
        return HyruleFieldNightMusicDecision::NoChange;
    }
    if (!state.enabled) {
        return state.ownsNightBgm ? HyruleFieldNightMusicDecision::StopNight
                                : HyruleFieldNightMusicDecision::NoChange;
    }
    if (!state.isNight) {
        return state.ownsNightBgm ? HyruleFieldNightMusicDecision::StopNightAndRestoreDay
                                  : HyruleFieldNightMusicDecision::NoChange;
    }
    return state.ownsNightBgm && state.nightBgmPlaying ? HyruleFieldNightMusicDecision::NoChange
                                                       : HyruleFieldNightMusicDecision::StartNight;
}

uint16_t HyruleFieldNightMusic_ValidateSequence(uint16_t selected, bool isValid, uint16_t fallback) {
    return isValid ? selected : fallback;
}

#ifndef HYRULE_FIELD_NIGHT_MUSIC_TEST
#include "AudioCollection.h"
#include "AudioEditor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"

extern "C" {
#include "functions.h"
#include "sequence.h"
#include "variables.h"
#include "z64.h"
extern PlayState* gPlayState;
}

static constexpr uint16_t kDefaultNightSequence = NA_BGM_KAKARIKO_ADULT;
static bool sOwnsNightBgm = false;
static uint16_t sNightPlaybackSeq = NA_BGM_DISABLED;

static bool HasExplicitAudioOverride(PlayState* play) {
    if (play->csCtx.state != CS_STATE_IDLE || Player_InCsMode(play)) {
        return true;
    }

    const uint16_t mainSeq = func_800FA0B4(SEQ_PLAYER_BGM_MAIN);
    const uint16_t subSeq = func_800FA0B4(SEQ_PLAYER_BGM_SUB);
    const uint16_t fanfareSeq = func_800FA0B4(SEQ_PLAYER_FANFARE);
    const bool mainIsFieldAmbience = mainSeq == NA_BGM_NATURE_AMBIENCE ||
                                     (mainSeq & 0xFF) == NA_BGM_FIELD_LOGIC || mainSeq == NA_BGM_DISABLED;
    const bool subIsNightTrack = sOwnsNightBgm && (subSeq & 0xFF) == (sNightPlaybackSeq & 0xFF);
    return !mainIsFieldAmbience || (subSeq != NA_BGM_DISABLED && !subIsNightTrack) ||
           fanfareSeq != NA_BGM_DISABLED;
}

void HyruleFieldNightMusic_Update(PlayState* play) {
    if (play == nullptr) {
        HyruleFieldNightMusic_Reset();
        return;
    }

    const uint16_t subSeq = func_800FA0B4(SEQ_PLAYER_BGM_SUB);
    const HyruleFieldNightMusicState state = {
        .enabled = CVarGetInteger(CVAR_AUDIO("HyruleFieldNightMusic"), 0) != 0,
        .inHyruleField = play->sceneNum == SCENE_HYRULE_FIELD,
        .isNight = gSaveContext.nightFlag != 0,
        .ownsNightBgm = sOwnsNightBgm,
        .nightBgmPlaying = sOwnsNightBgm && (subSeq & 0xFF) == (sNightPlaybackSeq & 0xFF),
        .explicitAudioOverride = HasExplicitAudioOverride(play),
    };

    switch (HyruleFieldNightMusic_Select(state)) {
        case HyruleFieldNightMusicDecision::StartNight: {
            const uint16_t selected = static_cast<uint16_t>(
                CVarGetInteger(CVAR_AUDIO("HyruleFieldNightSequence"), kDefaultNightSequence));
            const uint16_t valid = HyruleFieldNightMusic_ValidateSequence(
                selected, AudioCollection::Instance->HasSequenceNum(selected), kDefaultNightSequence);
            sNightPlaybackSeq = AudioEditor_GetReplacementSeq(valid);
            Audio_QueueSeqCmd((SEQ_PLAYER_BGM_SUB << 24) | (0x1E << 16) | sNightPlaybackSeq);
            sOwnsNightBgm = true;
            break;
        }
        case HyruleFieldNightMusicDecision::StopNight:
            Audio_QueueSeqCmd((0x1 << 28) | (SEQ_PLAYER_BGM_SUB << 24) | (0x1E << 16) | 0xFF);
            sOwnsNightBgm = false;
            sNightPlaybackSeq = NA_BGM_DISABLED;
            break;
        case HyruleFieldNightMusicDecision::StopNightAndRestoreDay:
            Audio_QueueSeqCmd((0x1 << 28) | (SEQ_PLAYER_BGM_SUB << 24) | (0x1E << 16) | 0xFF);
            sOwnsNightBgm = false;
            sNightPlaybackSeq = NA_BGM_DISABLED;
            Audio_PlaySceneSequence(play->sequenceCtx.seqId);
            break;
        case HyruleFieldNightMusicDecision::NoChange:
            break;
    }
}

void HyruleFieldNightMusic_Reset() {
    sOwnsNightBgm = false;
    sNightPlaybackSeq = NA_BGM_DISABLED;
}

static void RegisterHyruleFieldNightMusic() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>([]() {
        HyruleFieldNightMusic_Update(gPlayState);
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDestroy>([]() {
        HyruleFieldNightMusic_Reset();
    });
}

static RegisterShipInitFunc initFunc(RegisterHyruleFieldNightMusic,
                                     { CVAR_AUDIO("HyruleFieldNightMusic"), CVAR_AUDIO("HyruleFieldNightSequence") });
#endif
