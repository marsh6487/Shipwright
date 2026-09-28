#include "soh/Enhancements/randomizer/NeiUsedMagicPolicy.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
#include <set>

using namespace NeiUsedMagic;
static void valid(const Mesh& m, float bound) {
    assert(m.count > 0 && m.count < m.vertices.size() && m.count % 3 == 0);
    size_t visible = 0;
    for (size_t i = 0; i < m.count; ++i) {
        const auto& v = m.vertices[i];
        assert(std::isfinite(v.p.x) && std::isfinite(v.p.y) && std::isfinite(v.p.z));
        assert(std::abs(v.p.x) <= bound && std::abs(v.p.y) <= bound && std::abs(v.p.z) <= bound);
        visible += v.alpha > 100;
    }
    assert(visible > 5);
}
static bool same(const Mesh& a, const Mesh& b) {
    if (a.count != b.count) return false;
    for (size_t i = 0; i < a.count; ++i) {
        const auto& x = a.vertices[i]; const auto& y = b.vertices[i];
        if (x.p.x != y.p.x || x.p.y != y.p.y || x.p.z != y.p.z || x.rgb != y.rgb || x.alpha != y.alpha)
            return false;
    }
    return true;
}
int main() {
    const Basis cameras[] = { {}, {{0, 0, 1}, {0, 1, 0}, {-1, 0, 0}},
        {{.7071f, 0, .7071f}, {.4082f, .8165f, -.4082f}, {-.5774f, .5774f, .5774f}} };
    for (const auto& camera : cameras) for (uint32_t frame = 0; frame < 180; ++frame) {
        for (Kind kind : {Kind::Ice, Kind::Light}) {
            auto projectile = SampleProjectile(kind, frame, 2.f, {1, .2f, .1f}, camera);
            valid(projectile, 45);
            assert(projectile.count <= 660); // 220 triangles per head, at most 15 heads.
            assert(same(projectile, SampleProjectile(kind, frame, 2.f, {1, .2f, .1f}, camera)));
            assert(SampleProjectile(kind, frame, 0.f, {}, camera).count == 0);
            assert(!same(projectile, SampleProjectile(kind, frame + 17, 2.f, {1, .2f, .1f}, camera)));
            valid(SampleSpin(kind, frame, 500, true, camera), 540);
            valid(SampleBurst(kind, frame, 1, camera), 85);
        }
        for (Kind kind : {Kind::Fire, Kind::Ice, Kind::Light}) {
            valid(SampleCharge(kind, frame, 1, camera), 90);
            valid(SampleChargeSparks(kind, frame, 1, camera), 40);
            assert(SampleCharge(kind, frame, 0, camera).count == 0);
            const auto surface = SampleChargeSurface(kind,frame,1,camera);
            valid(surface,90);
            for(size_t i=0;i<surface.count;++i) {
                assert(std::isfinite(surface.vertices[i].u) && std::isfinite(surface.vertices[i].v));
                assert(std::abs(surface.vertices[i].u*1024)<32767 && std::abs(surface.vertices[i].v*1024)<32767);
            }
        }
        auto portal = SamplePortal(frame, 1, 255, camera);
        valid(portal, 110);
        valid(SamplePortalSurface(frame,1,255),110);
        auto fade = SamplePortal(frame, 1, 127.5f, camera);
        auto small = SamplePortal(frame, .5f, 255, camera);
        assert(fade.count == portal.count && small.count == portal.count);
        for (size_t i = 0; i < portal.count; ++i) {
            assert(fade.vertices[i].alpha <= portal.vertices[i].alpha / 2 + 1);
            assert(std::abs(small.vertices[i].p.x * 2 - portal.vertices[i].p.x) < .001f);
            assert(std::abs(small.vertices[i].p.y * 2 - portal.vertices[i].p.y) < .001f);
            assert(std::abs(small.vertices[i].p.z * 2 - portal.vertices[i].p.z) < .001f);
        }
    }
    // Crystal geometry must include substantial hard, front-facing facets.
    const auto ice = SampleProjectile(Kind::Ice, 24, 2.f, {1, 0, 0}, {});
    std::set<uint32_t> facets; size_t hard = 0;
    for (size_t i = 0; i < ice.count; i += 3) {
        const auto& a = ice.vertices[i]; const auto& b = ice.vertices[i+1]; const auto& c = ice.vertices[i+2];
        if (a.alpha > 200 && a.alpha == b.alpha && a.alpha == c.alpha && a.rgb == b.rgb && b.rgb == c.rgb) {
            ++hard; facets.insert(a.rgb);
        }
    }
    assert(hard >= 18 && facets.size() >= 4);
    assert(SamplePortal(0, 0, 255).count == 0);
    assert(SamplePortal(0, 1, 0).count == 0);
    assert(SamplePortal(0, std::numeric_limits<float>::quiet_NaN(), 255).count == 0);
    assert(SampleProjectile(Kind::Fire, 0, 2, {}).count == 0); // Native fireballs are preserved.
    const Point wake[] = {{0,0,0}, {-18,0,0}, {-36,0,0}, {-54,0,0}, {-72,0,0}, {-90,0,0}};
    for (Kind kind : {Kind::Ice, Kind::Light}) valid(SampleTrail(kind, 30, wake, 6, 2), 110);
    assert(SampleTrail(Kind::Light, 0, nullptr, 6, 2).count == 0);
    std::cout << "USED VFX policy: deterministic animation, crystalline facets, bounded meshes, proportional portal fade/growth passed\n";
}
