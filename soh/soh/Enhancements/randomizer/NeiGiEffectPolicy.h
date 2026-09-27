#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace NeiGi {
enum class Kind { Neutral, Fire, Ice, Light, Hylia, Zonai, Demise, Leaf };
enum class Shape { Sparkle, Shard, Ember, Leaf };

// Taken from the original NEI display-list materials, not the inventory icons.
// Demise's core uses cor's approved #000000 override; its pale rim is the native shell.
constexpr uint32_t ColorHex(Kind kind) {
    switch (kind) {
        case Kind::Fire:
            return 0xFA8B20;
        case Kind::Ice:
            return 0x357CFF;
        case Kind::Light:
            return 0xFDFF7B;
        case Kind::Hylia:
            return 0xFF96FF;
        case Kind::Zonai:
            return 0x64FFE6;
        case Kind::Demise:
            return 0x000000;
        case Kind::Leaf:
            return 0x5AC85A;
        default:
            return 0xE6E6EB;
    }
}

struct Particle {
    float x, y, z, size, angle;
    uint32_t rgb;
    uint8_t alpha;
    Shape shape;
};
struct Frame {
    std::array<Particle, 8> particles{};
    size_t count = 0;
};

inline Frame Sample(Kind kind, uint32_t frame, bool enabled) {
    Frame out;
    if (!enabled)
        return out;
    const bool rod = kind == Kind::Fire || kind == Kind::Ice || kind == Kind::Light;
    const bool leaf = kind == Kind::Leaf;
    const bool neutral = kind == Kind::Neutral;
    out.count = neutral || leaf ? 4 : 8;
    // Reduce integer time before conversion: no large-frame float jitter or overflow.
    const uint32_t tick = frame % 180u;
    constexpr float tau = 6.28318530718f;
    for (size_t i = 0; i < out.count; ++i) {
        const float phase = ((tick + static_cast<uint32_t>(i) * 23u) % 180u) / 180.0f;
        const float angle = tau * (phase + i * 0.61803399f);
        const float radius = rod ? 10.0f + 3.0f * std::sin(phase * tau) : (leaf ? 20.0f : (neutral ? 25.0f : 23.0f));
        const float fade = std::sin(phase * 3.14159265359f);
        auto& p = out.particles[i];
        p.x = radius * std::cos(angle);
        p.z = radius * std::sin(angle);
        p.y = rod ? (phase - 0.35f) * 20.0f : (phase - 0.5f) * 66.0f;
        p.size = (leaf ? 2.2f : (neutral ? 1.5f : 1.8f)) * (0.45f + fade * 0.55f);
        p.alpha = static_cast<uint8_t>(fade * (neutral ? 145.0f : 205.0f));
        p.angle = angle;
        p.rgb = ColorHex(kind);
        // A few pale facets accompany the black Demise motes, using its native shell color.
        if (kind == Kind::Demise && i % 3 == 0)
            p.rgb = 0xE6E6EB;
        p.shape = leaf ? Shape::Leaf
                       : (kind == Kind::Fire
                              ? Shape::Ember
                              : (kind == Kind::Ice || kind == Kind::Zonai || kind == Kind::Demise ? Shape::Shard
                                                                                                  : Shape::Sparkle));
    }
    return out;
}
} // namespace NeiGi
