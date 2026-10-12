"""Compile and exercise the actual SoH seasonal state and native renderer."""
import argparse
import hashlib
import os
import re
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
GRAPHICS = ROOT / "soh/soh/Enhancements/Graphics"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--state-only", action="store_true")
    args = parser.parse_args()
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    flags = ["-std=c++20", "-g", "-Wall", "-Wextra", "-Werror", "-I" + str(ROOT / "soh")]
    if args.sanitize:
        flags += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer", "-fno-pie", "-no-pie"]
    with tempfile.TemporaryDirectory(prefix="soh-season-particles-") as temp:
        build = Path(temp)
        for name, source in (("state", "SeasonParticleState.cpp"), ("summer", "SeasonSummerState.cpp"),
                             ("winter", "SeasonWinterState.cpp")):
            test = ROOT / "soh/tests" / f"season_particles_{name}_test.cpp"
            if not test.exists():
                continue
            sources = [str(test)]
            if (GRAPHICS / source).exists():
                sources.append(str(GRAPHICS / source))
                if name == "winter":
                    sources.append(str(GRAPHICS / "SeasonParticleState.cpp"))
            executable = build / name
            subprocess.run([*compiler, *flags, *sources, "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)
        if args.state_only:
            return
        executable = build / "renderer"
        source = (ROOT / "soh/src/code/z_rcp.c").read_text()
        begin = source.index("Gfx sSetupDL[SETUPDL_MAX][6] = {")
        end = source.index("\n};", begin) + 3
        (build / "season_particles_native_setup.inc").write_text(source[begin:end])
        controller = ROOT / "soh/soh/Enhancements/audio/SeasonWeather.cpp"
        contents = controller.read_text()
        predicates = []
        for name in ("SeasonWeather_SeasonForPlay", "SeasonWeather_ParticlesEnabled",
                     "SeasonWeather_FlurriesEnabled", "SeasonWeather_SummerEffectEnabled",
                     "SeasonWeather_WinterEffectEnabled"):
            match = re.search(r'^extern "C" int32_t ' + name + r'\([^;]*?\)\s*\{', contents, re.M)
            assert match, name
            end = match.end()
            depth = 1
            while depth:
                depth += (contents[end] == "{") - (contents[end] == "}")
                end += 1
            line = contents.count("\n", 0, match.start()) + 1
            predicates.append(f'#line {line} "{controller}"\n' + contents[match.start():end])
        (build / "season_particles_controller.inc").write_text("\n".join(predicates))
        # The existing GameInteractor header has2 unused predicate parameters.
        engine_flags = ["-DF3DEX_GBI_2", "-Wno-unused-parameter", "-I" + str(build)]
        for directory in ("soh/include", "soh", "soh/assets", "libultraship/include", "libultraship/src"):
            engine_flags += ["-I" + str(ROOT / directory)]
        engine_flags += shlex.split(os.environ.get("SOH_SEASON_PARTICLE_CXXFLAGS", ""))
        sources = [str(ROOT / "soh/tests/season_particles_renderer_test.cpp")]
        if (GRAPHICS / "SeasonParticles.cpp").exists():
            sources += [str(GRAPHICS / name) for name in
                        ("SeasonParticles.cpp", "SeasonParticlesHooks.cpp", "SeasonParticleState.cpp",
                         "SeasonSummerState.cpp", "SeasonWinterState.cpp")]
        result = subprocess.run([*compiler, *flags, *engine_flags, *sources, "-o", str(executable)],
                                text=True, capture_output=True)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        subprocess.run([str(executable)], check=True)
        for name in ("SeasonParticles.cpp", "SeasonParticlesHooks.cpp"):
            subprocess.run([*compiler, *flags, *engine_flags, "-fsyntax-only", str(GRAPHICS / name)], check=True)
            print("PASS real-header C++20 syntax:", name)
        hashes = {
            "copper_tex": "869debce4a1f6dd7d29836ca33dd65821d2b5650a99429d9b3fd18372fd131f6",
            "crimson_tex": "a0b0420d5f228d9818c732940a5d9b68706007985b0a1b726c5f82d67d5e85bb",
            "gold_tex": "2d579d757e7f3e93c0664af2516315ce89e3d92601c23bb00ec91217004ebd3f",
            "orange_tex": "0e9b93e9e8af2263190d7d93ee0f440bbabc264625d9d720ebdbc07ccfa26c43",
        }
        for name, expected in hashes.items():
            data = (ROOT / "soh/assets/custom/objects/nei_autumn/leaves" / name).read_bytes()
            assert hashlib.sha256(data).hexdigest() == expected, f"donor leaf art changed: {name}"
        print("PASS exact ComboShip980e6fe09 leaf OTEX bytes:4 resources")


if __name__ == "__main__":
    main()
