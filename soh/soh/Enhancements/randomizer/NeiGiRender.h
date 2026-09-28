#pragma once

#include "NeiGiEffectPolicy.h"
#include "z64.h"
extern "C" {

// Shared presentation primitives. The caller owns pose, origin and scale;
// these functions do not add GI rotation or optional pickup shimmer.
NeiGi::Basis NeiGi_CameraBasis(PlayState* play);
void NeiGi_DrawMesh(PlayState* play, const NeiGi::Mesh& mesh, NeiGi::Kind orb = NeiGi::Kind::Neutral);
}
