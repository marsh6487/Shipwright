"""Compile the GI policy and production renderer against this checkout's real headers."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
flags = ["-std=c++20", "-DF3DEX_GBI_2", "-DLOG_LEVEL_GAME_PRINTS=0"]
flags += ["-I" + str(ROOT / p) for p in
          ("soh", "soh/include", "soh/src", "soh/assets", "soh/mods", "libultraship/include")]
cc = os.environ.get("CXX", "c++")
with tempfile.TemporaryDirectory(prefix="nei-gi-tests-") as tmp:
    for name in ("effect_policy", "presentation"):
        source = ROOT / "tests/nei_gi" / (name + "_test.cpp")
        out = str(Path(tmp) / name)
        subprocess.run([cc, *flags, str(source), "-o", out], check=True)
        subprocess.run([out], check=True)

# Check the actual C dispatch boundary, using the same CVar definitions as CMake.
cflags = ["-std=gnu2x", "-fsyntax-only", "-Werror=implicit-function-declaration",
          "-Wno-incompatible-pointer-types", "-Wno-int-conversion", "-Wno-pointer-to-int-cast"]
cflags += flags[1:]
for config in ("CMake/soh-cvars.cmake", "CMake/lus-cvars.cmake"):
    for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT / config).read_text()):
        cflags.append(f'-D{key}="{value}"')
subprocess.run([os.environ.get("CC", "cc"), *cflags, str(ROOT / "soh/src/code/z_draw.c")], check=True)
print("PASS: real-header GI C dispatch syntax")
