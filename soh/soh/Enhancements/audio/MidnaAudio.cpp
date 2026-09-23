#include "MidnaAudio.h"
#include "MidnaAudioResources.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <random>
#include <utility>
#include <vector>

namespace {
constexpr const char* kPaths[MIDNA_AUDIO_EVENT_COUNT] = {
    "objects/midna_navi/audio/dash.wav",         "objects/midna_navi/audio/appear.wav",
    "objects/midna_navi/audio/vanish.wav",       "objects/midna_navi/audio/target_npc.wav",
    "objects/midna_navi/audio/target_enemy.wav", "objects/midna_navi/audio/target_other.wav",
    "objects/midna_navi/audio/call.wav",         "objects/midna_navi/audio/hint.wav",
    "objects/midna_navi/audio/talk.wav",         "objects/midna_navi/audio/yawn.wav",
};
constexpr size_t kMaxSamples = 32000 * 10;
constexpr size_t kMovementGapFrames = 32000 * 8;

struct Voice {
    MidnaAudioEvent event = MIDNA_AUDIO_EVENT_COUNT;
    size_t position = 0;
};

std::mutex sMutex;
std::array<std::vector<int16_t>, MIDNA_AUDIO_EVENT_COUNT> sClips;
// Movement and speech each have one independent voice. New cues in the same
// category replace the old cue, rather than leaving a queue of stale calls.
std::array<Voice, 2> sVoices;
size_t sMovementGapRemaining = 0;
// Private randomness must not change gameplay's RNG sequence.
std::minstd_rand sIdleRandom{ 0x4D69646E };
unsigned sIdleDelay = 0;
unsigned sIdleQuietFrames = 0;

unsigned NextIdleDelay() {
    return 20 * 20 + sIdleRandom() % (25 * 20 + 1); // 20–45 seconds at 20 Hz
}

uint16_t Read16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

uint32_t Read32(const uint8_t* p) {
    return static_cast<uint32_t>(Read16(p)) | (static_cast<uint32_t>(Read16(p + 2)) << 16);
}

std::vector<int16_t> Decode(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
        std::memcmp(bytes.data() + 8, "WAVE", 4) != 0 ||
        static_cast<uint64_t>(Read32(bytes.data() + 4)) + 8 != bytes.size()) {
        return {};
    }
    const uint8_t* pcm = nullptr;
    size_t pcmSize = 0;
    bool format = false;
    size_t offset = 12;
    while (offset < bytes.size()) {
        if (bytes.size() - offset < 8) {
            return {};
        }
        const uint8_t* header = bytes.data() + offset;
        const size_t length = Read32(header + 4);
        offset += 8;
        if (length > bytes.size() - offset) {
            return {};
        }
        const uint8_t* data = bytes.data() + offset;
        if (std::memcmp(header, "fmt ", 4) == 0) {
            if (format || length < 16 || Read16(data) != 1 || Read16(data + 2) != 1 || Read32(data + 4) != 32000 ||
                Read32(data + 8) != 64000 || Read16(data + 12) != 2 || Read16(data + 14) != 16) {
                return {};
            }
            format = true;
        } else if (std::memcmp(header, "data", 4) == 0) {
            if (pcm != nullptr || length == 0 || length % 2 != 0 || length / 2 > kMaxSamples) {
                return {};
            }
            pcm = data;
            pcmSize = length;
        }
        offset += length;
        if (length % 2 != 0) {
            if (offset == bytes.size()) {
                return {};
            }
            ++offset;
        }
    }
    if (!format || pcm == nullptr) {
        return {};
    }
    std::vector<int16_t> samples(pcmSize / 2);
    for (size_t i = 0; i < samples.size(); ++i) {
        const int32_t raw = Read16(pcm + i * 2);
        samples[i] = static_cast<int16_t>(raw >= 32768 ? raw - 65536 : raw);
    }
    return samples;
}
} // namespace

extern "C" void MidnaAudio_Init(void) {
    std::array<std::vector<int16_t>, MIDNA_AUDIO_EVENT_COUNT> clips;
    if (MidnaAudioResources::HasModel()) {
        for (size_t i = 0; i < clips.size(); ++i) {
            std::vector<uint8_t> bytes;
            if (MidnaAudioResources::ReadClip(kPaths[i], bytes)) {
                clips[i] = Decode(bytes);
                if (clips[i].empty()) {
                    std::fprintf(stderr, "[midna-audio] unsupported clip %s; retaining native cue\n", kPaths[i]);
                }
            }
        }
    }
    // Decode and archive I/O stay outside both the mixer and its critical section.
    std::lock_guard<std::mutex> lock(sMutex);
    sVoices = {};
    sMovementGapRemaining = 0;
    sIdleDelay = NextIdleDelay();
    sIdleQuietFrames = 0;
    sClips = std::move(clips);
}

