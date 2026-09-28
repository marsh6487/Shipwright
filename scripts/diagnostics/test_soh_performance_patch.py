#!/usr/bin/env python3
"""Verify clean-checkout application, repeat configuration, and drift rejection."""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('apply_patch', ROOT / 'scripts/apply_soh_performance_patch.py')
patcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patcher)

class PatchTests(unittest.TestCase):
    def test_clean_repeat_and_drift(self):
        with tempfile.TemporaryDirectory(prefix='soh-engine-patch-') as directory:
            root = Path(directory)
            engine = root / 'libultraship'
            engine.mkdir()
            subprocess.run(['git', 'init', '-q', str(engine)], check=True)
            shutil.copytree(ROOT / 'patches', root / 'patches')
            manifest = json.loads((root / 'patches/soh-performance.json').read_text())
            for name, hashes in manifest.items():
                if hashes['before'] is None:
                    continue
                data = subprocess.check_output(['git', 'show', 'c57da1b4afa775b24b58b2adf93d63d3b561bb65:' + name], cwd=ROOT / 'libultraship')
                path = engine / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
            patcher.apply(root)
            patcher.apply(root)
            for name, hashes in manifest.items():
                self.assertEqual(hashlib.sha256((engine / name).read_bytes()).hexdigest(), hashes['after'])
            target = engine / 'src/fast/interpreter.cpp'
            target.write_text(target.read_text() + '\n// unrelated user edit\n')
            preserved = target.read_bytes()
            with self.assertRaisesRegex(RuntimeError, 'refusing to overwrite'):
                patcher.apply(root)
            self.assertEqual(target.read_bytes(), preserved)

if __name__ == '__main__':
    unittest.main()
