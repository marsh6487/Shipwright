#include "soh/Enhancements/Graphics/SeasonFoliageTint.h"
#include "test_require.h"
#include <iostream>

static std::shared_ptr<Fast::Texture> Make(Fast::TextureType type, unsigned flags,
                                           std::initializer_list<uint8_t> pixels) {
    auto init = std::make_shared<Ship::ResourceInitData>();
    init->Path = "fixture/foliage";
    auto tex = std::make_shared<Fast::Texture>(init);
    tex->Type = type;
    tex->Flags = flags;
    tex->Width = 2;
    tex->Height = 1;
    tex->HByteScale = 8;
    tex->VPixelScale = 4;
    tex->mImageBuffer = std::make_shared<std::vector<char>>(pixels.begin(), pixels.end());
    tex->ImageData = reinterpret_cast<uint8_t*>(tex->mImageBuffer->data());
    tex->ImageDataSize = pixels.size();
    return tex;
}

int main() {
    using namespace SeasonFoliage;
    const auto hd = Make(Fast::TextureType::RGBA16bpp, TEX_FLAG_LOAD_AS_RAW, { 40, 110, 25, 83, 95, 85, 70, 255 });
    const auto before = *hd->mImageBuffer;
    const auto autumn = BuildTexture(hd, Palette(2, Surface::Ground));
    REQUIRE(autumn && "Djipi raw RGBA pixels must receive autumn despite native RGBA16 type");
    REQUIRE(autumn->ImageData[0] > autumn->ImageData[1] && autumn->ImageData[1] > autumn->ImageData[2]);
    REQUIRE(autumn->ImageData[3] == 83);
    REQUIRE(std::equal(autumn->ImageData + 4, autumn->ImageData + 8, hd->ImageData + 4));
    REQUIRE(*hd->mImageBuffer == before && "source pixels must remain exact");
    REQUIRE(autumn->GetInitData() == hd->GetInitData() && autumn->Type == hd->Type && autumn->Flags == hd->Flags);
    REQUIRE(autumn->Width == hd->Width && autumn->Height == hd->Height);
    REQUIRE(autumn->HByteScale == hd->HByteScale && autumn->VPixelScale == hd->VPixelScale);
    const auto winter = BuildTexture(hd, Palette(3, Surface::Ground));
    REQUIRE(winter && winter->ImageData[2] > winter->ImageData[0]);
    REQUIRE(unsigned(winter->ImageData[0]) + winter->ImageData[1] + winter->ImageData[2] > 40 + 110 + 25);
    REQUIRE(winter->ImageData[3] == hd->ImageData[3]);
    REQUIRE(!BuildTexture(hd, Palette(0, Surface::Ground)) && "spring ground must stay green");
    REQUIRE(!BuildTexture(hd, Palette(1, Surface::Canopy)) && "summer must restore source");
    const auto canopy = Make(Fast::TextureType::RGBA16bpp, TEX_FLAG_LOAD_AS_RAW, { 60, 110, 70, 83, 95, 85, 70, 255 });
    const auto spring = BuildTexture(canopy, Palette(0, Surface::Canopy));
    REQUIRE(spring && spring->ImageData[0] > spring->ImageData[1] && spring->ImageData[2] > spring->ImageData[1]);
    const auto subtle = BuildTexture(hd, Palette(0, Surface::Understory));
    REQUIRE(subtle && subtle->ImageData[1] > subtle->ImageData[0]);
    // Packed RGBA5551 retains both opaque and transparent alpha bits and rock texels.
    const auto native = Make(Fast::TextureType::RGBA16bpp, 0, { 0x26, 0x87, 0x7B, 0xD0 });
    const auto nativeBefore = *native->mImageBuffer;
    const auto nativeAutumn = BuildTexture(native, Palette(2, Surface::Ground));
    REQUIRE(nativeAutumn && (nativeAutumn->ImageData[1] & 1) == 1);
    REQUIRE(nativeAutumn->ImageData[2] == 0x7B && nativeAutumn->ImageData[3] == 0xD0);
    REQUIRE(*native->mImageBuffer == nativeBefore);
    const auto rgba = Make(Fast::TextureType::RGBA32bpp, 0, { 40, 110, 25, 0, 95, 85, 70, 255 });
    REQUIRE(BuildTexture(rgba, Palette(2, Surface::Ground))->ImageData[3] == 0);
    rgba->ImageDataSize = 7;
    REQUIRE(!BuildTexture(rgba, Palette(2, Surface::Ground)));
    rgba->ImageDataSize = 8;
    rgba->Type = Fast::TextureType::Palette8bpp;
    REQUIRE(!BuildTexture(rgba, Palette(2, Surface::Ground)));
    REQUIRE(!BuildTexture(nullptr, Palette(2, Surface::Ground)));
    std::cout << "PASS native/HD seasonal foliage: spring scope, autumn/winter palettes, source/alpha/rock/metadata "
                 "preservation\n";
}