extern "C" bool MidnaAudio_TryPlay(MidnaAudioEvent event) {
    if (static_cast<unsigned>(event) >= MIDNA_AUDIO_EVENT_COUNT) {
        return false;
    }
    std::lock_guard<std::mutex> lock(sMutex);
    if (event != MIDNA_AUDIO_YAWN) {
        // An interaction also wins when its optional custom clip is absent and
        // the caller will fall back to native audio.
        if (sVoices[1].event == MIDNA_AUDIO_YAWN) {
            sVoices[1] = {};
        }
        sIdleQuietFrames = 0;
    } else if (sVoices[0].event != MIDNA_AUDIO_EVENT_COUNT || sVoices[1].event != MIDNA_AUDIO_EVENT_COUNT) {
        return false;
    }
    if (sClips[event].empty()) {
        return false;
    }
    if (event == MIDNA_AUDIO_DASH || event == MIDNA_AUDIO_APPEAR) {
        if (sMovementGapRemaining != 0) {
            // This cue is intentionally silent. Returning false would leak the
            // native fairy dash through the actor's per-clip fallback.
            return true;
        }
        sMovementGapRemaining = kMovementGapFrames;
    }
    Voice& voice = sVoices[event <= MIDNA_AUDIO_VANISH ? 0 : 1];
    if (voice.event != event) {
        voice = { event, 0 };
    }
    return true;
}

extern "C" void MidnaAudio_Mix(int16_t* output, size_t frames) {
    if (output == nullptr || frames == 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(sMutex);
    // Use the mixer's 32 kHz timeline so the gap advances during silence and
    // has no wall-clock jumps, frame-rate dependence, or queued vocal cues.
    sMovementGapRemaining -= std::min(sMovementGapRemaining, frames);
    if (sVoices[0].event == MIDNA_AUDIO_EVENT_COUNT && sVoices[1].event == MIDNA_AUDIO_EVENT_COUNT) {
        return;
    }
    float gain = MidnaAudioResources::Gain();
    gain = std::isfinite(gain) ? std::clamp(gain, 0.0f, 1.0f) : 0.0f;
    for (size_t i = 0; i < frames; ++i) {
        int32_t sample = 0;
        for (Voice& voice : sVoices) {
            if (voice.event == MIDNA_AUDIO_EVENT_COUNT) {
                continue;
            }
            const auto& clip = sClips[voice.event];
            sample += static_cast<int32_t>(clip[voice.position++] * gain);
            if (voice.position == clip.size()) {
                voice = {};
            }
        }
        for (size_t channel = 0; channel < 2; ++channel) {
            const size_t index = i * 2 + channel;
            output[index] =
                static_cast<int16_t>(std::clamp(static_cast<int32_t>(output[index]) + sample, -32768, 32767));
        }
    }
}

extern "C" void MidnaAudio_UpdateIdle(bool eligible) {
    std::lock_guard<std::mutex> lock(sMutex);
    if (!eligible) {
        if (sVoices[1].event == MIDNA_AUDIO_YAWN) {
            sVoices[1] = {};
        }
        sIdleQuietFrames = 0;
        return; // pause the unspent interval; never accumulate hidden time
    }
    if (sClips[MIDNA_AUDIO_YAWN].empty()) {
        return;
    }
    if (sIdleDelay > 0) {
        --sIdleDelay;
    }
    if (sVoices[0].event != MIDNA_AUDIO_EVENT_COUNT || sVoices[1].event != MIDNA_AUDIO_EVENT_COUNT) {
        sIdleQuietFrames = 0;
        return;
    }
    // After movement or a voice cue, allow two quiet seconds before a due yawn.
    sIdleQuietFrames = std::min(sIdleQuietFrames + 1, 40u);
    if (sIdleDelay == 0 && sIdleQuietFrames == 40) {
        sVoices[1] = { MIDNA_AUDIO_YAWN, 0 };
        sIdleDelay = NextIdleDelay();
        sIdleQuietFrames = 0;
    }
}

extern "C" void MidnaAudio_Reset(void) {
    std::lock_guard<std::mutex> lock(sMutex);
    sVoices = {};
    sIdleQuietFrames = 0;
}
