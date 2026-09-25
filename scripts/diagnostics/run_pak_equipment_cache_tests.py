"""Compile the actual equipment-cache implementation with controlled resource loads.

The fixture replaces only engine state, archive lookup and matrix conversion.
Cache assembly, pointer validation, frame rotation and selection keys are production.
"""
import pathlib
import argparse
import subprocess
import tempfile
from run_time_pedestal_tests import functions

ROOT = pathlib.Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitize", action="store_true", help="enable address and undefined-behavior sanitizers")
    args = parser.parse_args()
    source = (ROOT / "soh/mods/pak_loader/pak_loader.cpp").read_text()
    cache = source[source.index("static std::vector<Gfx*> sRuntimeCombinedDLs;"):
                   source.index("static void MergeTimePedestalSwordPieces")]
    with tempfile.TemporaryDirectory(prefix="pak-equipment-cache-") as temporary:
        directory = pathlib.Path(temporary)
        (directory / "pak_equipment_cache.inc").write_text(cache)
        (directory / "pak_slot_hash.inc").write_text(functions(source, {"SlotMixHash"})["SlotMixHash"])
        resource_source = (ROOT / "soh/soh/ResourceManagerHelpers.cpp").read_text().replace('extern "C" ', '')
        signature_start = resource_source.index("int ResourceMgr_OTRSigCheck(char* imgData) {")
        signature_end = resource_source.index("\n}\n", signature_start) + 3
        (directory / "pak_otr_signature.inc").write_text(resource_source[signature_start:signature_end])
        executable = directory / "test"
        flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"] if args.sanitize else []
        subprocess.run(["c++", "-std=c++20", "-Wall", "-Wextra", *flags, "-DF3DEX_GBI_2", "-I" + str(directory),
                        "-I" + str(ROOT / "libultraship/include"),
                        "-I" + str(ROOT / "soh/assets"), "-I" + str(ROOT / "soh/include"),
                        str(ROOT / "soh/tests/pak_equipment_cache_test.cpp"), "-o", str(executable)], check=True)
        failures = []
        for case in ("deferred", "sheath", "remote", "opcodes", "layers", "pool"):
            result = subprocess.run([str(executable), case])
            if result.returncode:
                failures.append(case)
        if failures:
            raise SystemExit("FAILED: " + ", ".join(failures))


if __name__ == "__main__":
    main()
