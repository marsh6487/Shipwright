#!/usr/bin/env python3
"""Run the real Shadow Scepter code and check its complete player translation unit."""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
flags = ['-std=gnu2x', '-DF3DEX_GBI_2', '-DLOG_LEVEL_GAME_PRINTS=0', '-DNDEBUG',
         '-Werror=implicit-function-declaration', '-Wno-int-conversion',
         '-Wno-incompatible-pointer-types', '-Wno-discarded-qualifiers']
flags += ['-I' + str(ROOT / path) for path in
          ('libultraship/include', 'soh/include', 'soh/src', 'soh/assets', 'soh', 'soh/mods')]
for path in ('CMake/soh-cvars.cmake', 'CMake/lus-cvars.cmake'):
    for key, value in re.findall(r'set\((CVAR_PREFIX_\w+)\s+"?([^\s"\)]+)', (ROOT / path).read_text()):
        flags.append(f'-D{key}="{value}"')
compiler = os.environ.get('CC', 'cc')
with tempfile.TemporaryDirectory(prefix='shadow-scepter-test-') as folder:
    binary = str(Path(folder) / 'shadow-scepter')
    subprocess.run([compiler, *flags, str(ROOT / 'soh/tests/shadow_scepter_test.c'), '-lm', '-o', binary],
                   cwd=ROOT, check=True)
    subprocess.run([binary], cwd=ROOT, check=True)
syntax = subprocess.run([compiler, *flags, '-fsyntax-only',
                         str(ROOT / 'soh/src/overlays/actors/ovl_player_actor/z_player.c')],
                        cwd=ROOT, capture_output=True, text=True)
if syntax.returncode:
    sys.stderr.write(syntax.stderr)
    syntax.check_returncode()
print(f'Player syntax check: {syntax.stderr.count("warning:")} compiler warnings (no errors)')
print('PASS full player translation unit, including all six wand modes, compiles against production headers')
