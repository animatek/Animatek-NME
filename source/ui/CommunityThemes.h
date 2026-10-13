#pragma once

#include <juce_core/juce_core.h>

#include <optional>
#include <vector>

// Community themes (#91): the themes in the public animatek/nme-themes repository.
// Its index.json (rebuilt there by CI on every merge) lists each theme with the
// sha256 of its file, which is how a theme is told new, changed or installed.
// Installed community themes are the repository's own bytes, under the same file
// name, in the user's themes folder.
namespace CommunityThemes
{
    constexpr const char* kRepoUrl  = "https://github.com/animatek/nme-themes";
    constexpr const char* kRawBase  = "https://raw.githubusercontent.com/animatek/nme-themes/main/";
    constexpr int kIndexFormat = 1;
    constexpr juce::int64 kMaxIndexBytes = 1024 * 1024;

    struct Entry
    {
        juce::String name, file, author, description, sha256;
        bool official = false;   // one of NME's own themes, kept in the repository too
    };

    // False (with `error` set) when `text` is not an index NME can read. Entries whose
    // file name is not a plain "lower-case-words.json" are dropped: the name ends up
    // in a path on the user's disk.
    bool parseIndex(const juce::String& text, std::vector<Entry>& out, juce::String& error);

    // BuiltIn: an official theme NME ships exactly as the repository has it.
    // Changed: the repository's file differs from the installed or built-in one.
    enum class State { NotInstalled, Installed, Changed, BuiltIn };
    // `folder` is the user's themes folder; `builtinSha` the hash of what NME ships
    // under that name (ThemeFile::officialJson), empty when it ships no such theme.
    State stateOf(const Entry& e, const juce::File& folder, const juce::String& builtinSha = {});

    // The key that remembers an entry was seen: a changed file is new again.
    inline juce::String seenKey(const Entry& e) { return e.file + ":" + e.sha256; }

    // New to the user: not seen yet, and not already the installed version.
    bool isNew(const Entry& e, State s, const juce::StringArray& seen);

    juce::String sha256Of(const juce::MemoryBlock& data);

    // Downloads `url` (blocking; call it off the message thread). Empty on any failure
    // or when the answer is bigger than `maxBytes`. On Linux this runs `curl`, since
    // the app is built without libcurl.
    std::optional<juce::MemoryBlock> fetch(const juce::String& url, juce::int64 maxBytes);
}
