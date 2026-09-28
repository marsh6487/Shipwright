#include "soh/Enhancements/randomizer/NeiUsedMagicPolicy.h"
#include <cassert>
#include <iostream>
using namespace NeiUsedMagic;
int main() {
  for (unsigned frame = 0; frame < 180; ++frame) {
    auto ice = SampleProjectile(Kind::Ice, frame, 2, {1, 0, 0});
    float xmin = 100, xmax = -100, width = 0;
    for (size_t i = 0; i < ice.count; ++i) {
      auto v = ice.vertices[i];
      xmin = std::min(xmin, v.p.x);
      xmax = std::max(xmax, v.p.x);
      width = std::max(width, std::max(std::abs(v.p.y), std::abs(v.p.z)));
    }
    assert(ice.count > 0 && ice.count < ice.vertices.size());
    assert(xmax - xmin > 35 && xmax - xmin < 60 &&
           width >
               15); // Compact bright head leaves the actual history visible.
    const Point history[] = {{0, 0, 0},   {-15, 0, 0}, {-30, 0, 0},
                             {-45, 0, 0}, {-60, 0, 0}, {-75, 0, 0}};
    const auto wake = SampleTrail(Kind::Ice, frame, history, 6, 2);
    bool oldIce = false;
    for (size_t i = 0; i < wake.count; ++i) {
      const auto &v = wake.vertices[i];
      oldIce |= v.p.x < -50 && v.alpha > 90 &&
                (std::abs(v.p.y) > 3 || std::abs(v.p.z) > 3);
    }
    assert(oldIce); // Substantial visible frost behind the body, not an
                    // occluded smooth line.
    assert(SampleProjectile(Kind::Fire, frame, 2, {1, 0, 0}).count > 0);
    auto release = SampleSpin(Kind::Fire, frame, 150, true);
    assert(release.count > 0 && release.count < release.vertices.size());
    for (auto kind : {Kind::Fire, Kind::Ice, Kind::Light}) {
      auto spin = SampleSpinSurface(kind, frame, 150, true);
      float height = 0;
      for (size_t i = 0; i < spin.count; ++i)
        height = std::max(height, spin.vertices[i].p.y);
      assert(spin.count < spin.vertices.size());
      assert(height == 192); // Native full-height wall; texture alpha defines
                             // the elemental crest.
      if (kind != Kind::Light) {
        const auto flow = SampleSpinFlow(kind, frame, 150, true, 1);
        const auto next = SampleSpinFlow(kind, frame + 1, 150, true, 1);
        assert(flow.count > 0 && flow.count + spin.count <= 864);
        assert(flow.vertices[0].u != next.vertices[0].u &&
               flow.vertices[0].v != next.vertices[0].v);
        assert(flow.vertices[0].u !=
               spin.vertices[0].u); // Independent material motion.
      }
    }
    assert(SampleChargeSurface(Kind::Fire, frame, 1).count ==
           0); // no enclosing portal cylinder
    auto fire = SampleCharge(Kind::Fire, frame, 1);
    assert(fire.count > 0 && fire.count <= 300);
    for (size_t i = 0; i < fire.count; ++i) {
      auto p = fire.vertices[i].p;
      assert(std::abs(p.x) <= 12 && std::abs(p.z) <= 12 && p.y >= -5 &&
             p.y <= 40);
    }
  }
  std::cout << "PASS substantial Ice/Fire bolts, native-height elemental "
               "walls, compact Fire focus\n";
}
