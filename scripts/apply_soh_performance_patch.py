#!/usr/bin/env python3
"""Apply the reviewed SoH-only engine patch to the pinned libultraship checkout.

Exact file hashes prevent silently patching a different engine. Reconfiguration
is idempotent; unknown or partially modified inputs are rejected, never reset.
"""
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def apply(root=ROOT):
    engine = root / "libultraship"
    manifest = json.loads((root / "patches/soh-performance.json").read_text())

    def matches(version):
        for name, hashes in manifest.items():
            path = engine / name
            digest = hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest() if path.exists() else None
            if digest != hashes[version]:
                return False
        return True

    if matches("after"):
        return
    if not matches("before"):
        raise RuntimeError("SoH performance patch: engine differs from the pinned baseline; refusing to overwrite it")
    patch = root / "patches/soh-performance.patch"
    subprocess.run(["git", "apply", "--check", str(patch)], cwd=engine, check=True)
    subprocess.run(["git", "apply", str(patch)], cwd=engine, check=True)
    if not matches("after"):
        raise RuntimeError("SoH performance patch verification failed")


if __name__ == "__main__":
    apply()
