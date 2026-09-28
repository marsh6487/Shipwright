#!/usr/bin/env python3
"""Compile production triangle submission against a recording graphics backend.

No ROM, game archives, GPU, or third-party headers are needed. The interpreter
header, triangle/combiner functions, and backend interface come from production;
only external resource types and the backend are substituted. --benchmark times
CPU submission, not driver/GPU work or game FPS.
"""
import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def function(source, name):
    match = re.search(r"^(?:template\s*<[^>]+>\s*)?(?:[\w:*<>]+\s+)*" + re.escape(name) +
                      r"\([^;{}]*\)\s*(?:const\s*)?\{", source, re.M)
    if not match:
        raise RuntimeError(f"Missing production function: {name}")
    # Ignore braces in comments and literals while retaining source offsets.
    masked = re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"',
                    lambda m: " " * len(m.group()), source)
    pos, depth = match.end(), 1
    while depth:
        depth += (masked[pos] == "{") - (masked[pos] == "}")
        pos += 1
    return source[match.start():pos] + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--benchmark", action="store_true")
    parser.add_argument("--baseline-ref", help="Compile triangle code/header from this git ref")
    args = parser.parse_args()
    def read(path):
        if args.baseline_ref:
            return subprocess.check_output(["git", "show", f"{args.baseline_ref}:" + path.removeprefix("libultraship/")], cwd=ROOT / "libultraship", text=True)
        return (ROOT / path).read_text(encoding="utf-8")
    source = read("libultraship/src/fast/interpreter.cpp")
    header = read("libultraship/include/fast/interpreter.h")
    api = (ROOT / "libultraship/include/fast/backends/gfx_rendering_api.h").read_text()
    with tempfile.TemporaryDirectory(prefix="triangle-run-") as temporary:
        build = Path(temporary)
        stubs = {
            "fast/interpreter.h": header,
            "fast/backends/gfx_rendering_api.h": api,
            "fast/resource/type/Texture.h": "#pragma once\nnamespace Fast { class Texture; enum class TextureType {}; }\n",
            "ship/resource/Resource.h": "#pragma once\nnamespace Ship { class IResource; }\n",
            "imconfig.h": "#pragma once\nusing ImTextureID = unsigned long long;\n",
            "nlohmann/json.hpp": "#pragma once\n#include <initializer_list>\nnamespace nlohmann { struct json { json() = default; json(std::initializer_list<json>) {} template<class T> json(const T&) {} static json array() { return {}; } }; }\n",
        }
        for path, content in stubs.items():
            target = build / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(content)
        # Unused graphics operations are no-ops; observed operations are overridden
        # in RecordingBackend. Keep the actual virtual ABI and production types.
        methods = re.findall(r"virtual\s+([^;{}]+?)\s*=\s*0\s*;", api, re.S)
        defaults = []
        for method in methods:
            defaults.append(method.strip() + " override {" + ("" if method.strip().startswith("void ") else "return {};") + "}")
        (build / "backend_defaults.inc").write_text("\n".join(defaults))
        helpers = source[source.index("static UcodeHandlers ucode_handler_index"):source.index("static std::string GetPathWithoutFileName")]
        names = ["Interpreter::Interpreter", "Interpreter::~Interpreter", "Interpreter::Flush",
                 "Interpreter::LookupOrCreateShaderProgram", "Interpreter::GenerateCC",
                 "Interpreter::LookupOrCreateColorCombiner", "GetTileSizeFromCoordinates",
                 "Interpreter::GfxSpTri1", "gfx_tri1_otr_handler_f3dex2", "gfx_tri1_handler_f3dex2",
                 "gfx_tri1_handler_f3dex", "gfx_tri1_handler_f3d", "gfx_tri2_handler_f3dex",
                 "gfx_quad_handler_f3dex2", "gfx_quad_handler_f3dex", "Interpreter::SpReset",
                 "Interpreter::NormalizeVector", "Interpreter::TransposedMatrixMul", "Interpreter::CalculateNormalDir",
                 "gfx_set_prim_depth_handler_rdp", "gfx_set_key_r_handler_rdp", "gfx_set_key_gb_handler_rdp"]
        if "Interpreter::PrepareTriangleState(" in source:
            names.insert(names.index("Interpreter::GfxSpTri1"), "Interpreter::PrepareTriangleState")
        if "Interpreter::GfxSpTri1Impl(" in source:
            names.insert(names.index("Interpreter::GfxSpTri1"), "Interpreter::GfxSpTri1Impl")
        # Destructor name needs the same parser, with its tilde included.
        generated = "namespace Fast {\nconstexpr size_t MAX_TRI_BUFFER = 256;\nstatic constexpr float N64_PRIM_DEPTH_MAX = 32767.0f;\n" + helpers
        explicit_dispatch = "(*GfxOpcodeHandlerFunc)(Interpreter*" in source
        generated += "static std::weak_ptr<Interpreter> mInstance;\n"
        generated += re.search(r"typedef bool \(\*GfxOpcodeHandlerFunc\).*?;", source).group(0) + "\n"
        if explicit_dispatch:
            generated += "#define GFX_EXPLICIT_DISPATCH 1\n"
        generated += "\n".join(re.findall(r"^#define C[01].*$", source, re.M)) + "\n"
        generated += "\n".join(function(source, name) for name in names)
        generated += source[source.index("class UcodeHandler {"):source.index("static constexpr UcodeHandler rdpHandlers")]
        generated += "\n}\n"
        generated += function(source, "gfx_cc_get_features")
        (build / "triangle_production.inc").write_text(generated)
        binary = build / "triangle-test"
        flags = shlex.split(os.environ.get("GFX_TRIANGLE_TEST_CXXFLAGS", "-O3"))
        subprocess.run([*shlex.split(os.environ.get("CXX", "c++")), "-std=c++20", *flags,
                        "-I" + str(build), "-I" + str(build / "fast"), "-I" + str(ROOT / "libultraship/include"),
                        str(ROOT / "soh/tests/gfx_triangle_run_test.cpp"), "-o", str(binary)], check=True)
        subprocess.run([str(binary), *(["--benchmark"] if args.benchmark else []), *(["--reference"] if args.baseline_ref else [])], check=True)


if __name__ == "__main__":
    main()
