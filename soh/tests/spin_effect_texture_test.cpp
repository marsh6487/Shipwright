#include "fast/resource/factory/TextureFactory.h"
#include "fast/resource/type/Texture.h"
#include "ship/utils/binarytools/MemoryStream.h"
#ifndef SPIN_BASELINE_FACTORY
#include "../soh/resource/importer/SpinEffectTextureFactory.h"
#endif
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#define CHECK(c)                                                      \
    do {                                                              \
        if (!(c)) {                                                   \
            std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); \
            std::exit(1);                                             \
        }                                                             \
    } while (0)

static void Append(std::vector<char>& buffer, uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        buffer.push_back(static_cast<char>(value >> shift));
}

struct Fixture {
    std::shared_ptr<Ship::File> file;
    std::shared_ptr<Ship::ResourceInitData> init;
    std::vector<uint8_t> pixels;
    uint32_t version = 0;
};

static Fixture Make(const std::string& path, uint32_t width, uint32_t height, uint32_t type = 6) {
    Fixture f;
    f.init = std::make_shared<Ship::ResourceInitData>();
    f.init->Path = path;
    f.init->Format = RESOURCE_FORMAT_BINARY;
    f.file = std::make_shared<Ship::File>();
    f.file->Buffer = std::make_shared<std::vector<char>>();
    auto& b = *f.file->Buffer;
    Append(b, type);
    Append(b, width);
    Append(b, height);
    Append(b, width * height);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            // A distinct row/column signal exposes low-resolution row-stride mistakes.
            uint8_t v = static_cast<uint8_t>((x * 17 + y * 31) & 255);
            b.push_back(static_cast<char>(v));
            f.pixels.push_back(v);
        }
    }
    auto reader = std::make_shared<Ship::BinaryReader>(std::make_shared<Ship::MemoryStream>(f.file->Buffer));
    reader->SetEndianness(Ship::Endianness::Little);
    f.file->Reader = reader;
    return f;
}

static Fixture MakeV1(const std::string& path, uint32_t width, uint32_t height, float hs, float vs,
                      uint32_t flags = TEX_FLAG_LOAD_AS_RAW, uint32_t type = 6) {
    auto f = Make(path, width, height, type);
    f.version = 1;
    auto& b = *f.file->Buffer;
    b.clear();
    f.pixels.clear();
    Append(b, type);
    Append(b, width);
    Append(b, height);
    Append(b, flags);
    Append(b, std::bit_cast<uint32_t>(hs));
    Append(b, std::bit_cast<uint32_t>(vs));
    Append(b, width * height * 4);
    for (uint32_t i = 0; i < width * height * 4; ++i) {
        const auto v = static_cast<uint8_t>((i * 31 + i / width) & 255);
        b.push_back(static_cast<char>(v));
        f.pixels.push_back(v);
    }
    auto reader = std::make_shared<Ship::BinaryReader>(std::make_shared<Ship::MemoryStream>(f.file->Buffer));
    reader->SetEndianness(Ship::Endianness::Little);
    f.file->Reader = reader;
    return f;
}

static std::shared_ptr<Fast::Texture> Load(Fixture& f) {
#ifdef SPIN_BASELINE_FACTORY
    Fast::ResourceFactoryBinaryTextureV0 factory;
    Fast::ResourceFactoryBinaryTextureV1 factoryV1;
#else
    SOH::SpinEffectTextureFactoryV0 factory;
    SOH::SpinEffectTextureFactoryV1 factoryV1;
#endif
    if (f.version == 1)
        return std::dynamic_pointer_cast<Fast::Texture>(factoryV1.ReadResource(f.file, f.init));
    return std::dynamic_pointer_cast<Fast::Texture>(factory.ReadResource(f.file, f.init));
}

static void Promoted(const std::string& path, uint32_t nw, uint32_t nh, uint32_t scale) {
    auto f = Make(path, nw * scale, nh * scale);
    auto t = Load(f);
    CHECK(t != nullptr);
    CHECK(t->Flags == (TEX_FLAG_LOAD_AS_RAW | TEX_FLAG_LOAD_AS_IMG));
    // Non-unit scales trigger GfxSpTri's HD clamp to the actor's 8x8 scroll
    // tile. Whole-image upload with unit scales retains the native UV domain.
    CHECK(t->HByteScale == 1.0f);
    CHECK(t->VPixelScale == 1.0f);
    CHECK(t->ImageDataSize == f.pixels.size() * 4);
    CHECK(t->Type == Fast::TextureType::Grayscale8bpp);
    // The renderer must receive every source row, with intensity in all channels,
    // particularly alpha: forcing opaque alpha would expose the polygon as a box.
    for (size_t i = 0; i < f.pixels.size(); ++i)
        for (size_t c = 0; c < 4; ++c)
            CHECK(t->ImageData[i * 4 + c] == f.pixels[i]);
    auto data = t->ImageData;
    f.file.reset();
    f.init.reset();
    CHECK(t->ImageData == data);
    CHECK(t->ImageData[4 * (nw * scale + 9)] == f.pixels[nw * scale + 9]);
    std::printf("PASS promoted %s x%u; row stride, intensity alpha, buffer lifetime\n", path.c_str(), scale);
}

