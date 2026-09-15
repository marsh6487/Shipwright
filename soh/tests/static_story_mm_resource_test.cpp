/* Assembled with the actual production loader functions by mm_ordinary_verify.py. */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <fast/resource/factory/TextureFactory.h>
#include <fast/resource/ResourceType.h>
#include "soh/OTRGlobals.h"
#include "soh/resource/importer/SkeletonFactory.h"
#include "soh/resource/importer/SkeletonLimbFactory.h"
#include "soh/resource/importer/AnimationFactory.h"
#include "mods/transformation_masks/assets/mm_normal_actor_resource.h"
#include "mods/transformation_masks/assets/mm_kafei_resource.h"
#include "soh/resource/importer/PlayerAnimationFactory.h"
#include "mods/transformation_masks/assets/mm_asset_loader.h"
extern "C" {
#include "src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.h"
}
#include "tests/test_require.h"

// Application startup is the boundary; all resource/archive factories and types are real.
OTRGlobals* OTRGlobals::Instance = nullptr;
OTRGlobals::OTRGlobals() {}
OTRGlobals::~OTRGlobals() {}
static std::shared_ptr<Ship::Archive> sMmArchive;
static std::unordered_map<std::string, std::shared_ptr<Ship::IResource>> sMmResourceCache;
#define MMASSETS_LOG(...) ((void)0)
/* PRODUCTION_RESOURCE_FUNCTIONS */

