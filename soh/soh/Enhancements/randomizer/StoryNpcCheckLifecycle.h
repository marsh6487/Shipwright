#pragma once

namespace Rando {

constexpr bool StoryNpcCheck_MalonReturned(bool isRandomizer, bool vanillaDecision, bool, bool, bool) {
    if (!isRandomizer) {
        return vanillaDecision;
    }
    return true;
}

constexpr bool StoryNpcCheck_SariaEligible(bool isRandomizer, bool vanillaDecision, bool songCheckCollected) {
    return isRandomizer ? !songCheckCollected : vanillaDecision;
}

} // namespace Rando
