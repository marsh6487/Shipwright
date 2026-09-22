#include <string.h>
#include "tests/test_require.h"
#include "src/overlays/actors/ovl_En_Elf/z_en_elf.h"
#include "soh/Enhancements/audio/MidnaAudio.h"

Vec3f gSfxDefaultPos;
f32 gSfxDefaultFreqAndVolScale = 1;
s8 gSfxDefaultReverb;
static u16 sCUpInvisible, sCUpTimer;
static int nativeCalls, customCalls, clipPresent, disableCalls;
static u16 nativeId;
static MidnaAudioEvent customEvent;

int32_t CVarGetInteger(const char* name, int32_t fallback) {
    return disableCalls;
}
bool MidnaAudio_TryPlay(MidnaAudioEvent event) {
    ++customCalls;
    customEvent = event;
    return clipPresent;
}
void Audio_PlaySoundGeneral(u16 id, Vec3f* pos, u8 token, f32* freq, f32* vol, s8* reverb) {
    REQUIRE(pos == &gSfxDefaultPos && token == 4);
    REQUIRE(freq == &gSfxDefaultFreqAndVolScale && vol == freq && reverb == &gSfxDefaultReverb);
    ++nativeCalls;
    nativeId = id;
}
void func_800F4524(Vec3f* pos, u16 id, s8 duration) {
    REQUIRE(pos == &gSfxDefaultPos && duration == 32);
    ++nativeCalls;
    nativeId = id;
}
void Audio_PlayActorSound2(Actor* actor, u16 id) {
    ++nativeCalls;
    nativeId = id;
}

/* PRODUCTION_MIDNA_AUDIO_ROUTING */

int main(void) {
    static PlayState play;
    const u16 calls[] = { 0x1E, 0x1D };
    const u16 native[] = { NA_SE_VO_NAVY_CALL, NA_SE_VO_NA_HELLO_2 };
    const MidnaAudioEvent events[] = { MIDNA_AUDIO_CALL, MIDNA_AUDIO_HINT };
    for (int i = 0; i < 2; ++i) {
        for (clipPresent = 0; clipPresent <= 1; ++clipPresent) {
            memset(&play, 0, sizeof(play));
            nativeCalls = customCalls = disableCalls = 0;
            sCUpInvisible = 1;
            Interface_SetNaviCall(&play, calls[i]);
            REQUIRE(customCalls == 1 && customEvent == events[i]);
            REQUIRE(nativeCalls == !clipPresent);
            if (!clipPresent)
                REQUIRE(nativeId == native[i]);
            REQUIRE(play.interfaceCtx.naviCalling && sCUpInvisible == 0 && sCUpTimer == 10);
            Interface_SetNaviCall(&play, calls[i]);
            REQUIRE(customCalls == 1); // repeated HUD polling must not retrigger
            Interface_SetNaviCall(&play, 0x1F);
            REQUIRE(!play.interfaceCtx.naviCalling);
        }
    }
    memset(&play, 0, sizeof(play));
    nativeCalls = customCalls = 0;
    disableCalls = 1;
    Interface_SetNaviCall(&play, 0x1E);
    REQUIRE(customCalls == 0 && nativeCalls == 0 && play.interfaceCtx.naviCalling);
    disableCalls = 0;
    memset(&play, 0, sizeof(play));
    play.csCtx.state = CS_STATE_SKIPPABLE_EXEC;
    Interface_SetNaviCall(&play, 0x1D);
    REQUIRE(customCalls == 0 && nativeCalls == 0 && !play.interfaceCtx.naviCalling);

    /* ACTOR_ROUTING_CHECKS */
    puts("PASS: Navi actor/HUD routing, native arguments, call mute and cutscene gates");
}
