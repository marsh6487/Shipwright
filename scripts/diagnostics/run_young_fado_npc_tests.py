"""Compile both real NPC adapters and check optional Young Fado asset routing."""
import os
from pathlib import Path
import re
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]


def main():
    args = [os.environ.get("CC", "cc"), "-std=gnu2x", "-O2",
            "-Werror=implicit-function-declaration", "-Wno-incompatible-pointer-types",
            "-Wno-int-conversion", "-Wno-discarded-qualifiers",
            "-DLOG_LEVEL_GAME_PRINTS=0", "-DF3DEX_GBI_2"]
    args += ["-I" + str(ROOT / path) for path in
             ("soh/include", "soh/src", "soh/assets", "soh", "soh/mods", "libultraship/include")]
    for path in ("CMake/soh-cvars.cmake", "CMake/lus-cvars.cmake"):
        for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT / path).read_text()):
            args.append(f'-D{key}="{value}"')
    with tempfile.TemporaryDirectory(prefix="young-fado-npc-") as folder:
        build = Path(folder)
        for number, path in enumerate(("soh/src/overlays/actors/ovl_En_Ko/z_en_ko.c",
                                       "soh/src/overlays/actors/ovl_En_Viewer/static_story_kokiri.c")):
            subprocess.run([*args, "-c", str(ROOT / path), "-o", str(build / f"actor-{number}.o")], check=True)
            print("PASS real-header actor object compilation:", path, flush=True)
        binary = build / "routing-test"
        subprocess.run([*args, str(ROOT / "soh/tests/young_fado_npc_routing_test.c"),
                        "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
