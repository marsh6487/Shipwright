"""Run the real sword actor, native scripts, age handoff and player placement.

Game state/resource/graphics boundaries are supplied by a small engine fixture.
Only unrelated translation-unit dependencies are omitted; the tested function
bodies are copied verbatim from production, as in the audio runtime tests.
"""
import pathlib
import re
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
ACTOR = ROOT / "soh/src/overlays/actors/ovl_Bg_Toki_Swd"


def functions(source, names=None):
    pattern = r"^(?:static[ \t]+)?[A-Za-z_][\w *]*[ \t]+(\w+)\s*\([^;{}]*\)\s*\{"
    found = {}
    for match in re.finditer(pattern, source, re.M):
        if names is not None and match[1] not in names:
            continue
        start = match.start()
        pos = match.end()
        depth = 1
        while depth:
            depth += (source[pos] == "{") - (source[pos] == "}")
            pos += 1
        found[match[1]] = source[start:pos]
    return found


def without_includes(source):
    return re.sub(r"^#include[^\n]*\n", "", source, flags=re.M)


def block_from(source, start):
    pos = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[pos] == "{") - (source[pos] == "}")
        pos += 1
    return source[start:pos]


def main():
    with tempfile.TemporaryDirectory(prefix="soh-time-pedestal-") as directory:
        build = pathlib.Path(directory)
        (build / "libultraship").mkdir()
        (build / "libultraship/libultra.h").write_text("#pragma once\n")
        (build / "time_pedestal_actor.h").write_text(without_includes((ACTOR / "z_bg_toki_swd.h").read_text()))
        actor_source = (ACTOR / "z_bg_toki_swd.c").read_text()
        source = functions(actor_source)
        actor_body = "\n".join(body[:body.index("{")] + ";" for body in source.values())
        actor_body += "\nstatic int sInitChain[1], sCylinderInit, sColChkInfoInit;\n"
        arrival_state = re.search(r"static struct \{.*?\} sTimePedestalArrival;", actor_source, re.S)
        if arrival_state:
            actor_body += arrival_state[0] + "\n"
        actor_body += "\n".join(source.values())
        (build / "actor.c").write_text('#include "time_pedestal_fixture.h"\n' + actor_body)
        parameter = functions((ROOT / "soh/src/code/z_parameter.c").read_text())
        (build / "parameter.c").write_text('#include "time_pedestal_fixture.h"\n' +
            parameter["Rando_Inventory_SwapAgeEquipment"] + "\n" + parameter["Inventory_SwapAgeEquipment"])
        hud_source = parameter["func_80083108"]
        hud_start = hud_source.index("if (interfaceCtx->restrictions.bButton == 0)")
        hud_zero = block_from(hud_source, hud_start)
        hud_one_start = hud_source.index("if (interfaceCtx->restrictions.bButton == 1)", hud_start)
        hud_one = block_from(hud_source, hud_one_start)
        (build / "hud.c").write_text('#include "time_pedestal_fixture.h"\n' +
            "void Fixture_HudRestore(PlayState* play) { InterfaceContext* interfaceCtx = &play->interfaceCtx; s16 sp28 = 0;\n" +
            hud_zero + " else " + hud_one + "\n}\n" + parameter["func_80084BF4"])
        player_source = (ROOT / "soh/src/overlays/actors/ovl_player_actor/z_player.c").read_text()
        player = functions(player_source)
        player_body = ('#include "time_pedestal_fixture.h"\n' +
            "static Vec3f D_80855198 = { -1.0f, 70.0f, 20.0f };\n" + player["func_808519EC"] + "\n" +
            re.search(r"static struct_808551A4 D_808551A4\[\] = \{.*?^};", player_source, re.M | re.S)[0] + "\n" +
            re.search(r"static AnimSfxEntry D_808551AC\[\] = \{.*?^};", player_source, re.M | re.S)[0] + "\n" +
            player["func_80851A50"])
        player_body += "\nstatic Vec3f D_808546F4 = { -1.0f, 69.0f, 20.0f };\n"
        for name in ("D_808549F0", "D_808549F4"):
            player_body += re.search(r"static AnimSfxEntry " + name + r"\[\] = \{.*?^};", player_source, re.M | re.S)[0] + "\n"
        for name in ("Player_StartMode_TimeTravel", "func_8083C0E8", "func_8084E988",
                     "Player_FinishTimePedestalArrival", "Player_Action_8084E9AC"):
            if name in player:
                player_body += player[name] + "\n"
        init = player["Player_Init"]
        start = init.find("    if (BgTokiSwd_BeginTimePedestalArrival")
        if start < 0:
            start = init.index("    if (GameInteractor_Should(VB_EXECUTE_PLAYER_STARTMODE_FUNC")
        end = init.index("    if (startMode != PLAYER_START_MODE_NOTHING)", start)
        player_body += "static void (*sStartModeFuncs[16])(PlayState*, Player*) = {\n"
        player_body += "[PLAYER_START_MODE_IDLE] = Player_StartMode_Idle, [PLAYER_START_MODE_TIME_TRAVEL] = Player_StartMode_TimeTravel };\n"
        player_body += "void Fixture_PlayerStartMode(PlayState* play, s32 startMode) { Player* this = GET_PLAYER(play);\n" + init[start:end] + "}\n"
        (build / "player.c").write_text(player_body)
        render = functions((ROOT / "soh/src/code/z_player_lib.c").read_text())
        # Exercise the actual final override order, with incoming hand/model DLs
        # and the resource manager supplied at the graphics boundary.
        tail = render["Player_OverrideLimbDrawGameplayDefault"]
        tail = tail[tail.index("    GameInteractor_Should(VB_PLAYER_OVERRIDE_LIMB_DRAW"):]
        helpers = "static s32 sDListsLodOffset;\n" + render["Player_ApplyBackEquipmentVisibility"] + "\n"
        if "Player_ReverseTimePedestalEquipmentSword" in render:
            helpers += render["Player_ReverseTimePedestalEquipmentSword"] + "\n"
        if "Player_ApplyTimePedestalSword" in render:
            helpers += render["Player_ApplyTimePedestalSword"] + "\n"
        (build / "render.c").write_text('#include "time_pedestal_fixture.h"\n' + helpers +
            "static s32 Fixture_RenderTail(PlayState* play, Player* this, s32 limbIndex, Gfx** dList, Vec3s* rot) {\n"
            "void* thisx = this;\n" + tail + "\n"
            "void Fixture_ApplyLateHandOverrides(PlayState* p, Player* player, s32 limb, Gfx** dl) {\n"
            "Vec3s rot = { 0 }; Fixture_RenderTail(p, player, limb, dl, &rot);\n}\n"
            "void Fixture_ApplyLateHandOverridesWithRot(PlayState* p, Player* player, s32 limb, Gfx** dl, Vec3s* rot) {\n"
            "Fixture_RenderTail(p, player, limb, dl, rot);\n}\n")
        pak_source = (ROOT / "soh/mods/pak_loader/pak_loader.cpp").read_text().replace('extern "C" ', '')
        pak = functions(pak_source, {"FindEquip", "PakLoader_GetEquipDL", "PakLoader_UsedCombinedDL"})
        (build / "pak.cpp").write_text('#include "time_pedestal_fixture.h"\n' +
            "extern std::map<u32, Gfx*> fixturePakEquipment;\n"
            "static int sForcedModelIndex = -1, sSelectedEquipIndex = -1, sForcedEquipIndex = -1;\n"
            "static u8 sPakLeftHandCombined, sPakRightHandCombined;\n"
            "static void EnsureSlotMixLoaded() {}\n"
            "static bool AnySlotMixActive() { return PakLoader_HasActiveModel(); }\n"
            "static std::map<u32, Gfx*>* sGetEquipDLs() { return &fixturePakEquipment; }\n"
            "static bool IsValidGfxPtrOrOtrPath(Gfx* p) { return (uintptr_t)p > 4096; }\n"
            "#define PAK_LOG(...) ((void)0)\n" + pak["FindEquip"] + "\n" +
            pak["PakLoader_GetEquipDL"] + "\n" + pak["PakLoader_UsedCombinedDL"])
        custom = functions((ROOT / "soh/soh/Enhancements/customequipment.cpp").read_text().replace('extern "C" ', ''),
                           {"LoadGfxByName", "LoadCustomGfx", "CustomEquipment_OverrideMasterSwordHand"})
        (build / "custom.cpp").write_text('#include "time_pedestal_fixture.h"\n' +
            "static const char* ResolveCustomFPSHand(const char* path) { return path; }\n"
            "#define BuildHandItemDL Fixture_BuildHandItemDL\n" + "\n".join(custom.values()))
        play_destroy = functions((ROOT / "soh/src/code/z_play.c").read_text())["Play_Destroy"]
        start = play_destroy.index("if (gSaveContext.linkAge != play->linkAgeOnLoad)")
        (build / "handoff.c").write_text('#include "time_pedestal_fixture.h"\n' +
            "void Fixture_PlayDestroyAgeHandoff(PlayState* play) { Player* player = GET_PLAYER(play);\n" +
            block_from(play_destroy, start) + "\n}\n")
        (build / "save.c").write_text('#include "time_pedestal_fixture.h"\n' +
            functions((ROOT / "soh/src/code/z_play.c").read_text())["Play_PerformSave"])
        sram = functions((ROOT / "soh/src/code/z_sram.c").read_text())["Sram_OpenSave"]
        repair_start = sram.index("if (LINK_AGE_IN_YEARS == YEARS_ADULT && !CHECK_OWNED_EQUIP")
        repair_end = sram.index("if (GameInteractor_Should(VB_REVERT_SPOILING_ITEMS", repair_start)
        (build / "reload.c").write_text('#include "time_pedestal_fixture.h"\n' +
            "void Fixture_AdultLoadRepair(void) {\n" + sram[repair_start:repair_end] + "\n}\n")
        give = parameter["Item_Give"]
        sword_start = give.index("if ((item >= ITEM_SWORD_KOKIRI) && (item <= ITEM_SWORD_BGS))")
        (build / "ownership.c").write_text('#include "time_pedestal_fixture.h"\n' +
            "u8 Fixture_GiveSword(PlayState* play, u8 item) {\n" + block_from(give, sword_start) +
            "\nreturn ITEM_NONE;\n}\n" +
            functions((ROOT / "soh/src/code/z_inventory.c").read_text())["Inventory_DeleteEquipment"])
        metadata = functions((ROOT / "soh/mods/nei_save.cpp").read_text())
        (build / "metadata.cpp").write_text('#include "time_pedestal_fixture.h"\n' +
            "static NeiSaveData& gNeiSave = *Nei_Save();\n" + metadata["NeiSave_Save"] + "\n" + metadata["NeiSave_Load"])
        (build / "music.c").write_text('#include "time_pedestal_fixture.h"\n' +
            functions((ROOT / "soh/src/code/z_kankyo.c").read_text())["Environment_PlaySceneSequence"])
        ext_source = (ROOT / "soh/mods/extended_equipment.c").read_text()
        ext = functions(ext_source)
        ext_names = ["ExtEquip_GetAgeReq", "ExtEquip_CheckAgeReq", "ExtEquip_SetCurrentByType", "ExtEquip_GetBit",
                     "ExtEquip_HasItem", "ExtEquip_OwnedShieldBase", "ExtEquip_ApplyVanillaBase",
                     "ExtEquip_TridentAllowsShield", "ExtEquip_ApplyTridentShieldPolicy", "ExtEquip_SetSlot",
                     "ExtEquip_ValidateForAge", "ExtEquip_GetCurrent", "ExtEquip_GetItemId"]
        if "ExtEquip_ValidateForAgeWithoutProgression" in ext:
            ext_names.append("ExtEquip_ValidateForAgeWithoutProgression")
        ext_body = "\n".join(ext[name][:ext[name].index("{")] + ";" for name in ext_names)
        ext_body += "\n" + re.search(r"static const u8 sExtEquipAgeReqs\[4\]\[3\] = \{.*?^};", ext_source, re.M | re.S)[0]
        ext_body += "\n" + "\n".join(ext[name] for name in ext_names)
        (build / "extended.c").write_text('#include "time_pedestal_fixture.h"\n' + ext_body)
        (build / "switch.cpp").write_text('#include "time_pedestal_fixture.h"\n' +
            without_includes((ROOT / "soh/soh/Enhancements/SwitchAge.cpp").read_text()))
        arrays = '#include "time_pedestal_fixture.h"\n#include "z64cutscene_commands.h"\n'
        for number in (1, 2, 3):
            arrays += without_includes((ACTOR / f"z_bg_toki_swd_cutscene_data_{number}.c").read_text())
        (build / "scripts.c").write_text(arrays)
        includes = ["-I" + str(p) for p in (build, ROOT / "soh/tests", ROOT / "soh/include", ROOT / "soh", ACTOR)]
        sources = [build / name for name in
                   ("actor.c", "parameter.c", "player.c", "render.c", "scripts.c", "handoff.c", "extended.c", "hud.c", "save.c", "music.c", "reload.c", "ownership.c")]
        helper = ACTOR / "time_pedestal_cutscene.c"
        if helper.exists():
            (build / "cutscene.c").write_text('#include "time_pedestal_fixture.h"\n' + without_includes(helper.read_text()))
            sources.append(build / "cutscene.c")
        objects = []
        for source_path in sources:
            obj = source_path.with_suffix(".o")
            subprocess.run(["cc", "-std=c11", "-g", "-Wno-missing-braces", "-Werror=implicit-function-declaration", *includes,
                            "-c", str(source_path), "-o", str(obj)], check=True)
            objects.append(str(obj))
        executable = build / "time_pedestal_test"
        subprocess.run(["c++", "-std=c++20", "-g", "-Wall", *includes, str(build / "switch.cpp"),
                        str(build / "metadata.cpp"), str(build / "pak.cpp"), str(build / "custom.cpp"),
                        str(ROOT / "soh/tests/time_pedestal_test.cpp"), *objects, "-lm", "-o", str(executable)], check=True)
        subprocess.run([str(executable), *sys.argv[1:]], check=True)


if __name__ == "__main__":
    main()
