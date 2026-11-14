#pragma once

// Playing notes from the computer keyboard (issue #124), the way Bitwig does it
// with Caps Lock: the middle letter row is the white keys, the row above it the
// black ones, from C up to the E of the next octave. Z and X shift the octave.
//
// Pure: it maps a key code to a note offset and knows nothing of JUCE, so the
// tests can check the layout.
namespace ComputerKeys
{
    constexpr int octaveDown = -100;
    constexpr int octaveUp   = -101;

    // Semitones above the keyboard's C for a key, octaveDown/octaveUp for Z/X,
    // or -1 for any other key. Letters match in either case, since Caps Lock
    // turns them upper case. The key right of L is ';' on a US layout and 'ñ'
    // on a Spanish one.
    inline int offsetForKey(int keyCode)
    {
        if (keyCode >= 'a' && keyCode <= 'z')
            keyCode -= 'a' - 'A';

        switch (keyCode)
        {
            case 'A': return 0;
            case 'W': return 1;
            case 'S': return 2;
            case 'E': return 3;
            case 'D': return 4;
            case 'F': return 5;
            case 'T': return 6;
            case 'G': return 7;
            case 'Y': return 8;
            case 'H': return 9;
            case 'U': return 10;
            case 'J': return 11;
            case 'K': return 12;
            case 'O': return 13;
            case 'L': return 14;
            case 'P': return 15;
            case ';': case 0xF1: case 0xD1: return 16;   // ; ñ Ñ
            case 'Z': return octaveDown;
            case 'X': return octaveUp;
            default:  return -1;
        }
    }
}
