#!/usr/bin/env python3
"""Compile every production command table with the selected compiler (including GCC 11).

Only handler bodies and interpreter state are fixtures; all six tables and their
constructor/invoke code are extracted unchanged from the engine.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / 'libultraship/src/fast/interpreter.cpp').read_text()
production = source[source.index('class UcodeHandler {'):source.index('const char* GfxGetOpcodeName')]
handlers = sorted(set(re.findall(r'\bgfx_\w+\b', production)))
triangles = {'gfx_tri1_otr_handler_f3dex2', 'gfx_tri1_handler_f3dex2', 'gfx_tri1_handler_f3dex',
             'gfx_tri1_handler_f3d', 'gfx_tri2_handler_f3dex', 'gfx_quad_handler_f3dex2', 'gfx_quad_handler_f3dex'}
prefix = '''#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <iterator>
#include <limits>
#include <utility>
#include <fast/lus_gbi.h>
namespace Fast {
struct Interpreter {
    struct { bool valid = true; } mTriangleState;
    bool mTriangleStateReuseEnabled = true;
    bool mTriangleStateReuseAllowed = false;
};
using GfxOpcodeHandlerFunc = bool (*)(Interpreter*, F3DGfx**);
'''
for name in handlers:
    expected = 'true' if name in triangles else 'false'
    prefix += f'bool {name}(Interpreter* gfx, F3DGfx**) {{ assert(gfx->mTriangleStateReuseAllowed == (gfx->mTriangleStateReuseEnabled && {expected})); return false; }}\n'
suffix = '''
}
int main() {
    using namespace Fast;
    assert(ucode_handlers.size() == 6);
    assert(ucode_handlers[0] == &f3dHandlers && ucode_handlers[1] == &f3dHandlers);
    assert(ucode_handlers[2] == &f3dexHandlers && ucode_handlers[3] == &f3dexHandlers);
    assert(ucode_handlers[4] == &f3dex2Handlers && ucode_handlers[5] == &s2dexHandlers);
    Interpreter gfx;
    F3DGfx packet, *cursor = &packet;
    unsigned count = 0;
    for (const auto* table : { &rdpHandlers, &otrHandlers, &f3dex2Handlers, &f3dexHandlers, &f3dHandlers, &s2dexHandlers }) {
        for (int opcode = -128; opcode < 128; ++opcode) {
            if (!table->contains(static_cast<int8_t>(opcode))) continue;
            for (bool enabled : {false, true}) {
                gfx.mTriangleStateReuseEnabled = enabled;
                table->invoke(static_cast<int8_t>(opcode), &cursor, &gfx);
                assert(!gfx.mTriangleStateReuseAllowed);
            }
            ++count;
        }
    }
    assert(count > 100);
    std::printf("PASS all six production dispatch tables: %u registered commands, enabled/disabled boundaries\\n", count);
}
'''
with tempfile.TemporaryDirectory(prefix='gfx-dispatch-') as directory:
    build = Path(directory)
    cpp = build / 'test.cpp'
    cpp.write_text(prefix + production + suffix)
    exe = build / 'test'
    subprocess.run([*shlex.split(os.environ.get('CXX', 'c++')), '-std=c++20', '-O2',
                    '-Wall', '-Wextra', '-Werror', '-I' + str(ROOT / 'libultraship/include'), str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
