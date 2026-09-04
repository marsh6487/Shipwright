#include "AudioEditor.h"
#include "sequence.h"

#include <map>
#include <set>
#include <string>
#include <functions.h>
#include "soh/ShipUtils.h"
#include "soh/OTRGlobals.h"
#include "soh/cvar_prefixes.h"
#include <ship/utils/StringHelper.h>
#include "soh/SohGui/SohMenu.h"
#include "soh/SohGui/SohGui.hpp"
#include "AudioCollection.h"
#include "soh/Enhancements/enhancementTypes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "../../../src/code/concurrent_weather_audio.h"

extern "C" {
#include "z64save.h"
extern SaveContext gSaveContext;
}

Vec3f pos = { 0.0f, 0.0f, 0.0f };
f32 freqScale = 1.0f;
s8 reverbAdd = 0;

using namespace UIWidgets;

static WidgetInfo lowHpAlarm;
static WidgetInfo naviCall;
static WidgetInfo enemyProx;
static WidgetInfo leeverProx;
static WidgetInfo leadingMusic;
static WidgetInfo displaySeqName;
static WidgetInfo ovlDuration;
static WidgetInfo voicePitch;
static WidgetInfo randomAudioGenModes;
static WidgetInfo lowerOctaves;
static WidgetInfo proximityWeatherThunder;
static WidgetInfo proximityWeatherThunderStyle;
static WidgetInfo proximityWeatherRainVolume;
static WidgetInfo proximityWeatherThunderVolume;

namespace SohGui {
extern std::shared_ptr<SohMenu> mSohMenu;
}

// Authentic sequence counts
// used to ensure we have enough to shuffle
#define SEQ_COUNT_BGM_WORLD 30
#define SEQ_COUNT_BGM_BATTLE 6
#define SEQ_COUNT_FANFARE 15
#define SEQ_COUNT_OCARINA 12
#define SEQ_COUNT_NOSHUFFLE 6
#define SEQ_COUNT_BGM_EVENT 17
#define SEQ_COUNT_INSTRUMENT 6
#define SEQ_COUNT_SFX 57
#define SEQ_COUNT_VOICE 108
#define SEQ_COUNT_ENDING 5

size_t AuthenticCountBySequenceType(SeqType type) {
    switch (type) {
        case SEQ_NOSHUFFLE:
            return SEQ_COUNT_NOSHUFFLE;
        case SEQ_BGM_WORLD:
            return SEQ_COUNT_BGM_WORLD;
        case SEQ_BGM_EVENT:
            return SEQ_COUNT_BGM_EVENT;
        case SEQ_BGM_BATTLE:
            return SEQ_COUNT_BGM_BATTLE;
        case SEQ_OCARINA:
            return SEQ_COUNT_OCARINA;
        case SEQ_FANFARE:
            return SEQ_COUNT_FANFARE;
        case SEQ_SFX:
            return SEQ_COUNT_SFX;
        case SEQ_INSTRUMENT:
            return SEQ_COUNT_INSTRUMENT;
        case SEQ_VOICE:
            return SEQ_COUNT_VOICE;
        case SEQ_ENDING:
            return SEQ_COUNT_ENDING;
        default:
            return 0;
    }
}

static const std::map<int32_t, const char*> audioRandomizerModes = {
    { RANDOMIZE_OFF, "Manual" },
    { RANDOMIZE_ON_NEW_SCENE, "On New Scene" },
    { RANDOMIZE_ON_RANDO_GEN_ONLY, "On Rando Gen Only" },
    { RANDOMIZE_ON_FILE_LOAD, "On File Load" },
    { RANDOMIZE_ON_FILE_LOAD_SEEDED, "On File Load (Seeded)" },
};

static const std::map<int32_t, const char*> proximityWeatherThunderStyles = {
    { CONCURRENT_WEATHER_THUNDER_LOW, "Low Thunder" },
    { CONCURRENT_WEATHER_THUNDER_LAYERED, "Layered Thunder" },
};

// Grabs the current BGM sequence ID and replays it
// which will lookup the proper override, or reset back to vanilla
void ReplayCurrentBGM() {
    u16 curSeqId = func_800FA0B4(SEQ_PLAYER_BGM_MAIN);
    // TODO: replace with Audio_StartSeq when the macro is shared
    // The fade time and audio player flags will always be 0 in the case of replaying the BGM, so they are not set here
    Audio_QueueSeqCmd(0x00000000 | curSeqId);
}

// Attempt to update the BGM if it matches the current sequence that is being played
// The seqKey that is passed in should be the vanilla ID, not the override ID
void UpdateCurrentBGM(u16 seqKey, SeqType seqType) {
    if (seqType != SEQ_BGM_WORLD) {
        return;
    }

    u16 curSeqId = func_800FA0B4(SEQ_PLAYER_BGM_MAIN);
    if (curSeqId == seqKey) {
        ReplayCurrentBGM();
    }
}

void RandomizeGroup(SeqType type, bool manual = true) {
    std::vector<u16> values;

    uint64_t localRngState = 0;
    uint64_t* shuffleState = nullptr;

    if (!manual) {
        int randomizeMode = CVarGetInteger(CVAR_AUDIO("RandomizeAudioGenModes"), 0);
        if (randomizeMode == RANDOMIZE_ON_FILE_LOAD_SEEDED || randomizeMode == RANDOMIZE_ON_RANDO_GEN_ONLY) {

            uint32_t finalSeed = type + (IS_RANDO ? Rando::Context::GetInstance()->GetSeed()
                                                  : static_cast<uint32_t>(gSaveContext.ship.stats.fileCreatedAt));
            ShipUtils::RandInit(finalSeed, &localRngState);
            shuffleState = &localRngState;
        }
        // For RANDOMIZE_ON_NEW_SCENE, shuffleState remains nullptr, which uses the global RNG
    }

    // An empty IncludedSequences set means that the AudioEditor window has never been drawn
    if (AudioCollection::Instance->GetIncludedSequences().empty()) {
        AudioCollection::Instance->InitializeShufflePool();
    }

    // use a while loop to add duplicates if we don't have enough included sequences
    while (values.size() < AuthenticCountBySequenceType(type)) {
        for (const auto& seqData : AudioCollection::Instance->GetIncludedSequences()) {
            if (seqData->category & type && seqData->canBeUsedAsReplacement) {
                values.push_back(seqData->sequenceId);
            }
        }

        // if we didn't find any, return early without shuffling to prevent an infinite loop
        if (!values.size())
            return;
    }
    ShipUtils::Shuffle(values, shuffleState);
    for (const auto& [seqId, seqData] : AudioCollection::Instance->GetAllSequences()) {
        const std::string cvarKey = AudioCollection::Instance->GetCvarKey(seqData.sfxKey);
        const std::string cvarLockKey = AudioCollection::Instance->GetCvarLockKey(seqData.sfxKey);
        // don't randomize locked entries
        if ((seqData.category & type) && CVarGetInteger(cvarLockKey.c_str(), 0) == 0) {
            // Only save authentic sequence CVars
            if ((((seqData.category & SEQ_BGM_CUSTOM) || seqData.category == SEQ_FANFARE) &&
                 seqData.sequenceId >= MAX_AUTHENTIC_SEQID) ||
                seqData.canBeReplaced == false) {
                continue;
            }
            const int randomValue = values.back();
            CVarSetInteger(cvarKey.c_str(), randomValue);
            values.pop_back();
        }
    }
}

