#include <cassert>

#include "soh/Enhancements/randomizer/StoryNpcCheckLifecycle.h"

int main() {
    using Rando::StoryNpcCheck_MalonReturned;
    using Rando::StoryNpcCheck_SariaEligible;

    // Outside randomizer, preserve the native actor decision exactly.
    assert(StoryNpcCheck_MalonReturned(false, true, false, false, true));
    assert(!StoryNpcCheck_MalonReturned(false, false, true, true, false));
    assert(StoryNpcCheck_SariaEligible(false, true, true));
    assert(!StoryNpcCheck_SariaEligible(false, false, false));

    // Randomizer location bookkeeping must never suppress the native reward actor.
    assert(StoryNpcCheck_MalonReturned(true, false, true, true, false));
    assert(StoryNpcCheck_MalonReturned(true, false, false, false, true));

    // Saria remains eligible until her shuffled song check is collected.
    assert(StoryNpcCheck_SariaEligible(true, false, false));
    assert(!StoryNpcCheck_SariaEligible(true, true, true));
}
