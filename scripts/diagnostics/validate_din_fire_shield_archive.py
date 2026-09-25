"""Read the delivered bytes independently; reject unsafe references/geometry."""
from pathlib import Path
import hashlib
import json
import struct
import sys
import zipfile


def hash_path(path):
    value = (1 << 64) - 1
    for byte in path.encode("ascii"):
        value ^= byte << 56
        for _ in range(8):
            high = value & (1 << 63)
            value = (value << 1) & ((1 << 64) - 1)
            if high:
                value ^= 0x42F0E1EBA9EA3693
    return value


def validate(path):
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
        assert len(archive.namelist()) == len(set(archive.namelist()))
        files = {name: archive.read(name) for name in archive.namelist()}
    root = "objects/din_fire_shield/poc1/"
    wanted = {root + name for name in ("SurfaceDL", "SurfaceVertices", "RimDL", "RimVertices", "FlameTex", "FlowTex",
                                      "GIBracerDL", "GIBracerVertices", "IconTex")}
    assert set(files) == wanted | {"DinFireShieldPOC1.json"}
    manifest = json.loads(files["DinFireShieldPOC1.json"])
    assert not manifest["overrides"]
    for name in wanted:
        assert manifest["resources"][name] == hashlib.sha256(files[name]).hexdigest()
    for name in ("FlameTex", "FlowTex"):
        tex = files[root + name]
        assert struct.unpack_from("<II", tex, 4) == (0x4F544558, 0)
        assert struct.unpack_from("<IIII", tex, 64) == (6, 64, 32, 2048)
        assert len(tex) == 2128 and max(tex[80:]) > 180
    assert min(files[root + "FlameTex"][80:]) == 0
    icon = files[root + "IconTex"]
    assert struct.unpack_from("<II", icon, 4) == (0x4F544558, 0)
    assert struct.unpack_from("<IIII", icon, 64) == (1, 32, 32, 4096)
    assert len(icon) == 4176 and min(icon[83::4]) == 0 and max(icon[83::4]) == 255
    hashes = {hash_path(name): name for name in files}
    total = 0
    for layer in ("Surface", "Rim", "GIBracer"):
        vertices = files[root + layer + "Vertices"]
        assert struct.unpack_from("<I", vertices, 4)[0] == 0x4F415252
        kind, count = struct.unpack_from("<II", vertices, 64)
        assert kind == 0x19 and len(vertices) == 72 + count * 16
        records = [struct.unpack_from("<hhhHhhBBBB", vertices, 72 + i * 16) for i in range(count)]
        if layer == "GIBracer":
            assert all(all(-60 <= p <= 60 for p in v[:3]) and v[9] == 255 for v in records)
        else:
            assert all(-1500 <= v[0] <= 1500 and -1500 <= v[1] <= 1500 and -200 <= v[2] <= 0 for v in records)
            assert min(v[9] for v in records) == 0 and max(v[9] for v in records) > 100
        data = files[root + layer + "DL"]
        assert struct.unpack_from("<I", data, 4)[0] == 0x4F444C54 and data[64] == 4
        pc = 72
        loaded = 0
        triangles = 0
        ended = False
        while pc < len(data):
            a, b = struct.unpack_from("<II", data, pc)
            op = a >> 24
            pc += 8
            if op == 0x32:
                hi, lo = struct.unpack_from("<II", data, pc); pc += 8
                assert hashes[(hi << 32) | lo] == root + layer + "Vertices"
                loaded = (a >> 12) & 255
                assert 0 < loaded <= 30 and b % 16 == 0 and b + loaded * 16 <= count * 16
                assert ((a & 0xFF) >> 1) == loaded
            elif op == 0x05:
                indices = [(a >> shift) & 255 for shift in (16, 8, 0)]
                assert all(i % 2 == 0 and i // 2 < loaded for i in indices)
                triangles += 1
            elif op == 0xDF:
                assert pc == len(data)
                ended = True
            else:
                raise AssertionError(f"unexpected material/matrix/segment opcode {op:02x}")
        assert ended and triangles * 3 == count
        assert triangles == manifest["geometry"][layer.lower() + "_triangles"]
        total += triangles
    result = {"archive": str(path), "sha256": hashlib.sha256(Path(path).read_bytes()).hexdigest(),
              "private_resources": len(wanted), "triangles": total, "runtime_tested": False}
    print(json.dumps(result, indent=2))
    return result


if __name__ == "__main__":
    validate(Path(sys.argv[1]))