static void Unchanged(const std::string& path, uint32_t w, uint32_t h, uint32_t type = 6) {
    auto f = Make(path, w, h, type);
    auto t = Load(f);
    CHECK(t != nullptr);
    CHECK(t->Flags == 0);
    CHECK(t->HByteScale == 1 && t->VPixelScale == 1);
    CHECK(t->ImageDataSize == f.pixels.size());
    CHECK(t->mImageBuffer == f.file->Buffer);
    CHECK(std::memcmp(t->ImageData, f.pixels.data(), f.pixels.size()) == 0);
}

static void CheckV1(Fixture f, uint32_t flags, float hs, float vs) {
    auto t = Load(f);
    CHECK(t != nullptr);
    CHECK(t->Flags == flags && t->HByteScale == hs && t->VPixelScale == vs);
    CHECK(t->ImageDataSize == f.pixels.size());
    CHECK(t->mImageBuffer == f.file->Buffer);
    CHECK(std::memcmp(t->ImageData, f.pixels.data(), f.pixels.size()) == 0);
    auto data = t->ImageData;
    f.file.reset();
    f.init.reset();
    CHECK(t->ImageData == data);
    CHECK(std::memcmp(t->ImageData, f.pixels.data(), f.pixels.size()) == 0);
}

int main() {
    Promoted("alt/objects/gameplay_keep/gTorchFlameTex", 64, 32, 4);
    Promoted("objects/gameplay_keep/gEffUnknown1Tex", 64, 32, 16);
    Promoted("alt/objects/gameplay_keep/gFlameWall1Tex", 32, 32, 4);
    Promoted("objects/gameplay_keep/gFlameWall2Tex", 32, 32, 4);
    Unchanged("objects/gameplay_keep/gTorchFlameTex", 64, 32);
    Unchanged("alt/objects/gameplay_keep/gFlameWall1Tex", 32, 32);
    Unchanged("objects/gameplay_keep/gDecorativeFlameTex", 256, 128);
    Unchanged("other/gameplay_keep/gTorchFlameTex", 256, 128);
    Unchanged("objects/gameplay_keep/gTorchFlameTex", 128, 128);
    Unchanged("objects/gameplay_keep/gTorchFlameTex", 100, 50);
    Unchanged("objects/gameplay_keep/gTorchFlameTex", 256, 128, 1);
    std::puts("PASS native textures, unrelated paths/formats and unsupported dimensions unchanged");
    constexpr uint32_t imageFlags = TEX_FLAG_LOAD_AS_RAW | TEX_FLAG_LOAD_AS_IMG;
    CheckV1(MakeV1("objects/gameplay_keep/gTorchFlameTex", 256, 128, 16, 4), imageFlags, 1, 1);
    CheckV1(MakeV1("alt/objects/gameplay_keep/gEffUnknown1Tex", 1024, 512, 64, 16), imageFlags, 1, 1);
    CheckV1(MakeV1("objects/gameplay_keep/gFlameWall1Tex", 128, 128, 16, 4), imageFlags, 1, 1);
    CheckV1(MakeV1("alt/objects/gameplay_keep/gFlameWall2Tex", 128, 128, 16, 4), imageFlags, 1, 1);
    std::puts("PASS V1 raw replacements use whole-image upload; authored RGBA and ownership preserved");
    CheckV1(MakeV1("objects/gameplay_keep/gTorchFlameTex", 64, 32, 4, 1), TEX_FLAG_LOAD_AS_RAW, 4, 1);
    CheckV1(MakeV1("objects/gameplay_keep/gOtherTex", 256, 128, 16, 4), TEX_FLAG_LOAD_AS_RAW, 16, 4);
    CheckV1(MakeV1("objects/gameplay_keep/gTorchFlameTex", 256, 128, 16, 4, TEX_FLAG_LOAD_AS_RAW, 1),
            TEX_FLAG_LOAD_AS_RAW, 16, 4);
    CheckV1(MakeV1("objects/gameplay_keep/gTorchFlameTex", 128, 128, 8, 4), TEX_FLAG_LOAD_AS_RAW, 8, 4);
    CheckV1(MakeV1("objects/gameplay_keep/gTorchFlameTex", 256, 128, 8, 4), TEX_FLAG_LOAD_AS_RAW, 8, 4);
    CheckV1(MakeV1("objects/gameplay_keep/gTorchFlameTex", 256, 128, 16, 2), TEX_FLAG_LOAD_AS_RAW, 16, 2);
    CheckV1(MakeV1("objects/gameplay_keep/gTorchFlameTex", 256, 128, 1, 1, imageFlags), imageFlags, 1, 1);
    CheckV1(MakeV1("objects/gameplay_keep/gTorchFlameTex", 256, 128, 16, 4, 0), 0, 16, 4);
    std::puts("PASS V1 native images, unrelated paths/formats and explicit flags/scales unchanged");
#ifndef SPIN_BASELINE_FACTORY
    auto f = Make("alt/objects/gameplay_keep/gFlameWall1Tex", 128, 128);
    f.file->Buffer->pop_back(); // Header claims a full image but payload is truncated.
    CHECK(Load(f) == nullptr);
    auto v1 = MakeV1("objects/gameplay_keep/gEffUnknown1Tex", 1024, 512, 64, 16);
    v1.file->Buffer->pop_back();
    CHECK(Load(v1) == nullptr);
    std::puts("PASS truncated V0 and V1 candidates rejected");
#endif
}
