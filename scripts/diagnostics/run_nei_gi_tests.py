"""Compile the GI policy and production renderer against this checkout's real headers."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import sys
import struct
import xml.etree.ElementTree as ET
from run_time_pedestal_tests import functions

ROOT = Path(__file__).resolve().parents[2]
flags = ["-std=c++20", "-DF3DEX_GBI_2", "-DLOG_LEVEL_GAME_PRINTS=0"]
flags += ["-I" + str(ROOT / p) for p in
          ("soh", "soh/include", "soh/src", "soh/assets", "soh/mods", "libultraship/include")]
for config in ("CMake/soh-cvars.cmake", "CMake/lus-cvars.cmake"):
    for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT / config).read_text()):
        flags.append(f'-D{key}="{value}"')
cc = os.environ.get("CXX", "c++")
with tempfile.TemporaryDirectory(prefix="nei-gi-tests-") as tmp:
    draw = functions((ROOT / "soh/src/code/z_draw.c").read_text())
    player = functions((ROOT / "soh/src/code/z_player_lib.c").read_text())
    shop = functions((ROOT / "soh/src/overlays/actors/ovl_En_GirlA/z_en_girla.c").read_text())
    (Path(tmp) / "nei_gi_dispatch.inc").write_text(draw["GetItemEntry_Draw"] + "\n" +
        re.sub(r"\bthis\b", "player", player["Player_DrawGetItemImpl"]) + "\n" +
        re.sub(r"\bthis\b", "shop", shop["EnGirlA_Draw"]))
    fixtures = []
    for slug, callback in (("ball_and_chain", "BallAndChain"), ("shovel", "Shovel"),
                           ("fire_rod", "FireRod"), ("ice_rod", "IceRod"), ("light_rod", "LightRod")):
        root = ROOT / "soh/assets/custom/objects/nei_gi_redesign" / slug
        ys = [int(v.get("Y")) for v in ET.parse(root / "mesh_opa_vtx").getroot()]
        words = struct.unpack_from("<16I", (root / "scale_mtx").read_bytes(), 64)
        scale = ((words[0] >> 16) * 65536 + (words[8] >> 16)) / 65536
        fixtures.append(f'{{Randomizer_Draw{callback}, "{slug}", {min(ys)*scale}f, {max(ys)*scale}f}},')
    (Path(tmp) / "nei_gi_bounds.inc").write_text("\n".join(fixtures))
    names = ["nei_gi/effect_policy", "nei_gi/presentation"]
    if "--held" in sys.argv:
        names.append("nei_held/presentation")
        names.extend(("nei_gi/lantern_policy", "nei_gi/lantern_presentation"))
    for name in names:
        source = ROOT / "tests" / (name + "_test.cpp")
        out = str(Path(tmp) / name.replace("/", "_"))
        subprocess.run([cc, *flags, "-I" + tmp, str(source), "-o", out], check=True)
        subprocess.run([out], check=True)

# Check the actual C dispatch boundary, using the same CVar definitions as CMake.
cflags = ["-std=gnu2x", "-fsyntax-only", "-Werror=implicit-function-declaration",
          "-Wno-incompatible-pointer-types", "-Wno-int-conversion", "-Wno-pointer-to-int-cast"]
cflags += flags[1:]
sources = ["soh/src/code/z_draw.c", "soh/src/code/z_player_lib.c",
           "soh/src/overlays/actors/ovl_En_GirlA/z_en_girla.c"]
if "--held" in sys.argv:
    sources += ["soh/src/overlays/actors/ovl_player_actor/z_player.c", "soh/src/overlays/actors/ovl_Arms_Hook/z_arms_hook.c"]
    for source in ("NeiHeldPresentation.cpp", "NeiLanternPresentation.cpp"):
        subprocess.run([cc, *flags, "-fsyntax-only", str(ROOT / "soh/soh/Enhancements/randomizer" / source)], check=True)
for source in sources:
    subprocess.run([os.environ.get("CC", "cc"), *cflags, str(ROOT / source)], check=True)
print("PASS: real-header common, overhead and shop GI C translation units")
if "--held" in sys.argv:
    print("PASS: held C++ renderers, custom-items unity and hook actor C translation units")
