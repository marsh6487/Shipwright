#include "SpinEffectTextureFactory.h"

#include "fast/resource/factory/TextureFactory.h"
#include "fast/resource/type/Texture.h"
#include "spdlog/spdlog.h"
#include <string_view>
#include <vector>

namespace SOH {
namespace {

uint32_t GetNativeWidth(std::string_view path, const Fast::Texture& texture) {
    if (path.starts_with("alt/")) {
        path.remove_prefix(4);
    }

    uint32_t nativeWidth;
    constexpr uint32_t nativeHeight = 32;
    if (path == "objects/gameplay_keep/gTorchFlameTex" || path == "objects/gameplay_keep/gEffUnknown1Tex") {
        nativeWidth = 64;
    } else if (path == "objects/gameplay_keep/gFlameWall1Tex" || path == "objects/gameplay_keep/gFlameWall2Tex") {
        nativeWidth = 32;
    } else {
        return 0;
    }

    // Do not reinterpret other formats, native-size images, or reshaped textures.
    if (texture.Type != Fast::TextureType::Grayscale8bpp || texture.Width <= nativeWidth || texture.Width > 4096 ||
        texture.Height > 4096 || texture.Width % nativeWidth != 0 || texture.Height % nativeHeight != 0 ||
        texture.Width / nativeWidth != texture.Height / nativeHeight) {
        return 0;
    }
    return nativeWidth;
}

bool HasCompletePayload(const Ship::File& file, const Fast::Texture& texture, size_t bytes) {
    const size_t offset = reinterpret_cast<const char*>(texture.ImageData) - file.Buffer->data();
    return texture.ImageDataSize == bytes && offset <= file.Buffer->size() && bytes <= file.Buffer->size() - offset;
}

void UseWholeImage(Fast::Texture& texture) {
    // These four native effect textures are always loaded as whole images.
    // IMG uploads the physical RGBA dimensions directly. Unit scales retain
    // native byte accounting and avoid the HD UV clamp to the actor's 8x8
    // scroll tile. RAW also makes resource previews decode RGBA correctly.
    texture.Flags = TEX_FLAG_LOAD_AS_RAW | TEX_FLAG_LOAD_AS_IMG;
    texture.HByteScale = 1.0f;
    texture.VPixelScale = 1.0f;
}

} // namespace

std::shared_ptr<Ship::IResource>
SpinEffectTextureFactoryV0::ReadResource(std::shared_ptr<Ship::File> file,
                                         std::shared_ptr<Ship::ResourceInitData> initData) {
    Fast::ResourceFactoryBinaryTextureV0 nativeFactory;
    auto resource = nativeFactory.ReadResource(file, initData);
    if (resource == nullptr) {
        return nullptr;
    }

    auto texture = std::static_pointer_cast<Fast::Texture>(resource);
    if (GetNativeWidth(initData->Path, *texture) == 0) {
        return resource;
    }
    const size_t pixelCount = static_cast<size_t>(texture->Width) * texture->Height;
    if (!HasCompletePayload(*file, *texture, pixelCount)) {
        SPDLOG_WARN("Ignoring incomplete high-resolution spin texture: {}", initData->Path);
        return nullptr;
    }
    auto rgba = std::make_shared<std::vector<char>>(pixelCount * 4);
    for (size_t i = 0; i < pixelCount; ++i) {
        // I8 has intensity alpha, not opaque alpha. Losing this reveals the
        // underlying rectangular polygon even when RGB is correctly decoded.
        const char intensity = static_cast<char>(texture->ImageData[i]);
        for (size_t channel = 0; channel < 4; ++channel) {
            (*rgba)[4 * i + channel] = intensity;
        }
    }

    UseWholeImage(*texture);
    texture->ImageDataSize = static_cast<uint32_t>(rgba->size());
    texture->mImageBuffer = std::move(rgba);
    texture->ImageData = reinterpret_cast<uint8_t*>(texture->mImageBuffer->data());
    return resource;
}

std::shared_ptr<Ship::IResource>
SpinEffectTextureFactoryV1::ReadResource(std::shared_ptr<Ship::File> file,
                                         std::shared_ptr<Ship::ResourceInitData> initData) {
    Fast::ResourceFactoryBinaryTextureV1 nativeFactory;
    auto resource = nativeFactory.ReadResource(file, initData);
    if (resource == nullptr) {
        return nullptr;
    }

    auto texture = std::static_pointer_cast<Fast::Texture>(resource);
    const uint32_t nativeWidth = GetNativeWidth(initData->Path, *texture);
    // Normalize only the conventional raw RGBA representation of these I8
    // upscales. Preserve author-specified flags, scales and other formats.
    if (nativeWidth == 0 || texture->Flags != TEX_FLAG_LOAD_AS_RAW ||
        texture->HByteScale != 4.0f * texture->Width / nativeWidth ||
        texture->VPixelScale != static_cast<float>(texture->Height) / 32) {
        return resource;
    }
    const size_t bytes = static_cast<size_t>(texture->Width) * texture->Height * 4;
    if (!HasCompletePayload(*file, *texture, bytes)) {
        SPDLOG_WARN("Ignoring incomplete high-resolution spin texture: {}", initData->Path);
        return nullptr;
    }
    // Keep authored RGBA and its owning buffer byte-for-byte intact.
    UseWholeImage(*texture);
    return resource;
}

} // namespace SOH
