#include "WeatherSamplePlayer.h"

#include <array>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "soh/ResourceManagerHelpers.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "z64audio.h"

namespace {
constexpr size_t kMaximumVoices = 2;
constexpr size_t kSamplesPerFrame = 16;

struct Voice {
    const std::vector<int16_t>* samples = nullptr;
    size_t position = 0;
    float gain = 0.0f;
};

std::mutex sMutex;
std::unordered_map<std::string, std::vector<int16_t>> sDecodedSamples;
std::array<Voice, kMaximumVoices> sVoices;

int16_t ClampSample(int32_t sample) {
    return static_cast<int16_t>(std::clamp(sample, -32768, 32767));
}

std::vector<int16_t> DecodeSample(const SoundFontSample* sample) {
    if (sample == nullptr || sample->sampleAddr == nullptr || sample->book == nullptr || sample->book->book == nullptr) {
        return {};
    }

    const size_t frameSize = sample->codec == 0 ? 9 : sample->codec == 3 ? 5 : 0;
    if (frameSize == 0) {
        return {};
    }

    const size_t frameCount = sample->size / frameSize;
    size_t sampleCount = frameCount * kSamplesPerFrame;
    if (sample->loop != nullptr && sample->loop->sampleEnd > 0 && sample->loop->sampleEnd < sampleCount) {
        sampleCount = sample->loop->sampleEnd;
    }

    std::vector<int16_t> decoded;
    decoded.reserve(sampleCount);
    int16_t history[2] = { 0, 0 };
    const int16_t* book = sample->book->book;

    for (size_t frameIndex = 0; frameIndex < frameCount && decoded.size() < sampleCount; ++frameIndex) {
        const uint8_t* frame = sample->sampleAddr + frameIndex * frameSize;
        const int32_t shift = frame[0] >> 4;
        const int32_t predictor = frame[0] & 0x0F;
        const int32_t base = predictor * sample->book->order * 8;
        const int16_t* table0 = &book[base];
        const int16_t* table1 = &book[base + 8];

        for (size_t half = 0; half < 2 && decoded.size() < sampleCount; ++half) {
            int16_t values[8] = {};
            if (sample->codec == 0) {
                const uint8_t* data = &frame[1 + half * 4];
                for (size_t i = 0; i < 4; ++i) {
                    int32_t high = data[i] >> 4;
                    int32_t low = data[i] & 0x0F;
                    values[i * 2] = static_cast<int16_t>((high >= 8 ? high - 16 : high) << shift);
                    values[i * 2 + 1] = static_cast<int16_t>((low >= 8 ? low - 16 : low) << shift);
                }
            } else {
                const uint8_t* data = &frame[1 + half * 2];
                for (size_t i = 0; i < 2; ++i) {
                    for (size_t pair = 0; pair < 4; ++pair) {
                        const int32_t bitShift = 6 - static_cast<int32_t>(pair * 2);
                        int32_t value = (data[i] >> bitShift) & 3;
                        values[i * 4 + pair] = static_cast<int16_t>((value >= 2 ? value - 4 : value) << shift);
                    }
                }
            }

            int16_t output[8] = {};
            for (size_t i = 0; i < 8; ++i) {
                int32_t accumulator = table0[i] * history[0] + table1[i] * history[1] + (values[i] << 11);
                for (size_t previous = 0; previous < i; ++previous) {
                    accumulator += table1[i - previous - 1] * values[previous];
                }
                output[i] = ClampSample(accumulator >> 11);
                if (decoded.size() < sampleCount) {
                    decoded.push_back(output[i]);
                }
            }
            history[0] = output[6];
            history[1] = output[7];
        }
    }
    return decoded;
}
} // namespace

extern "C" void WeatherSamplePlayer_Init(void) {
    std::lock_guard<std::mutex> lock(sMutex);
    sDecodedSamples.clear();
    for (const char* path : { "audio/samples/Low Thunder_META", "audio/samples/Lightning_META" }) {
        sDecodedSamples.emplace(path, DecodeSample(ResourceMgr_LoadAudioSample(path)));
    }
    sVoices = {};
}

extern "C" bool WeatherSamplePlayer_Play(const char* resourcePath, float gain) {
    if (resourcePath == nullptr) {
        return false;
    }
    std::lock_guard<std::mutex> lock(sMutex);
    const auto sample = sDecodedSamples.find(resourcePath);
    if (sample == sDecodedSamples.end() || sample->second.empty()) {
        return false;
    }
    for (Voice& voice : sVoices) {
        if (voice.samples == nullptr) {
            voice = { &sample->second, 0, WeatherSamplePlayer_ClampGain(gain) };
            return true;
        }
    }
    return false;
}

extern "C" void WeatherSamplePlayer_Mix(int16_t* interleavedStereo, size_t frameCount) {
    if (interleavedStereo == nullptr || frameCount == 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(sMutex);
    const float sfxVolume = std::clamp(CVarGetInteger(CVAR_SETTING("Volume.SFX"), 100) / 100.0f, 0.0f, 1.0f);
    for (Voice& voice : sVoices) {
        if (voice.samples == nullptr) {
            continue;
        }
        const size_t remaining = voice.samples->size() - voice.position;
        const size_t mixedFrames = std::min(frameCount, remaining);
        WeatherSamplePlayer_TestMixMono(interleavedStereo, voice.samples->data() + voice.position, mixedFrames,
                                        voice.gain * sfxVolume);
        voice.position += mixedFrames;
        if (voice.position >= voice.samples->size()) {
            voice = {};
        }
    }
}

extern "C" void WeatherSamplePlayer_Reset(void) {
    std::lock_guard<std::mutex> lock(sMutex);
    sVoices = {};
}