void ResetGroup(const std::map<u16, SequenceInfo>& map, SeqType type) {
    for (const auto& [defaultValue, seqData] : map) {
        if (seqData.category == type) {
            // Only save authentic sequence CVars
            if (seqData.category == SEQ_FANFARE && defaultValue >= MAX_AUTHENTIC_SEQID) {
                continue;
            }
            const std::string cvarKey = AudioCollection::Instance->GetCvarKey(seqData.sfxKey);
            const std::string cvarLockKey = AudioCollection::Instance->GetCvarLockKey(seqData.sfxKey);
            if (CVarGetInteger(cvarLockKey.c_str(), 0) == 0) {
                CVarClear(cvarKey.c_str());
            }
        }
    }
}

void LockGroup(const std::map<u16, SequenceInfo>& map, SeqType type) {
    for (const auto& [defaultValue, seqData] : map) {
        if (seqData.category == type) {
            // Only save authentic sequence CVars
            if (seqData.category == SEQ_FANFARE && defaultValue >= MAX_AUTHENTIC_SEQID) {
                continue;
            }
            const std::string cvarKey = AudioCollection::Instance->GetCvarKey(seqData.sfxKey);
            const std::string cvarLockKey = AudioCollection::Instance->GetCvarLockKey(seqData.sfxKey);
            CVarSetInteger(cvarLockKey.c_str(), 1);
        }
    }
}

void UnlockGroup(const std::map<u16, SequenceInfo>& map, SeqType type) {
    for (const auto& [defaultValue, seqData] : map) {
        if (seqData.category == type) {
            // Only save authentic sequence CVars
            if (seqData.category == SEQ_FANFARE && defaultValue >= MAX_AUTHENTIC_SEQID) {
                continue;
            }
            const std::string cvarKey = AudioCollection::Instance->GetCvarKey(seqData.sfxKey);
            const std::string cvarLockKey = AudioCollection::Instance->GetCvarLockKey(seqData.sfxKey);
            CVarSetInteger(cvarLockKey.c_str(), 0);
        }
    }
}

void DrawPreviewButton(uint16_t sequenceId, std::string sfxKey, SeqType sequenceType) {
    const std::string cvarKey = AudioCollection::Instance->GetCvarKey(sfxKey);
    const std::string hiddenKey = "##" + cvarKey;
    const std::string stopButton = ICON_FA_STOP + hiddenKey;
    const std::string previewButton = ICON_FA_PLAY + hiddenKey;

    if (CVarGetInteger(CVAR_AUDIO("Playing"), 0) == sequenceId) {
        if (UIWidgets::Button(stopButton.c_str(), UIWidgets::ButtonOptions()
                                                      .Size(UIWidgets::Sizes::Inline)
                                                      .Padding(ImVec2(10.0f, 6.0f))
                                                      .Tooltip("Stop Preview")
                                                      .Color(THEME_COLOR))) {
            func_800F5C2C();
            CVarSetInteger(CVAR_AUDIO("Playing"), 0);
        }
    } else {
        if (UIWidgets::Button(previewButton.c_str(), UIWidgets::ButtonOptions()
                                                         .Size(UIWidgets::Sizes::Inline)
                                                         .Padding(ImVec2(10.0f, 6.0f))
                                                         .Tooltip("Play Preview")
                                                         .Color(THEME_COLOR))) {
            if (CVarGetInteger(CVAR_AUDIO("Playing"), 0) != 0) {
                func_800F5C2C();
                CVarSetInteger(CVAR_AUDIO("Playing"), 0);
            } else {
                if (sequenceType == SEQ_SFX || sequenceType == SEQ_VOICE) {
                    Audio_PlaySoundGeneral(sequenceId, &pos, 4, &freqScale, &freqScale, &reverbAdd);
                } else if (sequenceType == SEQ_INSTRUMENT) {
                    AudioOcarina_SetInstrument(sequenceId - INSTRUMENT_OFFSET);
                    AudioOcarina_SetPlaybackSong(9, 1);
                } else {
                    // TODO: Cant do both here, so have to click preview button twice
                    PreviewSequence(sequenceId);
                    CVarSetInteger(CVAR_AUDIO("Playing"), sequenceId);
                }
            }
        }
    }
}

