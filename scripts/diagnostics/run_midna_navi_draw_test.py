#!/usr/bin/env python3
"""Compile real Navi draw logic against production headers, without a ROM."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
from run_child_ruto_face_test import function

ROOT = Path(__file__).resolve().parents[2]
source_path = ROOT / 'soh/src/overlays/actors/ovl_En_Elf/z_en_elf.c'
source = source_path.read_text()
production = ''
if 'static s32 EnElf_TryDrawMidna(' in source:
    production += function(source, 'EnElf_TryDrawMidna')
production += function(source, 'EnElf_Draw')
fixture = (ROOT / 'soh/tests/midna_navi_draw_test.c').read_text().replace(
    '/* PRODUCTION_MIDNA_DRAW */', production)
flags = ['-std=gnu2x', '-DF3DEX_GBI_2', '-DLOG_LEVEL_GAME_PRINTS=0', '-DNDEBUG',
         '-Werror=implicit-function-declaration', '-Wno-int-conversion',
         '-Wno-incompatible-pointer-types', '-Wno-discarded-qualifiers',
         '-Ilibultraship/include', '-Isoh/include', '-Isoh/src', '-Isoh/assets', '-Isoh', '-Isoh/mods']
for path in ('CMake/soh-cvars.cmake', 'CMake/lus-cvars.cmake'):
    for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT/path).read_text()):
        flags.append(f'-D{key}="{value}"')
with tempfile.TemporaryDirectory(prefix='midna-navi-test-') as folder:
    folder = Path(folder)
    cfile, binary = folder/'draw.c', folder/'draw'
    cfile.write_text(fixture)
    subprocess.run([os.environ.get('CC','cc'), *flags, str(cfile), '-lm', '-o', str(binary)], cwd=ROOT, check=True)
    subprocess.run([str(binary)], cwd=ROOT, check=True)
    subprocess.run([os.environ.get('CC','cc'), *flags, '-fsyntax-only', str(source_path)], cwd=ROOT, check=True)
    print('PASS: entire z_en_elf.c compiles against current integration headers')
