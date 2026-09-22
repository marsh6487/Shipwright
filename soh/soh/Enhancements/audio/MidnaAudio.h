#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MidnaAudioEvent {
    MIDNA_AUDIO_DASH,
    MIDNA_AUDIO_APPEAR,
    MIDNA_AUDIO_VANISH,
    MIDNA_AUDIO_TARGET_NPC,
    MIDNA_AUDIO_TARGET_ENEMY,
    MIDNA_AUDIO_TARGET_OTHER,
    MIDNA_AUDIO_CALL,
    MIDNA_AUDIO_HINT,
    MIDNA_AUDIO_TALK,
    MIDNA_AUDIO_EVENT_COUNT
} MidnaAudioEvent;

// Load optional private clips on startup. No original sound-bank entries change.
void MidnaAudio_Init(void);
// False means the caller must play its original sound, with its original arguments.
bool MidnaAudio_TryPlay(MidnaAudioEvent event);
// Adds dry, centered mono clips to the engine's 32 kHz stereo output.
void MidnaAudio_Mix(int16_t* interleavedStereo, size_t frameCount);
// Stop only Midna's voices at scene teardown; keep decoded clips for the next scene.
void MidnaAudio_Reset(void);

#ifdef __cplusplus
}
#endif
