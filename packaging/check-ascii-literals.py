#!/usr/bin/env python3
"""Fail the build on non-ASCII characters inside narrow C++ string literals.

juce::String(const char*) decodes its bytes with CharPointer_ASCII, i.e. as
Latin-1, not UTF-8. A "--" typed as an em dash therefore reaches the UI as the
mojibake "a<TM>" instead (issue #43), and trips the assertion in
juce_String.cpp. Keep every user-visible literal in plain ASCII; when a real
Unicode glyph is wanted, wrap it in juce::CharPointer_UTF8 as
PatchBrowserPanel.cpp does for its play triangle.

Comments may contain whatever they like, so only literals are checked.
"""

import re
import sys
from pathlib import Path

LITERAL = re.compile(r'"(?:[^"\\\n]|\\.)*"')
# A literal deliberately handed to CharPointer_UTF8 is fine; those are written
# as hex escapes anyway, so they never contain raw non-ASCII bytes.


def offenders(path):
    for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        code = line.split("//", 1)[0]
        for literal in LITERAL.findall(code):
            bad = sorted({c for c in literal if ord(c) > 127})
            if bad:
                yield lineno, literal.strip(), bad


def main(argv):
    root = Path(argv[1]) if len(argv) > 1 else Path(__file__).resolve().parent.parent
    found = 0
    for path in sorted(root.glob("source/**/*.[ch]pp")) + sorted(root.glob("source/**/*.h")):
        for lineno, literal, bad in offenders(path):
            found += 1
            chars = " ".join("U+%04X (%s)" % (ord(c), c) for c in bad)
            rel = path.relative_to(root)
            print("%s:%d: non-ASCII in string literal: %s" % (rel, lineno, chars))
            print("    %s" % literal[:120])
    if found:
        print()
        print("%d literal(s) would reach juce::String as Latin-1. Use ASCII, or"
              " juce::CharPointer_UTF8 for a deliberate glyph." % found)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
