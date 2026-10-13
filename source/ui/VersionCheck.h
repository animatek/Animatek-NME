#pragma once

#include <juce_core/juce_core.h>

// New version notice (#97). The builds are on Patreon, so NME only says a newer one
// is out and links there: it never downloads or replaces itself. A release is a
// "vX.Y.Z" tag of this public repository, read once a day from GitHub's tag list.
namespace VersionCheck
{
    constexpr const char* kTagsUrl    = "https://api.github.com/repos/animatek/Animatek-NME/tags?per_page=100";
    constexpr const char* kPatreonUrl = "https://www.patreon.com/collection/2038913";
    constexpr juce::int64 kMaxTagsBytes = 512 * 1024;

    // "0.21.0" from "v0.21.0" or "0.21.0"; empty for anything else ("v0.22.0-rc1",
    // "nightly"), so only plain release tags count.
    juce::String plainVersion(const juce::String& tag);

    // True when `candidate` is a later version than `running` (both plain "X.Y.Z").
    bool isNewer(const juce::String& candidate, const juce::String& running);

    // The highest release in GitHub's tag list (JSON), as "X.Y.Z"; empty when the
    // text is not such a list or has no release tag.
    juce::String latestRelease(const juce::String& tagsJson);
}