void Draw_SfxTab(const std::string& tabId, SeqType type, const std::string& tabName) {
    const std::map<u16, SequenceInfo>& map = AudioCollection::Instance->GetAllSequences();

    const std::string hiddenTabId = "##" + tabId;
    const std::string resetAllButton = "Reset All" + hiddenTabId;
    const std::string randomizeAllButton = "Randomize All" + hiddenTabId;
    const std::string lockAllButton = "Lock All" + hiddenTabId;
    const std::string unlockAllButton = "Unlock All" + hiddenTabId;

    ImGui::SeparatorText(tabName.c_str());
    if (UIWidgets::Button(resetAllButton.c_str(),
                          UIWidgets::ButtonOptions().Size(UIWidgets::Sizes::Inline).Color(THEME_COLOR))) {
        auto currentBGM = func_800FA0B4(SEQ_PLAYER_BGM_MAIN);
        auto prevReplacement = AudioCollection::Instance->GetReplacementSequence(currentBGM);
        ResetGroup(map, type);
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        auto curReplacement = AudioCollection::Instance->GetReplacementSequence(currentBGM);
        if (type == SEQ_BGM_WORLD && prevReplacement != curReplacement) {
            ReplayCurrentBGM();
        }
    }
    ImGui::SameLine();
    if (UIWidgets::Button(randomizeAllButton.c_str(),
                          UIWidgets::ButtonOptions().Size(UIWidgets::Sizes::Inline).Color(THEME_COLOR))) {
        auto currentBGM = func_800FA0B4(SEQ_PLAYER_BGM_MAIN);
        auto prevReplacement = AudioCollection::Instance->GetReplacementSequence(currentBGM);
        RandomizeGroup(type);
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        auto curReplacement = AudioCollection::Instance->GetReplacementSequence(currentBGM);
        if (type == SEQ_BGM_WORLD && prevReplacement != curReplacement) {
            ReplayCurrentBGM();
        }
    }
    ImGui::SameLine();
    if (UIWidgets::Button(lockAllButton.c_str(),
                          UIWidgets::ButtonOptions().Size(UIWidgets::Sizes::Inline).Color(THEME_COLOR))) {
        auto currentBGM = func_800FA0B4(SEQ_PLAYER_BGM_MAIN);
        auto prevReplacement = AudioCollection::Instance->GetReplacementSequence(currentBGM);
        LockGroup(map, type);
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        auto curReplacement = AudioCollection::Instance->GetReplacementSequence(currentBGM);
        if (type == SEQ_BGM_WORLD && prevReplacement != curReplacement) {
            ReplayCurrentBGM();
        }
    }
    ImGui::SameLine();
    if (UIWidgets::Button(unlockAllButton.c_str(),
                          UIWidgets::ButtonOptions().Size(UIWidgets::Sizes::Inline).Color(THEME_COLOR))) {
        auto currentBGM = func_800FA0B4(SEQ_PLAYER_BGM_MAIN);
        auto prevReplacement = AudioCollection::Instance->GetReplacementSequence(currentBGM);
        UnlockGroup(map, type);
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        auto curReplacement = AudioCollection::Instance->GetReplacementSequence(currentBGM);
        if (type == SEQ_BGM_WORLD && prevReplacement != curReplacement) {
            ReplayCurrentBGM();
        }
    }

    auto playingFromMenu = CVarGetInteger(CVAR_AUDIO("Playing"), 0);
    auto currentBGM = func_800FA0B4(SEQ_PLAYER_BGM_MAIN);

    // Longest text in Audio Editor
    ImVec2 columnSize = ImGui::CalcTextSize("Navi - Look/Hey/Watchout (Target Enemy)");
    ImGui::BeginTable(tabId.c_str(), 3, ImGuiTableFlags_SizingFixedFit);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, columnSize.x + 30);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, columnSize.x + 30);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 160.0f);
    for (const auto& [defaultValue, seqData] : map) {
        if (~(seqData.category) & type) {
            continue;
        }
        // Do not display custom sequences in the list
        if ((((seqData.category & SEQ_BGM_CUSTOM) || seqData.category == SEQ_FANFARE) &&
             defaultValue >= MAX_AUTHENTIC_SEQID) ||
            seqData.canBeReplaced == false) {
            continue;
        }

        const std::string initialSfxKey = seqData.sfxKey;
        const std::string cvarKey = AudioCollection::Instance->GetCvarKey(seqData.sfxKey);
        const std::string cvarLockKey = AudioCollection::Instance->GetCvarLockKey(seqData.sfxKey);
        const std::string hiddenKey = "##" + cvarKey;
        const std::string resetButton = ICON_FA_UNDO + hiddenKey;
        const std::string randomizeButton = ICON_FA_RANDOM + hiddenKey;
        const std::string lockedButton = ICON_FA_LOCK + hiddenKey;
        const std::string unlockedButton = ICON_FA_UNLOCK + hiddenKey;
        const int currentValue = CVarGetInteger(cvarKey.c_str(), defaultValue);
        const bool isCurrentlyPlaying = currentValue == playingFromMenu || seqData.sequenceId == currentBGM;

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (isCurrentlyPlaying) {
            ImGui::TextColored(UIWidgets::ColorValues.at(UIWidgets::Colors::Yellow), "%s %s", ICON_FA_PLAY,
                               seqData.label.c_str());
        } else {
            ImGui::Text("%s", seqData.label.c_str());
        }
        ImGui::TableNextColumn();
        ImGui::PushItemWidth(-FLT_MIN);
        const int initialValue = map.contains(currentValue) ? currentValue : defaultValue;
        UIWidgets::PushStyleCombobox(THEME_COLOR);
        if (ImGui::BeginCombo(hiddenKey.c_str(), map.at(initialValue).label.c_str())) {
            for (const auto& [value, seqData] : map) {
                // If excluded as a replacement sequence, don't show in other dropdowns except the effect's own
                // dropdown.
                if (~(seqData.category) & type ||
                    (!seqData.canBeUsedAsReplacement && initialSfxKey != seqData.sfxKey)) {
                    continue;
                }

                if (ImGui::Selectable(seqData.label.c_str())) {
                    CVarSetInteger(cvarKey.c_str(), value);
                    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
                    UpdateCurrentBGM(defaultValue, type);
                }

                if (currentValue == value) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }
        UIWidgets::PopStyleCombobox();
        ImGui::TableNextColumn();
        ImGui::PushItemWidth(-FLT_MIN);
        DrawPreviewButton((type == SEQ_SFX || type == SEQ_VOICE || type == SEQ_INSTRUMENT) ? defaultValue
                                                                                           : currentValue,
                          seqData.sfxKey, type);
        auto locked = CVarGetInteger(cvarLockKey.c_str(), 0) == 1;
        ImGui::SameLine();
        ImGui::PushItemWidth(-FLT_MIN);
        if (UIWidgets::Button(resetButton.c_str(), UIWidgets::ButtonOptions()
                                                       .Size(UIWidgets::Sizes::Inline)
                                                       .Padding(ImVec2(10.0f, 6.0f))
                                                       .Tooltip("Reset to default")
                                                       .Color(THEME_COLOR))) {
            CVarClear(cvarKey.c_str());
            CVarClear(cvarLockKey.c_str());
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
            UpdateCurrentBGM(defaultValue, seqData.category);
        }
        ImGui::SameLine();
        ImGui::PushItemWidth(-FLT_MIN);
        if (UIWidgets::Button(randomizeButton.c_str(), UIWidgets::ButtonOptions()
                                                           .Size(UIWidgets::Sizes::Inline)
                                                           .Padding(ImVec2(10.0f, 6.0f))
                                                           .Tooltip("Randomize this sound")
                                                           .Color(THEME_COLOR))) {
            std::vector<SequenceInfo*> validSequences = {};
            for (const auto seqInfo : AudioCollection::Instance->GetIncludedSequences()) {
                if (seqInfo->category & type) {
                    validSequences.push_back(seqInfo);
                }
            }

            if (validSequences.size()) {
                auto it = validSequences.begin();
                const auto& seqData =
                    *std::next(it, ShipUtils::Random(0, static_cast<uint32_t>(validSequences.size())));
                CVarSetInteger(cvarKey.c_str(), seqData->sequenceId);
                if (locked) {
                    CVarClear(cvarLockKey.c_str());
                }
                Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
                UpdateCurrentBGM(defaultValue, type);
            }
        }
        ImGui::SameLine();
        ImGui::PushItemWidth(-FLT_MIN);
        if (UIWidgets::Button(locked ? lockedButton.c_str() : unlockedButton.c_str(),
                              UIWidgets::ButtonOptions()
                                  .Size(UIWidgets::Sizes::Inline)
                                  .Padding(ImVec2(10.0f, 6.0f))
                                  .Tooltip(locked ? "Sound locked" : "Sound unlocked")
                                  .Color(THEME_COLOR))) {
            if (locked) {
                CVarClear(cvarLockKey.c_str());
            } else {
                CVarSetInteger(cvarLockKey.c_str(), 1);
            }
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        }
    }
    ImGui::EndTable();
}

