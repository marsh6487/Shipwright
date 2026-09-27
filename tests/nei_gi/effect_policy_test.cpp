#include "soh/Enhancements/randomizer/NeiGiEffectPolicy.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>

int main() {
  using namespace NeiGi;
  const Kind kinds[] = {Kind::Neutral, Kind::Fire,  Kind::Ice,    Kind::Light,
                        Kind::Hylia,   Kind::Zonai, Kind::Demise, Kind::Leaf};
  const uint32_t colors[] = {0xE6E6EB, 0xFA8B20, 0x357CFF, 0xFDFF7B,
                             0xFF96FF, 0x64FFE6, 0x000000, 0x5AC85A};
  for (size_t k = 0; k < 8; ++k) {
    assert(ColorHex(kinds[k]) == colors[k]);
    assert(Sample(kinds[k], 123, false).count == 0);
    for (uint32_t frame : {0u, 1u, 89u, 179u, 180u, 65535u, 1000000u,
                           std::numeric_limits<uint32_t>::max()}) {
      const auto a = Sample(kinds[k], frame, true);
      const auto b = Sample(kinds[k], frame, true);
      assert(a.count > 0 && a.count <= 8);
      for (size_t i = 0; i < a.count; ++i) {
        const auto &p = a.particles[i];
        assert(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
        assert(std::abs(p.x) <= 40 && std::abs(p.y) <= 40 &&
               std::abs(p.z) <= 40);
        assert(p.size > 0 && p.size <= 3 && p.alpha <= 210);
        assert(p.x == b.particles[i].x && p.y == b.particles[i].y &&
               p.alpha == b.particles[i].alpha);
        assert(p.rgb == colors[k] ||
               (kinds[k] == Kind::Demise && p.rgb == 0xE6E6EB));
      }
    }
  }
  const auto now = Sample(Kind::Leaf, 0, true);
  const auto later = Sample(Kind::Leaf, 30, true);
  assert(now.particles[0].x != later.particles[0].x);
  assert(now.particles[0].shape == Shape::Leaf);
  std::cout << "NEI effect policy: colors, disabled behavior, bounds, "
               "determinism and frame-wrap checks passed\n";
}
