#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "global.h"
#include "soh/Enhancements/cosmetics/YoungFadoNpc.h"

static unsigned sAssets;

bool ResourceMgr_IsAltAssetsEnabled(void) {
    return (sAssets & 1) != 0;
}

uint8_t ResourceMgr_FileExists(const char* path) {
    return (sAssets & 2) && strcmp(path, "__OTR__objects/object_fa/YoungFadoNpcPOC1/Skel") == 0;
}

uint8_t ResourceMgr_FileAltExists(const char* path) {
    if (strcmp(path, "__OTR__objects/object_fa/YoungFadoNpcPOC1/Skel") == 0) {
        return (sAssets & 4) != 0;
    }
    if (strcmp(path, "__OTR__objects/object_fa/YoungFadoNpcPOC1/Body/Limb14") == 0) {
        return (sAssets & 8) != 0;
    }
    return (sAssets & 16) && strstr(path, "/Seated") != NULL;
}

int main(void) {
    const char* nativeSkeleton = "__OTR__objects/object_kw1/gKw1Skel";
    Gfx nativeHead[1];
    Gfx nativeBody[1];
    unsigned checks = 0;

    for (sAssets = 0; sAssets < 32; ++sAssets) {
        bool active = (sAssets & 15) == 15;
        assert(YoungFadoNpc_IsActive() == active);
        const char* skel = YoungFadoNpc_SelectSkeleton(nativeSkeleton);
        assert((skel == nativeSkeleton) == ((sAssets & 2) == 0));
        if (sAssets & 2) {
            assert(strcmp(skel, "__OTR__objects/object_fa/YoungFadoNpcPOC1/Skel") == 0);
        }
        Gfx* head = YoungFadoNpc_SelectHead(nativeHead);
        assert((head != nativeHead) == active);
        if (active) {
            assert(strcmp((const char*)head, "__OTR__objects/object_fa/YoungFadoNpcPOC1/Body/Limb14") == 0);
        }
        for (u8 pose = 0; pose <= 8; ++pose) {
            for (s32 limb = -1; limb <= 16; ++limb) {
                Gfx* body = YoungFadoNpc_SelectSeatedBody(nativeBody, limb, pose);
                bool replaced =
                    active && (sAssets & 16) && (pose == 4 || pose == 5) && (limb == 1 || limb == 2 || limb == 5);
                assert((body != nativeBody) == replaced);
                if (replaced) {
                    char expected[100];
                    snprintf(expected, sizeof(expected), "__OTR__objects/object_fa/YoungFadoNpcPOC1/Seated%d/Limb%02d",
                             pose, limb - 1);
                    assert(strcmp((const char*)body, expected) == 0);
                    assert((uintptr_t)body % 2 == 0);
                }
                ++checks;
            }
        }
    }
    printf("PASS %u routing cases: absent/partial packs, Alt off/on, native fallback, seated limb slots.\n", checks);
    return 0;
}
