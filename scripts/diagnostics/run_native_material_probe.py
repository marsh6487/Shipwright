#!/usr/bin/env python3
"""Compile focused tests and inspect real Prelude exports; never modify archives.

Usage: python3 scripts/diagnostics/run_native_material_probe.py --json-include DIR
       --spdlog-include DIR [path/to/vanilla.o2r path/to/retextured.o2r]
Requires initialized libultraship plus a C++20 compiler. No game/ROM is required;
this is CPU validation, not an in-game high-resolution rendering test.
"""
import argparse
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def run(args):
    subprocess.run([str(x) for x in args], cwd=ROOT, check=True)


def fixture(path):
    with zipfile.ZipFile(path) as archive:
        project = json.loads(archive.read('prelude/project/edits.json'))
        items = []
        for edits in project['edits'].values():
            if not isinstance(edits, list):
                continue
            for edit in edits:
                for kind in ('pastes', 'shapes'):
                    for item in edit.get('data', {}).get(kind, []):
                        name = item['newDlPath']
                        if name not in archive.namelist():
                            continue
                        data = archive.read(name)
                        if len(data) < 72 or data[4:8] != b'TLDO':
                            continue
                        words = list(struct.iter_unpack('<II', data[72:]))
                        items.append(dict(path=name, metadata=item, pasted=kind == 'pastes',
                                          ucode=data[64], commands=words))
        return items


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--json-include', required=True)
    parser.add_argument('--spdlog-include', required=True)
    parser.add_argument('--cxx', default='c++')
    parser.add_argument('archives', nargs='*', type=Path)
    args = parser.parse_args()
    core = ROOT / 'soh/soh/Enhancements/Graphics/NativeMaterialProfile.cpp'
    with tempfile.TemporaryDirectory(prefix='prelude-native-probe-') as temp:
        temp = Path(temp)
        common = [args.cxx, '-std=c++20', '-g', '-ffunction-sections', '-fdata-sections',
                  '-Wl,--gc-sections', '-I' + args.json_include]
        for test in ('native_material_scroll_test', 'native_material_export_probe'):
            run(common + [ROOT / f'soh/tests/{test}.cpp', core, '-o', temp / test])
        run([temp / 'native_material_scroll_test'])
        native = (ROOT / 'soh/src/code/z_rcp.c').read_text()
        match = re.search(r'Gfx\* Gfx_TwoTexScrollEx\([^}]+\n}', native)
        if not match:
            raise RuntimeError('Cannot isolate native Gfx_TwoTexScrollEx; inspect source change')
        (temp / 'native_two_tex_scroll.inc').write_text(match.group())
        run(common + ['-DF3DEX_GBI_2', '-D_LANGUAGE_C', '-DCVAR_PREFIX_ENHANCEMENT="gEnhancements"',
                      '-I' + str(ROOT / 'libultraship/include'), '-I' + str(ROOT / 'soh'),
                      '-I' + args.spdlog_include, '-I' + str(temp),
                      ROOT / 'soh/tests/native_material_runtime_test.cpp', core,
                      '-o', temp / 'native_material_runtime_test'])
        run([temp / 'native_material_runtime_test'])
        for path in args.archives:
            data = temp / 'fixture.json'
            data.write_text(json.dumps(fixture(path.resolve())))
            print(f'EXPORT {path.name}', flush=True)
            run([temp / 'native_material_export_probe', data])


if __name__ == '__main__':
    main()