extern "C" u16 AudioEditor_GetReplacementSeq(u16 seqId) {
    return AudioCollection::Instance->GetReplacementSequence(seqId);
}

std::string GetSequenceTypeName(SeqType type) {
    switch (type) {
        case SEQ_NOSHUFFLE:
            return "No Shuffle";
        case SEQ_BGM_WORLD:
            return "World";
        case SEQ_BGM_EVENT:
            return "Event";
        case SEQ_BGM_BATTLE:
            return "Battle";
        case SEQ_OCARINA:
            return "Ocarina";
        case SEQ_FANFARE:
            return "Fanfare";
        case SEQ_BGM_ERROR:
            return "Error";
        case SEQ_SFX:
            return "SFX";
        case SEQ_VOICE:
            return "Voice";
        case SEQ_INSTRUMENT:
            return "Instrument";
        case SEQ_BGM_CUSTOM:
            return "Custom";
        default:
            return "No Sequence Type";
    }
}

ImVec4 GetSequenceTypeColor(SeqType type) {
    switch (type) {
        case SEQ_BGM_WORLD:
            return ImVec4(0.0f, 0.2f, 0.0f, 1.0f);
        case SEQ_BGM_EVENT:
            return ImVec4(0.3f, 0.0f, 0.15f, 1.0f);
        case SEQ_BGM_BATTLE:
            return ImVec4(0.2f, 0.07f, 0.0f, 1.0f);
        case SEQ_OCARINA:
            return ImVec4(0.0f, 0.0f, 0.4f, 1.0f);
        case SEQ_FANFARE:
            return ImVec4(0.3f, 0.0f, 0.3f, 1.0f);
        case SEQ_SFX:
            return ImVec4(0.4f, 0.33f, 0.0f, 1.0f);
        case SEQ_VOICE:
            return ImVec4(0.3f, 0.42f, 0.09f, 1.0f);
        case SEQ_INSTRUMENT:
            return ImVec4(0.0f, 0.25f, 0.5f, 1.0f);
        case SEQ_BGM_CUSTOM:
            return ImVec4(0.9f, 0.0f, 0.9f, 1.0f);
        default:
            return ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    }
}

void DrawTypeChip(SeqType type, std::string sequenceName) {
    ImGui::BeginDisabled();
    ImGui::PushStyleColor(ImGuiCol_Button, GetSequenceTypeColor(type));
    std::string buttonLabel = GetSequenceTypeName(type) + "##" + sequenceName;
    ImGui::Button(buttonLabel.c_str());
    ImGui::PopStyleColor();
    ImGui::EndDisabled();
}

void AudioEditorRegisterOnSceneInitHook() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t sceneNum) {
        if (gSaveContext.gameMode != GAMEMODE_END_CREDITS &&
            CVarGetInteger(CVAR_AUDIO("RandomizeAudioGenModes"), 0) == RANDOMIZE_ON_NEW_SCENE) {

            AudioEditor_AutoRandomizeAll();
        }
    });
}

void AudioEditorRegisterOnGenerationCompletionHook() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGenerationCompletion>([]() {
        if (CVarGetInteger(CVAR_AUDIO("RandomizeAudioGenModes"), 0) == RANDOMIZE_ON_RANDO_GEN_ONLY) {

            AudioEditor_AutoRandomizeAll();
        }
    });
}

void AudioEditorRegisterOnLoadGameHook() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t fileNum) {
        if (CVarGetInteger(CVAR_AUDIO("RandomizeAudioGenModes"), 0) == RANDOMIZE_ON_FILE_LOAD ||
            CVarGetInteger(CVAR_AUDIO("RandomizeAudioGenModes"), 0) == RANDOMIZE_ON_FILE_LOAD_SEEDED) {

            AudioEditor_AutoRandomizeAll();
        }
    });
}

void AudioEditor::InitElement() {
    AudioEditorRegisterOnSceneInitHook();
    AudioEditorRegisterOnGenerationCompletionHook();
    AudioEditorRegisterOnLoadGameHook();
}

