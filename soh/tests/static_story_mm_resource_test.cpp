/* Assembled with the actual production loader functions by mm_ordinary_verify.py. */
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
    auto link=MmAssets_LoadResourceObjectFromMmArchive("objects/gameplay_keep/gPlayerAnim_link_normal_wait_free");
    REQUIRE(!MmNormalActor::ValidateAnimation(link,21,89));
    auto texture=MmAssets_LoadResourceObjectFromMmArchive("objects/object_zov/gLuluEyeOpenTex");
    REQUIRE(MmNormalActor::ValidateTexture(texture));
    auto tex=std::dynamic_pointer_cast<Fast::Texture>(texture);
    auto width=tex->Width;tex->Width=0;
    REQUIRE(!MmNormalActor::ValidateTexture(texture));tex->Width=width;
    auto dataSize=tex->ImageDataSize;tex->ImageDataSize=0xFFFFFFFF;
    REQUIRE(!MmNormalActor::ValidateTexture(texture));tex->ImageDataSize=dataSize;
    REQUIRE(!MmAssets_LoadNormalActor(STATIC_STORY_ACTOR_LULU,4,&disabled));
    std::printf("PASS actual archive loader: %u presentations (10 x Alt off/on), 7 unique faces; type/bounds/identity/retention negatives\n",loads);
    // Headless Context destructor requires a Window. Explicit resource lifetime checks
    // precede this exit; application shutdown is intentionally outside this fixture.
    sMmResourceCache.clear();retained.clear();sMmArchive.reset();
    std::fflush(nullptr);
    std::_Exit(0);
}
