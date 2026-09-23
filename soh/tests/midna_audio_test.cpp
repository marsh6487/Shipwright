#include "soh/Enhancements/audio/MidnaAudio.h"
#include "soh/Enhancements/audio/MidnaAudioResources.h"
#include "test_require.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <thread>
#include <vector>

static bool model = true;
static float gain = 1.0f;
static std::map<std::string, std::vector<uint8_t>> files;
static int reads;

namespace MidnaAudioResources {
bool HasModel() {
    return model;
}
bool ReadClip(const char* path, std::vector<uint8_t>& bytes) {
    ++reads;
    const auto it = files.find(path);
    if (it == files.end())
        return false;
    bytes = it->second;
    return true;
}
float Gain() {
    return gain;
}
} // namespace MidnaAudioResources

static void le(std::vector<uint8_t>& bytes, size_t offset, uint32_t value, size_t width) {
    for (size_t i = 0; i < width; ++i)
        bytes.at(offset + i) = value >> (i * 8);
}

static std::vector<uint8_t> wav(std::initializer_list<int16_t> samples) {
    std::vector<uint8_t> result(44 + samples.size() * 2);
    std::memcpy(result.data(), "RIFF", 4);
    le(result, 4, result.size() - 8, 4);
    std::memcpy(result.data() + 8, "WAVEfmt ", 8);
    le(result, 16, 16, 4);
    le(result, 20, 1, 2); // PCM
    le(result, 22, 1, 2); // mono
    le(result, 24, 32000, 4);
    le(result, 28, 64000, 4);
    le(result, 32, 2, 2);
    le(result, 34, 16, 2);
    std::memcpy(result.data() + 36, "data", 4);
    le(result, 40, samples.size() * 2, 4);
    size_t offset = 44;
    for (int16_t sample : samples) {
        le(result, offset, static_cast<uint16_t>(sample), 2);
        offset += 2;
    }
    return result;
}

static void movementSpacing() {
    files["objects/midna_navi/audio/appear.wav"] = wav({ 1000, -2000, 3000, 4000 });
    MidnaAudio_Init();
    REQUIRE(MidnaAudio_TryPlay(MIDNA_AUDIO_DASH));
    std::array<int16_t, 8> output = {};
    MidnaAudio_Mix(output.data(), 4);
    REQUIRE(output[0] == 1000);
    REQUIRE(MidnaAudio_TryPlay(MIDNA_AUDIO_APPEAR));
    output = {};
    MidnaAudio_Mix(output.data(), 4);
    REQUIRE(output[0] == 0);                       // alternating movement events share one cooldown
    REQUIRE(MidnaAudio_TryPlay(MIDNA_AUDIO_DASH)); // handled, no native fallback
    REQUIRE(MidnaAudio_TryPlay(MIDNA_AUDIO_VANISH));
    REQUIRE(MidnaAudio_TryPlay(MIDNA_AUDIO_CALL));
    output = {};
    MidnaAudio_Mix(output.data(), 4);
    REQUIRE(output[0] == 700); // recall and attention calls are not throttled
    MidnaAudio_Reset();        // changing scenes must not bypass spacing
    REQUIRE(MidnaAudio_TryPlay(MIDNA_AUDIO_DASH));
    output = {};
    MidnaAudio_Mix(output.data(), 4);
    REQUIRE(output[0] == 0);
    std::vector<int16_t> silence((256000 - 17) * 2);
    MidnaAudio_Mix(silence.data(), 256000 - 17);
    REQUIRE(std::all_of(silence.begin(), silence.end(), [](int16_t x) { return x == 0; }));
    REQUIRE(MidnaAudio_TryPlay(MIDNA_AUDIO_APPEAR));
    output = {};
    MidnaAudio_Mix(output.data(), 1);
    REQUIRE(output[0] == 0); // one sample before expiry; skipped cues do not queue
    REQUIRE(MidnaAudio_TryPlay(MIDNA_AUDIO_APPEAR));
    output = {};
    MidnaAudio_Mix(output.data(), 4);
    REQUIRE(output[0] == 1000);
    files.erase("objects/midna_navi/audio/appear.wav");
    MidnaAudio_Init();
    REQUIRE(!MidnaAudio_TryPlay(MIDNA_AUDIO_APPEAR));
}