void AudioEditor::DrawElement() {
    AudioCollection::Instance->InitializeShufflePool();

    UIWidgets::Separator();
    if (UIWidgets::Button("Randomize All Groups",
                          UIWidgets::ButtonOptions()
                              .Size(ImVec2(230.0f, 0.0f))
                              .Color(THEME_COLOR)
                              .Tooltip("Randomizes all unlocked music and sound effects across tab groups"))) {
        AudioEditor_RandomizeAll();
    }
    ImGui::SameLine();
    if (UIWidgets::Button("Reset All Groups",
                          UIWidgets::ButtonOptions()
                              .Size(ImVec2(230.0f, 0.0f))
                              .Color(THEME_COLOR)
                              .Tooltip("Resets all unlocked music and sound effects across tab groups"))) {
        AudioEditor_ResetAll();
    }
    ImGui::SameLine();
    if (UIWidgets::Button("Lock All Groups", UIWidgets::ButtonOptions()
                                                 .Size(ImVec2(230.0f, 0.0f))
                                                 .Color(THEME_COLOR)
                                                 .Tooltip("Locks all music and sound effects across tab groups"))) {
        AudioEditor_LockAll();
    }
    ImGui::SameLine();
    if (UIWidgets::Button("Unlock All Groups", UIWidgets::ButtonOptions()
                                                   .Size(ImVec2(230.0f, 0.0f))
                                                   .Color(THEME_COLOR)
                                                   .Tooltip("Unlocks all music and sound effects across tab groups"))) {
        AudioEditor_UnlockAll();
    }
    UIWidgets::Separator();

    UIWidgets::PushStyleTabs(THEME_COLOR);
    if (ImGui::BeginTabBar("SfxContextTabBar", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton)) {

        static ImVec2 cellPadding(8.0f, 8.0f);
        if (ImGui::BeginTabItem("Audio Options")) {
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, cellPadding);
            ImGui::BeginTable("Audio Options", 1, ImGuiTableFlags_SizingStretchSame);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::BeginChild("SfxOptions", ImVec2(0, -8))) {
                SohGui::mSohMenu->MenuDrawItem(lowHpAlarm, static_cast<uint32_t>(ImGui::GetContentRegionAvail().x),
                                               THEME_COLOR);
                SohGui::mSohMenu->MenuDrawItem(naviCall, static_cast<uint32_t>(ImGui::GetContentRegionAvail().x),
                                               THEME_COLOR);
                SohGui::mSohMenu->MenuDrawItem(enemyProx, static_cast<uint32_t>(ImGui::GetContentRegionAvail().x),
                                               THEME_COLOR);
                if (!CVarGetInteger(CVAR_AUDIO("EnemyBGMDisable"), 0)) {
                    SohGui::mSohMenu->MenuDrawItem(leeverProx, static_cast<uint32_t>(ImGui::GetContentRegionAvail().x),
                                                   THEME_COLOR);
                }
                SohGui::mSohMenu->MenuDrawItem(leadingMusic, static_cast<uint32_t>(ImGui::GetContentRegionAvail().x),
                                               THEME_COLOR);
                SohGui::mSohMenu->MenuDrawItem(displaySeqName, static_cast<uint32_t>(ImGui::GetContentRegionAvail().x),
                                               THEME_COLOR);
                SohGui::mSohMenu->MenuDrawItem(ovlDuration, static_cast<uint32_t>(ImGui::GetContentRegionAvail().x),
                                               THEME_COLOR);
                SohGui::mSohMenu->MenuDrawItem(voicePitch, static_cast<uint32_t>(ImGui::GetContentRegionAvail().x),
                                               THEME_COLOR);
                ImGui::SameLine();
                ImGui::SetCursorPosY(ImGui::GetCursorPos().y + 40.f);
                if (UIWidgets::Button("Reset##linkVoiceFreqMultiplier",
                                      UIWidgets::ButtonOptions().Size(ImVec2(80, 36)).Padding(ImVec2(5.0f, 0.0f)))) {
                    CVarSetFloat(CVAR_AUDIO("LinkVoiceFreqMultiplier"), 1.0f);
                }
                SohGui::mSohMenu->MenuDrawItem(proximityWeatherThunder,
                                               static_cast<uint32_t>(ImGui::GetContentRegionAvail().x), THEME_COLOR);
                SohGui::mSohMenu->MenuDrawItem(proximityWeatherThunderStyle,
                                               static_cast<uint32_t>(ImGui::GetContentRegionAvail().x), THEME_COLOR);
                SohGui::mSohMenu->MenuDrawItem(proximityWeatherRainVolume,
                                               static_cast<uint32_t>(ImGui::GetContentRegionAvail().x), THEME_COLOR);
                SohGui::mSohMenu->MenuDrawItem(proximityWeatherThunderVolume,
                                               static_cast<uint32_t>(ImGui::GetContentRegionAvail().x), THEME_COLOR);
                SohGui::mSohMenu->MenuDrawItem(randomAudioGenModes,
                                               static_cast<uint32_t>(ImGui::GetContentRegionAvail().x), THEME_COLOR);
                SohGui::mSohMenu->MenuDrawItem(lowerOctaves, static_cast<uint32_t>(ImGui::GetContentRegionAvail().x),
                                               THEME_COLOR);
            }
            ImGui::EndChild();
            ImGui::EndTable();
            ImGui::PopStyleVar(1);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Background Music")) {
            Draw_SfxTab("backgroundMusic", SEQ_BGM_WORLD, "Background Music");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Fanfares")) {
            Draw_SfxTab("fanfares", SEQ_FANFARE, "Fanfares");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Events")) {
            Draw_SfxTab("event", SEQ_BGM_EVENT, "Events");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Battle Music")) {
            Draw_SfxTab("battleMusic", SEQ_BGM_BATTLE, "Battle Music");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Ending")) {
            Draw_SfxTab("ending", SEQ_ENDING, "Ending");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Ocarina")) {
            Draw_SfxTab("instrument", SEQ_INSTRUMENT, "Instruments");
            Draw_SfxTab("ocarina", SEQ_OCARINA, "Ocarina");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Sound Effects")) {
            Draw_SfxTab("sfx", SEQ_SFX, "Sound Effects");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Voices")) {
            Draw_SfxTab("voice", SEQ_VOICE, "Voices");
            ImGui::EndTabItem();
        }

        static bool excludeTabOpen = false;
        if (ImGui::BeginTabItem("Audio Shuffle Pool Management")) {
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, cellPadding);
            if (!excludeTabOpen) {
                excludeTabOpen = true;
            }

            static std::map<SeqType, bool> showType{
                { SEQ_BGM_WORLD, true }, { SEQ_BGM_EVENT, true },  { SEQ_BGM_BATTLE, true },
                { SEQ_OCARINA, true },   { SEQ_FANFARE, true },    { SEQ_SFX, true },
                { SEQ_VOICE, true },     { SEQ_INSTRUMENT, true }, { SEQ_BGM_CUSTOM, true },
            };

            // make temporary sets because removing from the set we're iterating through crashes ImGui
            std::set<SequenceInfo*> seqsToInclude = {};
            std::set<SequenceInfo*> seqsToExclude = {};

            static ImGuiTextFilter sequenceSearch;
            UIWidgets::PushStyleInput(THEME_COLOR);
            sequenceSearch.Draw("Filter (inc,-exc)", 490.0f);
            UIWidgets::PopStyleInput();
            ImGui::SameLine();
            if (UIWidgets::Button("Exclude All",
                                  UIWidgets::ButtonOptions().Size(UIWidgets::Sizes::Inline).Color(THEME_COLOR))) {
                for (auto seqInfo : AudioCollection::Instance->GetIncludedSequences()) {
                    if (sequenceSearch.PassFilter(seqInfo->label.c_str()) && showType[seqInfo->category]) {
                        seqsToExclude.insert(seqInfo);
                    }
                }
            }
            ImGui::SameLine();
            if (UIWidgets::Button("Include All",
                                  UIWidgets::ButtonOptions().Size(UIWidgets::Sizes::Inline).Color(THEME_COLOR))) {
                for (auto seqInfo : AudioCollection::Instance->GetExcludedSequences()) {
                    if (sequenceSearch.PassFilter(seqInfo->label.c_str()) && showType[seqInfo->category]) {
                        seqsToInclude.insert(seqInfo);
                    }
                }
            }

            ImGui::BeginTable("sequenceTypes", 9,
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders);

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Header, GetSequenceTypeColor(SEQ_BGM_WORLD));
            ImGui::Selectable(GetSequenceTypeName(SEQ_BGM_WORLD).c_str(), &showType[SEQ_BGM_WORLD]);
            ImGui::PopStyleColor(1);

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Header, GetSequenceTypeColor(SEQ_BGM_EVENT));
            ImGui::Selectable(GetSequenceTypeName(SEQ_BGM_EVENT).c_str(), &showType[SEQ_BGM_EVENT]);
            ImGui::PopStyleColor(1);

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Header, GetSequenceTypeColor(SEQ_BGM_BATTLE));
            ImGui::Selectable(GetSequenceTypeName(SEQ_BGM_BATTLE).c_str(), &showType[SEQ_BGM_BATTLE]);
            ImGui::PopStyleColor(1);

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Header, GetSequenceTypeColor(SEQ_OCARINA));
            ImGui::Selectable(GetSequenceTypeName(SEQ_OCARINA).c_str(), &showType[SEQ_OCARINA]);
            ImGui::PopStyleColor(1);

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Header, GetSequenceTypeColor(SEQ_FANFARE));
            ImGui::Selectable(GetSequenceTypeName(SEQ_FANFARE).c_str(), &showType[SEQ_FANFARE]);
            ImGui::PopStyleColor(1);

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Header, GetSequenceTypeColor(SEQ_SFX));
            ImGui::Selectable(GetSequenceTypeName(SEQ_SFX).c_str(), &showType[SEQ_SFX]);
            ImGui::PopStyleColor(1);

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Header, GetSequenceTypeColor(SEQ_VOICE));
            ImGui::Selectable(GetSequenceTypeName(SEQ_VOICE).c_str(), &showType[SEQ_VOICE]);
            ImGui::PopStyleColor(1);

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Header, GetSequenceTypeColor(SEQ_INSTRUMENT));
            ImGui::Selectable(GetSequenceTypeName(SEQ_INSTRUMENT).c_str(), &showType[SEQ_INSTRUMENT]);
            ImGui::PopStyleColor(1);

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Header, GetSequenceTypeColor(SEQ_BGM_CUSTOM));
            ImGui::Selectable(GetSequenceTypeName(SEQ_BGM_CUSTOM).c_str(), &showType[SEQ_BGM_CUSTOM]);
            ImGui::PopStyleColor(1);

            ImGui::EndTable();

            if (ImGui::BeginTable("tableAllSequences", 2, ImGuiTableFlags_BordersH | ImGuiTableFlags_BordersV)) {
                ImGui::TableSetupColumn("Included", ImGuiTableColumnFlags_WidthStretch, 200.0f);
                ImGui::TableSetupColumn("Excluded", ImGuiTableColumnFlags_WidthStretch, 200.0f);
                ImGui::TableHeadersRow();
                ImGui::TableNextRow();

                // COLUMN 1 - INCLUDED SEQUENCES
                ImGui::TableNextColumn();

                ImGui::BeginChild("ChildIncludedSequences", ImVec2(0, -8));
                for (auto seqInfo : AudioCollection::Instance->GetIncludedSequences()) {
                    if (sequenceSearch.PassFilter(seqInfo->label.c_str()) && showType[seqInfo->category]) {
                        if (UIWidgets::Button(std::string(ICON_FA_TIMES "##" + seqInfo->sfxKey).c_str(),
                                              UIWidgets::ButtonOptions()
                                                  .Size(UIWidgets::Sizes::Inline)
                                                  .Padding(ImVec2(9.0f, 6.0f))
                                                  .Color(THEME_COLOR))) {
                            seqsToExclude.insert(seqInfo);
                        }
                        ImGui::SameLine();
                        DrawPreviewButton(seqInfo->sequenceId, seqInfo->sfxKey, seqInfo->category);
                        ImGui::SameLine();
                        DrawTypeChip(seqInfo->category, seqInfo->label);
                        ImGui::SameLine();
                        ImGui::Text("%s", seqInfo->label.c_str());
                    }
                }
                ImGui::EndChild();

                // remove the sequences we added to the temp set
                for (auto seqInfo : seqsToExclude) {
                    AudioCollection::Instance->RemoveFromShufflePool(seqInfo);
                }

                // COLUMN 2 - EXCLUDED SEQUENCES
                ImGui::TableNextColumn();

                ImGui::BeginChild("ChildExcludedSequences", ImVec2(0, -8));
                for (auto seqInfo : AudioCollection::Instance->GetExcludedSequences()) {
                    if (sequenceSearch.PassFilter(seqInfo->label.c_str()) && showType[seqInfo->category]) {
                        if (UIWidgets::Button(std::string(ICON_FA_PLUS "##" + seqInfo->sfxKey).c_str(),
                                              UIWidgets::ButtonOptions()
                                                  .Size(UIWidgets::Sizes::Inline)
                                                  .Padding(ImVec2(9.0f, 6.0f))
                                                  .Color(THEME_COLOR))) {
                            seqsToInclude.insert(seqInfo);
                        }
                        ImGui::SameLine();
                        DrawPreviewButton(seqInfo->sequenceId, seqInfo->sfxKey, seqInfo->category);
                        ImGui::SameLine();
                        DrawTypeChip(seqInfo->category, seqInfo->sfxKey);
                        ImGui::SameLine();
                        ImGui::Text("%s", seqInfo->label.c_str());
                    }
                }
                ImGui::EndChild();

                // add the sequences we added to the temp set
                for (auto seqInfo : seqsToInclude) {
                    AudioCollection::Instance->AddToShufflePool(seqInfo);
                }

                ImGui::EndTable();
            }
            ImGui::PopStyleVar(1);
            ImGui::EndTabItem();
        } else {
            excludeTabOpen = false;
        }

        ImGui::EndTabBar();
    }
    UIWidgets::PopStyleTabs/:ïŸ-¢G§²ÚîÆ­yÒæ–æFö÷'2ÇÂ‡Æ’ÓæVçd7G‚çVæµô$bÒ„db’’°¢&–Ô6öÆ÷"ç"Ò56æG7F÷&Õ&–Ô6öÆ÷'5³Òç#°¢&–Ô6öÆ÷"ærÒ56æG7F÷&Õ&–Ô6öÆ÷'5³Òæs°¢&–Ô6öÆ÷"æ"Ò56æG7F÷&Õ&–Ô6öÆ÷'5³Òæ#°¢Vçd6öÆ÷"ç"Ò56æG7F÷&ÔVçd6öÆ÷'5³Òç#°¢Vçd6öÆ÷"ærÒ56æG7F÷&ÔVçd6öÆ÷'5³Òæs°¢Vçd6öÆ÷"æ"Ò56æG7F÷&ÔVçd6öÆ÷'5³Òæ#°¢ÒVÇ6R–b„EóƒdD42ÓÒEóƒdDC’°¢&–Ô6öÆ÷"ç"Ò56æG7F÷&Õ&–Ô6öÆ÷'5´EóƒdD45Òç#°¢&–Ô6öÆ÷"ærÒ56æG7F÷&Õ&–Ô6öÆ÷'5´EóƒdD45Òæs°¢&–Ô6öÆ÷"æ"Ò56æG7F÷&Õ&–Ô6öÆ÷'5´EóƒdD45Òæ#°¢Vçd6öÆ÷"ç"Ò56æG7F÷&ÔVçd6öÆ÷'5´EóƒdD45Òç#°¢Vçd6öÆ÷"ærÒ56æG7F÷&ÔVçd6öÆ÷'5´EóƒdD45Òæs°¢Vçd6öÆ÷"æ"Ò56æG7F÷&ÔVçd6öÆ÷'5´EóƒdD45Òæ#°¢ÒVÇ6R°¢&–Ô6öÆ÷"ç"Ò‡33"”c3%ôÄU%‡56æG7F÷&Õ&–Ô6öÆ÷'5´EóƒdD45Òç"Â56æG7F÷&Õ&–Ô6öÆ÷'5´EóƒdDCÒç"ÂEóƒdDCB“°¢&–Ô6öÆ÷"ærÒ‡33"”c3%ôÄU%‡56æG7F÷&Õ&–Ô6öÆ÷'5´EóƒdD45ÒærÂ56æG7F÷&Õ&–Ô6öÆ÷'5´EóƒdDCÒærÂEóƒdDCB“°¢&–Ô6öÆ÷"æ"Ò‡33"”c3%ôÄU%‡56æG7F÷&Õ&–Ô6öÆ÷'5´EóƒdD45Òæ"Â56æG7F÷&Õ&–Ô6öÆ÷'5´EóƒdDCÒæ"ÂEóƒdDCB“°¢Vçd6öÆ÷"ç"Ò‡33"”c3%ôÄU%‡56æG7F÷&ÔVçd6öÆ÷'5´EóƒdD45Òç"Â56æG7F÷&ÔVçd6öÆ÷'5´EóƒdDCÒç"ÂEóƒdDCB“°¢Vçd6öÆ÷"ærÒ‡33"”c3%ôÄU%‡56æG7F÷&ÔVçd6öÆ÷'5´EóƒdD45ÒærÂ56æG7F÷&ÔVçd6öÆ÷'5´EóƒdDCÒærÂEóƒdDCB“°¢Vçd6öÆ÷"æ"Ò‡33"”c3%ôÄU%‡56æG7F÷&ÔVçd6öÆ÷'5´EóƒdD45Òæ"Â56æG7F÷&ÔVçd6öÆ÷'5´EóƒdDCÒæ"ÂEóƒdDCB“°¢Ğ ¢Vçd6öÆ÷"ç"Ò‚†Vçd6öÆ÷"ç"¢7“‚’²‚ƒbãbÒ7“‚’¢&–Ô6öÆ÷"ç"’’¢ƒãbòbãb“°¢Vçd6öÆ÷"ærÒ‚†Vçd6öÆ÷"ær¢7“‚’²‚ƒbãbÒ7“‚’¢&–Ô6öÆ÷"ær’’¢ƒãbòbãb“°¢Vçd6öÆ÷"æ"Ò‚†Vçd6öÆ÷"æ"¢7“‚’²‚ƒbãbÒ7“‚’¢&–Ô6öÆ÷"æ"’’¢ƒãbòbãb“° ¢7“bÒ‡33"’„EóƒTdD#¢ƒãbòbãb’“°¢7“BÒ‡33"’„EóƒTdD#¢ƒ’ãbòbãb’“°¢7“"Ò‡33"’„EóƒTdD#¢ƒbãbòbãb’“° ¢õTåôD•52‡Æ’Óç7FFRævg„7G‚“° ¢ôÅ•õ„ÅUôD•5Òvg…õ6WGWDÅócB…ôÅ•õ„ÅUôD•5“°¢tE6WDÇ†F—F†W"…ôÅ•õ„ÅUôD•5²²ÂuôEôäô•4R“°¢tE6WD6öÆ÷$F—F†W"…ôÅ•õ„ÅUôD•5²²Âuô4Eôäô•4R“°¢tE6WE&–Ô6öÆ÷"…ôÅ•õ„ÅUôD•5²²ÂÂƒƒÂ&–Ô6öÆ÷"ç"Â&–Ô6öÆ÷"ærÂ&–Ô6öÆ÷"æ"ÂÆ’ÓæVçd7G‚ç6æG7F÷&Õ&–Ô“°¢tE6WDVçd6öÆ÷"…ôÅ•õ„ÅUôD•5²²ÂVçd6öÆ÷"ç"ÂVçd6öÆ÷"ærÂVçd6öÆ÷"æ"ÂÆ’ÓæVçd7G‚ç6æG7F÷&ÔVçd“°¢u56VvÖVçB…ôÅ•õ„ÅUôD•5²²Âƒ‚À¢vg…õGvõFW…67&öÆÄW‚‡Æ’Óç7FFRævg„7G‚ÂÂ‡S3"—7“bRƒÂÂƒ#Âƒ#ÂÂ‡S3"—7“BRƒÀ¢„ddbÒ‚‡S3"—7“"Rƒ’ÂƒÂƒCÂ7“‚ÂÂ7“‚¢ãVbÂ×7“‚’“°¢tE6WEFW‡GW&TÅUB…ôÅ•õ„ÅUôD•5²²ÂuõEEôäôäR“° ¢u5F—7Æ”Æ—7B…ôÅ•õ„ÅUôD•5²²Âtf–VÆE6æG7F÷&ÔDÂ“°¢4Äõ4UôD•52‡Æ’Óç7FFRævg„7G‚“° ¢EóƒTdD#³Ò‡33"—7“ƒ°§Ğ §fö–BVçf—&öæÖVçEôF§W7DÆ–v‡G2…Æ•7FFR¢Æ’Âc3"&sÂc3"&s"Âc3"&s2Âc3"&sB’°¢c3"FV×°¢33"“° ¢–b‡Æ’Óç&ööÔ7G‚æ7W%&ööÒæ&V†f–÷%G—SÒ$ôôÕô$T„d”õ%õE•SóRbbÆ•ô6Ô—4æ÷Df—†VB‡Æ’’’°¢&sÒ4ÄÕôÔ”â†&sÂãb“°¢&sÒ4ÄÕôÔ‚†&sÂãb“° ¢FV×Ò&sÒ&s3°¢–b†&sÂ&s2’°¢FV×Òãc°¢Ğ ¢Æ’ÓæVçd7G‚æF¤fötæV"Ò†&s"ÒÆ’ÓæVçd7G‚æÆ–v‡E6WGF–æw2æfötæV"’¢FV×° ¢–b†&sÓÒãb’°¢f÷"†’Ò²’Â3²’²²’°¢Æ’ÓæVçd7G‚æF¤föt6öÆ÷%¶•ÒÒ°¢Ğ¢ÒVÇ6R°¢FV×Ò&s¢Rãc°¢FV×Ò4ÄÕôÔ‚‡FV×Âãb“° ¢f÷"†’Ò²’Â3²’²²’°¢Æ’ÓæVçd7G‚æF¤föt6öÆ÷%¶•ÒÒÒ‡3b’‡Æ’ÓæVçd7G‚æÆ–v‡E6WGF–æw2æföt6öÆ÷%¶•Ò¢FV×“°¢Ğ¢Ğ ¢–b†&sBÃÒãb’°¢&WGW&ã°¢Ğ ¢&s£Ò&sC° ¢f÷"†’Ò²’Â3²’²²’°¢Æ’ÓæVçd7G‚æF¤Ö&–VçD6öÆ÷%¶•ÒÒÒ‡3b’‡Æ’ÓæVçd7G‚æÆ–v‡E6WGF–æw2æÖ&–VçD6öÆ÷%¶•Ò¢&s“°¢Æ’ÓæVçd7G‚æF¤Æ–v‡C6öÆ÷%¶•ÒÒÒ‡3b’‡Æ’ÓæVçd7G‚æÆ–v‡E6WGF–æw2æÆ–v‡C6öÆ÷%¶•Ò¢&s“°¢Ğ¢Ğ§Ğ §33"Vçf—&öæÖVçEôvWD&w4F”6÷VçB‡fö–B’°¢&WGW&âu6fT6öçFW‡Bæ&w4F”6÷VçC°§Ğ §fö–BVçf—&öæÖVçEô6ÆV$&w4F”6÷VçB‡fö–B’°¢u6fT6öçFW‡Bæ&w4F”6÷VçBÒ°§Ğ §33"Vçf—&öæÖVçEôvWEF÷FÄF—2‡fö–B’°¢&WGW&âu6fT6öçFW‡BçF÷FÄF—3°§Ğ §fö–BVçf—&öæÖVçEôf÷&6UÆ•6WVVæ6R‡Sb6W–B’°¢u6fT6öçFW‡Bæf÷&6VE6W–BÒ6W–C°§Ğ §33"Vçf—&öæÖVçEô—4f÷&6VE6WVVæ6TF—6&ÆVB‡fö–B’°¢33"—4F—6&ÆVBÒfÇ6S° ¢–b†u6fT6öçFW‡Bæf÷&6VE6W–BÓÒäô$tÕôD•4$ÄTB’°¢—4F—6&ÆVBÒG'VS°¢Ğ ¢&WGW&â—4F—6&ÆVC°§Ğ §fö–BVçf—&öæÖVçEõÆ•7F÷&ÔæGW&TÖ&–Væ6R…Æ•7FFR¢Æ’’°¢–b‡Æ’Óç6WVVæ6T7G‚ææGW&TÖ&–Væ6T–BÓÒäEU$Uô”EôäôäR’°¢VF–õõÆ”æGW&TÖ&–Væ6U6WVVæ6R„äEU$Uô”EôÔ$´UEôä”t…B“°¢ÒVÇ6R°¢VF–õõÆ”æGW&TÖ&–Væ6U6WVVæ6R‡Æ’Óç6WVVæ6T7G‚ææGW&TÖ&–Væ6T–B“°¢Ğ ¢VF–õõ6WDæGW&TÖ&–Væ6T6†ææVÄ”ò„äEU$Uô4„ääTÅõ$”âÂ4„ääTÅô”õõõ%EóÂ“°¢VF–õõ6WDæGW&TÖ&–Væ6T6†ææVÄ”ò„äEU$Uô4„ääTÅôÄ”t…Dä”ärÂ4„ääTÅô”õõõ%EóÂ“°§Ğ §fö–BVçf—&öæÖVçEõ7F÷7F÷&ÔæGW&TÖ&–Væ6R…Æ•7FFR¢Æ’’°¢VF–õõ6WDæGW&TÖ&–Væ6T6†ææVÄ”ò„äEU$Uô4„ääTÅõ$”âÂ4„ääTÅô”õõõ%EóÂ“°¢VF–õõ6WDæGW&TÖ&–Væ6T6†ææVÄ”ò„äEU$Uô4„ääTÅôÄ”t…Dä”ärÂ4„ääTÅô”õõõ%EóÂ“° ¢–b†gVæ5óƒd#B…4UõÄ”U%ô$tÕôÔ”â’ÓÒäô$tÕôäEU$UôÔ$”Tä4R’°¢u6fT6öçFW‡Bç6W–BÒäô$tÕôäEU$Uõ4e…õ$”ã°¢Vçf—&öæÖVçEõÆ•66VæU6WVVæ6R‡Æ’“°¢Ğ§Ğ §fö–BVçf—&öæÖVçEõv'6öætÆVfR…Æ•7FFR¢Æ’’°¢uvVF†W$ÖöFRÒ°¢u6fT6öçFW‡Bæ7WG66VæT–æFW‚Ò°¢u6fT6öçFW‡Bç&W7väfÆrÒÓ3°¢Æ’ÓææW‡DVçG&æ6T–æFW‚Òu6fT6öçFW‡Bç&W7våµ$U5tåôÔôDUõ$UEU$åÒæVçG&æ6T–æFWƒ°¢Æ’ÓçG&ç6—F–öåG&–vvW"ÒE$å5õE$”ttU%õ5D%C°¢Æ’ÓçG&ç6—F–öåG—RÒE$å5õE•UôdDUõt„•DS°¢u6fT6öçFW‡BææW‡EG&ç6—F–öåG—RÒE$å5õE•UôdDUõt„•DS° ¢7v—F6‚‡Æ’ÓææW‡DVçG&æ6T–æFW‚’°¢66RTåE%ôDTD…ôÔõTåD”åô5$DU%õUU%ôU„•C ¢fÆw5õ6WDWfVçD6†´–æb„UdTåD4„´”äeôTåDU$TEôDTD…ôÔõTåD”åô5$DU"“°¢'&V³°¢66RTåE%ôÄ´Uô…”Ä”ôäõ%D…ôU„•C ¢fÆw5õ6WDWfVçD6†´–æb„UdTåD4„´”äeôTåDU$TEôÄ´Uô…”Ä”“°¢'&V³°¢66RTåE%ôDU4U%Eô4ôÄõ55U5ôT5EôU„•C ¢fÆw5õ6WDWfVçD6†´–æb„UdTåD4„´”äeôTåDU$TEôDU4U%Eô4ôÄõ55U2“°¢'&V³°¢66RTåE%ôu$dU”$EôTåE$ä4S ¢fÆw5õ6WDWfVçD6†´–æb„UdTåD4„´”äeôTåDU$TEôu$dU”$B“°¢'&V³°¢66RTåE%õDTÕÄUôôeõD”ÔUôTåE$ä4S ¢fÆw5õ6WDWfVçD6†´–æb„UdTåD4„´”äeôTåDU$TEõDTÕÄUôôeõD”ÔR“°¢'&V³°¢66RTåE%õ45$TEôdõ$U5EôÔTDõuõ4õUD…ôU„•C ¢'&V³°¢Ğ§Ğ