int main(int argc, char** argv) {
    REQUIRE(argc == 2);
    auto context = Ship::Context::CreateUninitializedInstance("MM ordinary test", "mmtest", "/tmp/mm-ordinary-test.json");
    REQUIRE(context->InitLogging());
    REQUIRE(context->InitConfiguration());
    REQUIRE(context->InitConsoleVariables());
    REQUIRE(context->InitResourceManager({argv[1]}, {}, 1));
    OTRGlobals globals;
    globals.context = context;
    OTRGlobals::Instance = &globals;
    auto manager = context->GetResourceManager();
    auto loader = manager->GetResourceLoader();
    REQUIRE(loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinarySkeletonV0>(),
        RESOURCE_FORMAT_BINARY, "Skeleton", 0x4F534B4C, 0));
    REQUIRE(loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinarySkeletonLimbV0>(),
        RESOURCE_FORMAT_BINARY, "SkeletonLimb", 0x4F534C42, 0));
    REQUIRE(loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryAnimationV0>(),
        RESOURCE_FORMAT_BINARY, "Animation", 0x4F414E4D, 0));
    REQUIRE(loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryTextureV0>(),
        RESOURCE_FORMAT_BINARY, "Texture", static_cast<uint32_t>(Fast::ResourceType::Texture), 0));
    REQUIRE(loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryPlayerAnimationV0>(),
        RESOURCE_FORMAT_BINARY, "PlayerAnimation", 0x4F50414D, 0));
    sMmArchive = manager->GetArchiveManager()->GetArchives()->at(0);
    auto resolve = [manager](const std::string& path) { return manager->LoadResourceProcess(path); };
    unsigned loads = 0;
    for (bool alt : {false,true}) {
        manager->SetAltAssetsEnabled(alt);
        sMmResourceCache.clear();
        for (auto actor : {STATIC_STORY_ACTOR_HAPPY_MASK_SALESMAN, STATIC_STORY_ACTOR_KEATON, STATIC_STORY_ACTOR_LULU}) {
            unsigned count = actor == STATIC_STORY_ACTOR_LULU ? 4 : 3;
            for (unsigned pose=0;pose<count;++pose) {
                MmNormalActorResources resources{};
                bool loaded = MmAssets_LoadNormalActor(actor,pose,&resources);
                REQUIRE(loaded);
                auto presentation = StaticStoryMm_GetPresentation(actor,pose);
                REQUIRE(resources.owner && resources.skeleton && resources.animation);
                REQUIRE(resources.skeleton->sh.limbCount == presentation->limbCount);
                REQUIRE(resources.animation->common.frameCount == presentation->frameCount);
                auto retained = static_cast<std::vector<MmNormalActor::Resource>*>(resources.owner);
                REQUIRE(retained->size() == 2U + presentation->limbCount + presentation->eyeCount + presentation->mouthCount);
                std::weak_ptr<Ship::IResource> child = retained->at(1);
                auto rawChild = resources.skeleton->sh.segment[0];
                sMmResourceCache.clear();
                manager->UnloadResource(presentation->skeletonPath);
                for (const auto& path : std::static_pointer_cast<SOH::Skeleton>(retained->at(0))->limbTable)
                    manager->UnloadResource(path);
                REQUIRE(!child.expired());
                REQUIRE(child.lock()->GetRawPointer() == rawChild);
                REQUIRE(resources.animation->frameData[0] ==
                        (s16)std::static_pointer_cast<SOH::Animation>(retained->at(1+presentation->limbCount))->rotationValues[0]);
                MmAssets_ReleaseNormalActor(resources.owner);
                REQUIRE(child.expired());
                ++loads;
            }
        }
    }
    MmNormalActorResources disabled{};
    REQUIRE(!MmAssets_LoadNormalActor(STATIC_STORY_ACTOR_CHILD_KAFEI,0,&disabled));
    REQUIRE(!disabled.owner && !disabled.skeleton);
    auto skeleton=MmAssets_LoadResourceObjectFromMmArchive("objects/object_zov/gLuluSkel");
    auto animation=MmAssets_LoadResourceObjectFromMmArchive("objects/object_zov/gLuluLookDownAnim");
    auto typed=std::dynamic_pointer_cast<SOH::Skeleton>(skeleton);
    auto anim=std::dynamic_pointer_cast<SOH::Animation>(animation);
    REQUIRE(typed && anim);
    std::vector<MmNormalActor::Resource> retained;
    REQUIRE(MmNormalActor::ValidateSkeleton(skeleton,22,21,resolve,retained));
    retained.clear();
    auto original = typed->skeletonHeaderSegments[0];
    typed->skeletonHeaderSegments[0] = typed->skeletonHeaderSegments[1];
    REQUIRE(!MmNormalActor::ValidateSkeleton(skeleton,22,21,resolve,retained));
    typed->skeletonHeaderSegments[0] = original;
    auto child=std::dynamic_pointer_cast<SOH::SkeletonLimb>(resolve(typed->limbTable[0]));
    child->limbType = SOH::LimbType::LOD;
    REQUIRE(!MmNormalActor::ValidateSkeleton(skeleton,22,21,resolve,retained));
    child->limbType = SOH::LimbType::Standard;
    auto oldSibling=child->limbData.standardLimb.sibling;
    child->limbData.standardLimb.sibling=0;
    REQUIRE(!MmNormalActor::ValidateSkeleton(skeleton,22,21,resolve,retained));
    child->limbData.standardLimb.sibling=oldSibling;
    REQUIRE(!MmNormalActor::ValidateSkeleton(animation,22,21,resolve,retained));
    REQUIRE(!MmNormalActor::ValidateAnimation(skeleton,22,30));
    REQUIRE(!MmNormalActor::ValidateAnimation(animation,22,31));
    auto index=anim->rotationIndices[0].x;
    anim->rotationIndices[0].x=65535;
    REQUIRE(!MmNormalActor::ValidateAnimation(animation,22,30));
    anim->rotationIndices[0].x=index;
    auto data=anim->animationData.animationHeader.frameData;
    anim->animationData.animationHeader.frameData=nullptr;
    REQUIRE(!MmNormalActor::ValidateAnimation(animation,22,30));
    anim->animationData.animationHeader.frameData=data;
    anim->type=SOH::AnimationType::Link;
    REQUIRE(!MmNormalActor::ValidateAnimation(animation,22,30));
    anim->type=SOH::AnimationType::Normal;
    REQUIRE(!MmNormalActor::ValidateTexture(animation));
    auto kafei=MmAssets_LoadResourceObjectFromMmArchive("objects/object_test3/gKafeiSkel");
    REQUIRE(kafei);
    REQUIRE(!MmNormalActor::ValidateSkeleton(kafei,21,18,resolve,retained));
    // MM Link wrappers are deliberately unsupported by the global SoH factory.
    // Type rejection is also exercised above on a real SOH::Animation instance.
    REQUIRE(!MmNormalActor::ValidateAnimation(kafei,21,89));
    auto texture=MmAssets_LoadResourceObjectFromMmArchive("objects/object_zov/gLuluEyeOpenTex");
    REQUIRE(MmNormalActor::ValidateTexture(texture));
    auto tex=std::dynamic_pointer_cast<Fast::Texture>(texture);
    auto width=tex->Width;tex->Width=0;
    REQUIRE(!MmNormalActor::ValidateTexture(texture));tex->Width=width;
    auto dataSize=tex->ImageDataSize;tex->ImageDataSize=0xFFFFFFFF;
    REQUIRE(!MmNormalActor::ValidateTexture(texture));tex->ImageDataSize=dataSize;
    REQUIRE(!MmAssets_LoadNormalActor(STATIC_STORY_ACTOR_LULU,4,&disabled));
    std::printf("PASS actual archive loader: %u presentations (10 x Alt off/on), 7 unique faces; type/bounds/identity/retention negatives\n",loads);

    for (bool alt : {false,true}) for (unsigned pose=0;pose<2;++pose) {
        manager->SetAltAssetsEnabled(alt);
        const auto* p=StaticStoryMm_GetPresentation(STATIC_STORY_ACTOR_CHILD_KAFEI,pose);
        auto wrapper=*sMmArchive->LoadFile(p->animationPath)->Buffer;
        auto payload=*sMmArchive->LoadFile(MmKafei::ClipPath(pose)+7)->Buffer;
        auto header=*sMmArchive->LoadFile(p->skeletonPath)->Buffer;
        REQUIRE(MmKafei::Wrapper(wrapper,pose) && MmKafei::Payload(payload,pose) && MmKafei::SkeletonBytes(header));
        for (size_t at : {0U,4U,8U,64U,68U,69U,70U,74U}) {
            auto bad=wrapper;bad[at]^=1;REQUIRE(!MmKafei::Wrapper(bad,pose));
        }
        for (size_t at : {0U,4U,8U,64U,65U,67U}) {
            auto bad=payload;bad[at]^=1;REQUIRE(!MmKafei::Payload(bad,pose));
        }
        for (size_t size : {0U,63U,64U,68U,73U}) {
            auto bad=wrapper;bad.resize(size);REQUIRE(!MmKafei::Wrapper(bad,pose));
            bad=payload;bad.resize(size);REQUIRE(!MmKafei::Payload(bad,pose));
        }
        auto bad=wrapper;bad.push_back(0);REQUIRE(!MmKafei::Wrapper(bad,pose));
        bad=payload;bad.pop_back();REQUIRE(!MmKafei::Payload(bad,pose));
        bad=header;bad[83]^=1;REQUIRE(!MmKafei::SkeletonBytes(bad));
        for (unsigned i=0;i<21;++i) {
            auto bytes=*sMmArchive->LoadFile(MmKafei::Path(MmKafei::Limbs[i].name))->Buffer;
            REQUIRE(MmKafei::LimbBytes(bytes,i));
            bytes[64]=1;REQUIRE(!MmKafei::LimbBytes(bytes,i));bytes[64]=2;
            bytes.back()^=1;REQUIRE(!MmKafei::LimbBytes(bytes,i));
        }
        // Poison every globally selected child. The private header must ignore them.
        for (const auto& row:MmKafei::Limbs) manager->CacheExternalResource(MmKafei::Path(row.name),texture);
        MmNormalActorResources first{},second{};
        REQUIRE(MmAssets_LoadKafei(pose,&first) && MmAssets_LoadKafei(pose,&second));
        REQUIRE(first.skeleton != second.skeleton && first.playerFrames != second.playerFrames);
        REQUIRE(!first.animation && first.skeleton->sh.limbCount==21 && first.skeleton->dListCount==18);
        auto owner=static_cast<std::vector<MmNormalActor::Resource>*>(first.owner);
        REQUIRE(owner->size()==35);
        for(unsigned i=0;i<21;++i) REQUIRE(MmKafei::Limb(std::dynamic_pointer_cast<SOH::SkeletonLimb>(owner->at(i+1)),i));
        for(unsigned i=0;i<8;++i) REQUIRE(first.eyes[i]);
        for(unsigned i=0;i<4;++i) REQUIRE(first.mouths[i]);
        struct { uint32_t before; int16_t joints[66]; uint32_t after; } sample;
        sample.before=0xabcddcba;sample.after=0x12345678;
        uint16_t appearance=0xffff;float cursor=0;
        for(unsigned frame=0;frame<p->frameCount;++frame) {
            cursor=(float)frame;
            REQUIRE(StaticStoryMm_SampleKafei(first.playerFrames,p->frameCount,&cursor,0,sample.joints,&appearance));
            REQUIRE(memcmp(sample.joints,first.playerFrames+frame*67,132)==0 && appearance==0);
            REQUIRE(sample.before==0xabcddcba && sample.after==0x12345678);
        }
        cursor=p->frameCount-1;
        REQUIRE(StaticStoryMm_SampleKafei(first.playerFrames,p->frameCount,&cursor,1,sample.joints,&appearance) && cursor==0);
        REQUIRE(StaticStoryMm_SampleKafei(first.playerFrames,p->frameCount,&cursor,p->frameCount*3+0.5f,sample.joints,&appearance) && cursor==0.5f);
        REQUIRE(StaticStoryMm_SampleKafei(first.playerFrames,p->frameCount,&cursor,-1,sample.joints,&appearance) && cursor==p->frameCount-0.5f);
        cursor=0;
        REQUIRE(StaticStoryMm_SampleKafei(first.playerFrames,p->frameCount,&cursor,-1e-30f,sample.joints,&appearance) && cursor<p->frameCount);
        float saved=cursor;
        REQUIRE(!StaticStoryMm_SampleKafei(first.playerFrames,p->frameCount,&cursor,INFINITY,sample.joints,&appearance) && cursor==saved);
        for(unsigned eye=0;eye<16;++eye) for(unsigned mouth=0;mouth<16;++mouth) {
            auto face=StaticStoryMm_KafeiFace(0xff00|(mouth<<4)|eye);
            REQUIRE(face.eye==(eye>=1 && eye<=8?eye-1:0));
            REQUIRE(face.mouth==(mouth>=1 && mouth<=4?mouth-1:0));
        }
        MmAssets_ReleaseNormalActor(first.owner);sMmResourceCache.clear();
        cursor=0;REQUIRE(StaticStoryMm_SampleKafei(second.playerFrames,p->frameCount,&cursor,1,sample.joints,&appearance));
        MmAssets_ReleaseNormalActor(second.owner);
    }
    REQUIRE(!MmAssets_LoadKafei(2,&disabled));
    puts("PASS Kafei archive: finite private 21 LOD/18 matrix skeleton, two bounded clips, 12 faces, all-frame canaries and lifecycle");
    // Headless Context destructor requires a Window. Explicit resource lifetime checks
    // precede this exit; application shutdown is intentionally outside this fixture.
    sMmResourceCache.clear();retained.clear();sMmArchive.reset();
    std::fflush(nullptr);
    std::_Exit(0);
}