// Catch timer consumption while hidden/moving and idle audio stealing a cue.
static void idleYawn() {
    files["objects/midna_navi/audio/yawn.wav"] = wav({ 1700, 1800, 1900, 2000 });
    MidnaAudio_Init();
    auto tick = [](bool eligible) {
        MidnaAudio_UpdateIdle(eligible);
        std::array<int16_t, 2> out = {};
        MidnaAudio_Mix(out.data(), 1);
        return out[0];
    };
    for (int i = 0; i < 2000; ++i)
        REQUIRE(tick(false) == 0);
    int elapsed = 0;
    while (++elapsed <= 900) {
        int16_t sample = tick(true);
        if (sample != 0) {
            REQUIRE(sample == 1700);
            break;
        }
        if (elapsed % 100 == 0)
            REQUIRE(tick(false) == 0); // resetting instead of pausing would never reach 20 seconds
    }
    REQUIRE(elapsed >= 400 && elapsed <= 900); // 20–45 seconds of eligible 20 Hz updates
    REQUIRE(tick(true) == 1800);               // continues, never restarts every update
    REQUIRE(tick(false) == 0);                 // movement/recall interrupts the idle cue

    // After a pause the unspent interval resumes, without a catch-up burst.
    for (int i = 0; i < 100; ++i)
        REQUIRE(tick(true) == 0);
    for (int i = 0; i < 2000; ++i)
        REQUIRE(tick(false) == 0);
    elapsed = 0;
    while (++elapsed <= 800 && tick(true) == 0) {}
    REQUIRE(elapsed >= 299 && elapsed <= 799);
    REQUIRE(MidnaAudio_TryPlay(MIDNA_AUDIO_CALL));
    REQUIRE(tick(true) == 200); // Hey replaces the yawn, without an added voice
    REQUIRE(tick(true) == 300);

    // Even a missing custom cue must cancel the yawn before native fallback.
    REQUIRE(MidnaAudio_TryPlay(MIDNA_AUDIO_YAWN));
    REQUIRE(!MidnaAudio_TryPlay(MIDNA_AUDIO_TALK));
    REQUIRE(tick(true) == 0);
    REQUIRE(MidnaAudio_TryPlay(MIDNA_AUDIO_YAWN));
    REQUIRE(MidnaAudio_TryPlay(MIDNA_AUDIO_VANISH));
    REQUIRE(tick(true) == 500); // recall stops the yawn rather than layering it
    REQUIRE(!MidnaAudio_TryPlay(MIDNA_AUDIO_YAWN)); // busy voices beat idle audio
    REQUIRE(tick(true) == 600);
    REQUIRE(MidnaAudio_TryPlay(MIDNA_AUDIO_YAWN));
    MidnaAudio_Reset();
    REQUIRE(tick(true) == 0);
    files.erase("objects/midna_navi/audio/yawn.wav");
    MidnaAudio_Init();
    for (int i = 0; i < 2000; ++i)
        REQUIRE(tick(true) == 0); // older packs remain silent, with no native substitute
}

