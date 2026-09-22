#pragma once

#include "ship/resource/ResourceFactoryBinary.h"

namespace SOH {

// Keep oversized I8 spin-effect replacements in the native UV domain.
// Geometry and display-list selection remain entirely asset controlled.
class SpinEffectTextureFactoryV0 final : public Ship::ResourceFactoryBinary {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};

class SpinEffectTextureFactoryV1 final : public Ship::ResourceFactoryBinary {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};

} // namespace SOH
