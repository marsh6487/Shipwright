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

    // In randomizer, Malon returns only after the egg check and remains until her song check is collected.
    assert(StoryNpcCheck_MalonReturned(true, false, true, true, false));
    assert(!StoryNpcCheck_MalonReturned(true, true, false, true, false));
    assert(!StoryNpcCheck_MalonReturned(true, true, true, false, false));
    assert(!StoryNpcCheck_MalonReturned(true, true, true, true, true));

    // Saria remains eligible until her shuffled song check is collected.
    assert(StoryNpcCheck_SariaEligible(true, false, false));
    assert(!StoryNpcCheck_SariaEligible(true, true, true));
}