int main() {
    files["objects/midna_navi/audio/dash.wav"] = wav({ 1000, -2000, 3000, 4000 });
    files["objects/midna_navi/audio/vanish.wav"] = wav({ 500, 600 });
    files["objects/midna_navi/audio/call.wav"] = wav({ 200, 300 });
    model = false;
    MidnaAudio_Init();
    REQUIRE(!MidnaAudio_TryPlay(MIDNA_AUDIO_DASH));
    REQUIRE(reads == 0); // audio pack alone must not change native Navi
    model = true;
    MidnaAudio_Init();
    REQUIRE(!MidnaAudio_TryPlay(MIDNA_AUDIO_HINT));
    REQUIRE(!MidnaAudio_TryPlay(static_cast<MidnaAudioEvent>(-1)));
    REQUIRE(!MidnaAudio_TryPlay(MIDNA_AUDIO_EVENT_COUNT));
    REQUIRE(MidnaAudio_TryPlay(MIDNA_AUDIO_DASH));
    const int startupReads = reads;
    std::array<int16_t, 4> output = { 10, 20, 30, 40 };
    MidnaAudio_Mix(output.data(), 2);
    REQUIRE((output == std::array<int16_t, 4>{ 1010, 1020, -1970, -1960 }));
    REQUIRE(MidnaAudio_TryPlay(MIDNA_AUDIO_DASH)); // don't restart a still-playing identical event
    output = {};
    MidnaAudio_Mix(output.data(), 2);
    REQUIRE((output == std::array<int16_t, 4>{ 3000, 3000, 4000, 4000 }));
    MidnaAudio_Mix(output.data(), 2);
    REQUIRE(output[0] == 3000);     // exhausted clips add nothing
    REQUIRE(reads == startupReads); // no archive I/O on playback or the audio thread

    MidnaAudio_Init();
    REQUIRE(MidnaAudio_TryPlay(MIDNA_AUDIO_DASH));
    REQUIRE(MidnaAudio_TryPlay(MIDNA_AUDIO_VANISH)); // latest movement replaces earlier movement
    REQUIRE(MidnaAudio_TryPlay(MIDNA_AUDIO_CALL));   // speech has an independent voice
    output = {};
    MidnaAudio_Mix(output.data(), 2);
    REQUIRE((output == std::array<int16_t, 4>{ 700, 700, 900, 900 }));
    gain = 0.5f;
    MidnaAudio_Init();
    REQUIRE(MidnaAudio_TryPlay(MIDNA_AUDIO_DASH));
    output = {};
    MidnaAudio_Mix(output.data(), 2);
    REQUIRE(output[0] == 500 && output[2] == -1000);
    gain = 0;
    output = {};
    MidnaAudio_Mix(output.data(), 2);
    REQUIRE(output[0] == 0 && output[2] == 0);
    gain = 1;
    MidnaAudio_Mix(output.data(), 2);
    REQUIRE(output[0] == 0); // muted audio still advances
    MidnaAudio_Init();
    REQUIRE(MidnaAudio_TryPlay(MIDNA_AUDIO_DASH));
    output = { 32760, -32760, -32760, 32760 };
    MidnaAudio_Mix(output.data(), 2);
    REQUIRE(output[0] == 32767 && output[2] == -32768);
    MidnaAudio_Reset();
    output = {};
    MidnaAudio_Mix(output.data(), 2);
    REQUIRE(output[0] == 0);

    // Unsupported and damaged files must retain native fallback independently.
    auto valid = files["objects/midna_navi/audio/dash.wav"];
    for (auto offset : { 0, 8, 20, 22, 24, 28, 32, 34, 40 }) {
        auto bad = valid;
        bad[offset] ^= 0x7F;
        files["objects/midna_navi/audio/dash.wav"] = bad;
        MidnaAudio_Init();
        REQUIRE(!MidnaAudio_TryPlay(MIDNA_AUDIO_DASH));
        REQUIRE(MidnaAudio_TryPlay(MIDNA_AUDIO_CALL));
    }
    for (size_t length = 0; length < valid.size(); ++length) {
        files["objects/midna_navi/audio/dash.wav"] = { valid.begin(), valid.begin() + length };
        MidnaAudio_Init();
        REQUIRE(!MidnaAudio_TryPlay(MIDNA_AUDIO_DASH));
    }
    // RIFF may contain an odd-sized unknown chunk; skip its alignment byte.
    auto padded = valid;
    padded.insert(padded.begin() + 12, { 'J', 'U', 'N', 'K', 1, 0, 0, 0, 42, 0 });
    le(padded, 4, padded.size() - 8, 4);
    files["objects/midna_navi/audio/dash.wav"] = padded;
    MidnaAudio_Init();
    REQUIRE(MidnaAudio_TryPlay(MIDNA_AUDIO_DASH));
    gain = std::numeric_limits<float>::quiet_NaN();
    output = {};
    MidnaAudio_Mix(output.data(), 2);
    REQUIRE(output[0] == 0);
    gain = 1;
    // Exercise the game/audio thread boundary and teardown with real synchronization.
    std::thread mixer([] {
        std::array<int16_t, 8> buffer;
        for (int i = 0; i < 1000; ++i) {
            buffer = {};
            MidnaAudio_Mix(buffer.data(), 4);
        }
    });
    for (int i = 0; i < 1000; ++i) {
        MidnaAudio_TryPlay(MIDNA_AUDIO_DASH);
        MidnaAudio_Reset();
    }
    mixer.join();
    MidnaAudio_Reset();
    movementSpacing();
    idleYawn();
    puts("PASS: Midna private clips, native fallback, PCM validation, mixing, volume and teardown");
}
