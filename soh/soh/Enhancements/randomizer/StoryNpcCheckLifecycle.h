#pragma once

namespace Rando {

constexpr bool StoryNpcCheck_MalonReturned(bool isRandomizer, bool vanillaDecision, bool talonReturned,
                                           bool eggCheckCollected, bool songCheckCollected) {
    if (!isRandomizer) {
        return vanillaDecision;
    }
    return talonReturned && eggCheckCollected && !songCheckCollected;
}

constexpr bool StoryNpcCheck_SariaEligible(bool isRandomizer, bool vanillaDecision, bool songCheckCollected) {
    return isRandomizer ? !songCheckCollected : vanillaDecision;
}

} // namespace Rando
