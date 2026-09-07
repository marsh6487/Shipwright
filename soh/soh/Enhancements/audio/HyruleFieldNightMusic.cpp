#include "HyruleFieldNightMusicInternal.h"

HyruleFieldNightMusicDecision HyruleFieldNightMusic_Select(const HyruleFieldNightMusicState& state) {
    if (!state.enabled || !state.inHyruleField) {
        return state.ownsNightBgm ? HyruleFieldNightMusicDecision::StopNightRestoreDay
                                  : HyruleFieldNightMusicDecision::NoChange;
    }
    if (state.explicitAudioOverride || state.fanfarePlaying) {
        return HyruleFieldNightMusicDecision::NoChange;
    }
    if (!state.isNight) {
        return state.ownsNightBgm ? HyruleFieldNightMusicDecision::StopNightRestoreDay
                                  : HyruleFieldNightMusicDecision::NoChange;
    }
    if (!state.ownsNightBgm) {
        return HyruleFieldNightMusicDecision::StartNight;
    }
    return state.nightBgmPlaying ? HyruleFieldNightMusicDecision::NoChange
                                 : HyruleFieldNightMusicDecision::RestoreNight;
}

uint16_t HyruleFieldNightMusic_ResolveSequence(uint16_t selected, uint16_t replacement) {
    (void)selected;
    return replacement;
}

uint16_t HyruleFieldNightMusic_ValidateSequence(uint16_t selected, bool isValid, uint16_t fallback) {
    return isValid ? selected : fallback;
}

static constexpr uint16_t kDisabledSequence = 0xFFFF;
static uint16_t sNightPlaybackSeq = kDisabledSequence;

uint16_t HyruleFieldNightMusic_StartSequence(uint16_t selected, uint16_t fallback,
                                             HyruleFieldNightMusicSequenceValidator isValid,
                                             HyruleFieldNightMusicSequenceResolver getReplacement) {
    const uint16_t valid = HyruleFieldNightMusic_ValidateSequence(selected, isValid(selected), fallback);
    const uint16_t replacement = HyruleFieldNightMusic_ResolveSequence(valid, getReplacement(valid));
    sNightPlaybackSeq = HyruleFieldNightMusic_ValidateSequence(replacement, isValid(replacement), valid);
    return sNightPlaybackSeq;
}

uint16_t HyruleFieldNightMusic_RestoreSequence() {
    return sNightPlaybackSeq;
}

void HyruleFieldNightMusic_ClearSequence() {
    sNightPlaybackSeq = kDisabledSequence;
}

bool HyruleFieldNightMusic_ShouldRestoreDaySequence(const HyruleFieldNightMusicState& state) {
    return state.enabled && state.inHyruleField && !state.isNight && state.ownsNightBgm &&
           !state.explicitAudioOverride && !state.fanfarePlaying;
}

#ifndef HYRULE_FIELD_NIGHT_MUSIC_TEST
#include "AudioCollection.h"
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

static bool IsSequenceValid(uint16_t sequence) {
    return AudioCollection::Instance->HasSequenceNum(sequence);
}

static uint16_t GetReplacementSequence(uint16_t sequence) {
    return AudioCollection::Instance->GetReplacementSequence(sequence);
}

static bool HasExplicitAudioOverride(PlayState* play) {
    if (play->csCtx.state != CS_STATE_IDLE || Player_InCsMode(play)) {
        return true;
    }

    const uint16_t mainSeq = func_800FA0B4(SEQ_PLAYER_BGM_MAIN);
    const uint16_t subSeq = func_800FA0B4(SEQ_PLAYER_BGM_SUB);
    const bool mainIsFieldLifecycle =
        mainSeq == NA_BGM_NATURE_AMBIENCE || (mainSeq & 0xFF) == NA_BGM_FIELD_LOGIC || mainSeq == NA_BGM_DISABLED;
    const bool subIsNightTrack = sOwnsNightBgm && (subSeq & 0xFF) == (sNightPlaybackSeq & 0xFF);
    return !mainIsFieldLifecycle || (subSeq != NA_BGM_DISABLED && !subIsNightTrack);
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
        .fanfarePlaying = func_800FA0B4(SEQ_PLAYER_FANFARE) != NA_BGM_DISABLED,
        .explicitAudioOverride = HasExplicitAudioOverride(play),
    };

    switch (HyruleFieldNightMusic_Select(state)) {
        case HyruleFieldNightMusicDecision::StartNight: {
            const uint16_t selected =
                static_cast<uint16_t>(CVarGetInteger(CVAR_AUDIO("HyruleFieldNightSequence"), kDefaultNightSequence));
            const uint16_t playbackSequence = HyruleFieldNightMusic_StartSequence(
                selected, kDefaultNightSequence, IsSequenceValid, GetReplacementSequence);
            Audio_QueueResolvedSeqCmd(SEQ_PLAYER_BGM_SUB, playbackSequence, 0x1E);
            sOwnsNightBgm = true;
            break;
        }
        case HyruleFieldNightMusicDecision::RestoreNight:
            Audio_QueueResolvedSeqCmd(SEQ_PLAYER_BGM_SUB, HyruleFieldNightMusic_RestoreSequence(), 0x1E);
            break;
        case HyruleFieldNightMusicDecision::StopNightRestoreDay: {
            Audio_QueueSeqCmd((0x1 << 28) | (SEQ_PLAYER_BGM_SUB << 24) | (0x1E << 16) | 0xFF);
            if (HyruleFieldNightMusic_ShouldRestoreDaySequence(state)) {
                const uint16_t replacement = AudioCollection::Instance->GetReplacementSequence(NA_BGM_FIELD_LOGIC);
                const uint16_t daySequence = HyruleFieldNightMusic_ValidateSequence(
                    replacement, AudioCollection::Instance->HasSequenceNum(replacement), NA_BGM_FIELD_LOGIC);
                Audio_QueueResolvedSeqCmd(SEQ_PLAYER_BGM_MAIN, daySequence, 0x1E);
            }
            sOwnsNightBgm = false;
            HyruleFieldNightMusic_ClearSequence();
            break;
        }
        case HyruleFieldNightMusicDecision::NoChange:
            break;
    }
}

void HyruleFieldNightMusic_Reset() {
    sOwnsNightBgm = false;
    HyruleFieldNightMusic_ClearSequence();
}

static void RegisterHyruleFieldNightMusic() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(
        []() { HyruleFieldNightMusic_Update(gPlayState); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDestroy>([]() { HyruleFieldNightMusic_Reset(); });
}

static RegisterShipInitFunc initFunc(RegisterHyruleFieldNightMusic,
                                     { CVAR_AUDIO("HyruleFieldNightMusic"), CVAR_AUDIO("HyruleFieldNightSequence") });
#endif
