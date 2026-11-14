#include <doctest.h>
#include "midi/ComputerKeys.h"

// Issue #124: with Caps Lock on, the computer keyboard plays the floater.

TEST_CASE("the middle row is the white keys, the row above the black ones")
{
    using ComputerKeys::offsetForKey;
    CHECK(offsetForKey('A') == 0);    // C
    CHECK(offsetForKey('W') == 1);    // C#
    CHECK(offsetForKey('S') == 2);    // D
    CHECK(offsetForKey('D') == 4);    // E
    CHECK(offsetForKey('F') == 5);    // F
    CHECK(offsetForKey('J') == 11);   // B
    CHECK(offsetForKey('K') == 12);   // the next C
    CHECK(offsetForKey(';') == 16);   // E above it
    CHECK(offsetForKey(0xF1) == 16);  // ñ, the same key on a Spanish layout
}

TEST_CASE("letters match in either case")
{
    using ComputerKeys::offsetForKey;
    CHECK(offsetForKey('a') == offsetForKey('A'));
    CHECK(offsetForKey('p') == 15);
}

TEST_CASE("Z and X shift the octave; other keys play nothing")
{
    using ComputerKeys::offsetForKey;
    CHECK(offsetForKey('Z') == ComputerKeys::octaveDown);
    CHECK(offsetForKey('x') == ComputerKeys::octaveUp);
    CHECK(offsetForKey('Q') == -1);
    CHECK(offsetForKey('1') == -1);
    CHECK(offsetForKey(' ') == -1);
}
