// Real shared batcher, at the full existing 5 x 3 projectile-set capacity.
#define main NeiGiRegressionMain
#include "../nei_gi/presentation_test.cpp"
#undef main
#include "soh/Enhancements/randomizer/NeiUsedMagicPolicy.h"
int main() {
    using namespace Fixture;
    using namespace NeiUsedMagic;
    const NeiGi::TextureMaterial ice{"__OTR__objects/nei_used_magic/ice_fracture",true,true};
    const NeiGi::TextureMaterial rays{"__OTR__objects/nei_used_magic/light_rays",true,false};
    for (Kind kind : {Kind::Ice,Kind::Light}) {
        size_t maxBytes=0,maxCommands=0;
        for(uint32_t frame=0;frame<180;++frame) {
            Reset(); play.gameplayFrames=frame;
            files.insert(ice.path);files.insert(rays.path);
            auto draw=[&](const Mesh& mesh) {
                NeiGi_DrawMesh(&play,kind==Kind::Ice?IceAtmosphere(mesh):mesh);
                if(kind==Kind::Ice) NeiGi_DrawTexturedMesh(&play,IceSurface(mesh),ice);
            };
            for(int set=0;set<5;++set) {
                const Point trail[]={{0,0,0},{-18,0,0},{-36,0,0},{-54,0,0},{-72,0,0},{-90,0,0}};
                draw(SampleTrail(kind,frame,trail,6,2));
                for(int p=0;p<3;++p)draw(SampleProjectile(kind,frame+set*19+p*7,2,{1,0,0}));
            }
            draw(SampleCharge(kind,frame,1));
            if(kind!=Kind::Ice)NeiGi_DrawTexturedMesh(&play,SampleChargeSurface(kind,frame,1),rays);
            draw(SampleChargeSparks(kind,frame,1));
            NeiGi_DrawMesh(&play,NeiGi::SampleOrb(kind),kind);
            NeiGi_DrawMesh(&play,NeiGi::SampleEnergy(kind,frame));
            size_t bytes=0;for(const auto& a:arena)bytes+=a.size()*sizeof(Vtx);
            maxBytes=std::max(maxBytes,bytes);maxCommands=std::max(maxCommands,size_t(gfx.polyXlu.p-xlu));
            assert(gfx.polyXlu.p<xlu+4096 && allocations<128 && stack.empty() && interpolation==0 && loads==0);
        }
        assert(maxBytes<90000 && maxCommands<2400); // Reserve ordinary scene/actor space.
        std::cout<<(kind==Kind::Ice?"Ice":"Light")<<" maximum 15 heads + 5 wakes + charge: "
                 <<maxBytes<<" vertex bytes, "<<maxCommands<<" XLU commands\n";
    }
}
