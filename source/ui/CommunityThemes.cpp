#include "CommunityThemes.h"

#include <juce_cryptography/juce_cryptography.h>

namespace
{
bool plainFileName(const juce::String& f)
{
    if (!f.endsWith(".json") || f.length() <= 5 || f.length() > 100)
        return false;
    const auto stem = f.dropLastCharacters(5);
    return stem.containsOnly("abcdefghijklmnopqrstuvwxyz0123456789-")
        && !stem.startsWithChar('-') && !stem.endsWithChar('-');
}
}

bool CommunityThemes::parseIndex(const juce::String& text, std::vector<Entry>& out, juce::String& error)
{
    out.clear();
    juce::var json;
    if (juce::JSON::parse(text, json).failed() || json.getDynamicObject() == nullptr)
    {
        error = "the theme list is not JSON";
        return false;
    }
    if (static_cast<int>(json.getProperty("format", 0)) != kIndexFormat)
    {
        error = "the theme list is in a newer format: update NME";
        return false;
    }
    const auto* themes = json.getProperty("themes", {}).getArray();
    if (themes == nullptr)
    {
        error = "the theme list has no themes";
        return false;
    }
    for (const auto& t : *themes)
    {
        Entry e;
        e.name = t.getProperty("name", {}).toString().trim();
        e.file = t.getProperty("file", {}).toString();
        e.author = t.getProperty("author", {}).toString().trim();
        e.description = t.getProperty("description", {}).toString().trim();
        e.sha256 = t.getProperty("sha256", {}).toString().toLowerCase();
        e.official = static_cast<bool>(t.getProperty("official", false));
        if (e.name.isEmpty() || !plainFileName(e.file)
            || e.sha256.length() != 64 || !e.sha256.containsOnly("0123456789abcdef"))
            continue;
        out.push_back(std::move(e));
    }
    return true;
}

juce::String CommunityThemes::sha256Of(const juce::MemoryBlock& data)
{
    return juce::SHA256(data).toHexString().removeCharacters(" ").toLowerCase();
}

CommunityThemes::State CommunityThemes::stateOf(const Entry& e, const juce::File& folder,
                                                const juce::String& builtinSha)
{
    const auto f = folder.getChildFile(e.file);
    if (!f.existsAsFile())
    {
        if (e.official && builtinSha.isNotEmpty())
            return builtinSha == e.sha256 ? State::BuiltIn : State::Changed;
        return State::NotInstalled;
    }
    juce::MemoryBlock data;
    f.loadFileAsData(data);
    return sha256Of(data) == e.sha256 ? State::Installed : State::Changed;
}

bool CommunityThemes::isNew(const Entry& e, State s, const juce::StringArray& seen)
{
    return s != State::Installed && s != State::BuiltIn && !seen.contains(seenKey(e));
}

std::optional<juce::MemoryBlock> CommunityThemes::fetch(const juce::String& url, juce::int64 maxBytes)
{
    juce::MemoryBlock data;
   #if JUCE_LINUX || JUCE_BSD
    juce::ChildProcess curl;
    const juce::StringArray args { "curl", "-fsSL", "--max-time", "15",
                                   "--max-filesize", juce::String(maxBytes), url };
    if (!curl.start(args, juce::ChildProcess::wantStdOut))
        return std::nullopt;
    // The raw bytes, not readAllProcessOutput's String: the sha256 is of the file as it is.
    char buffer[8192];
    for (int n; (n = curl.readProcessOutput(buffer, sizeof(buffer))) > 0;)
    {
        data.append(buffer, static_cast<size_t>(n));
        if (static_cast<juce::int64>(data.getSize()) > maxBytes)
        {
            curl.kill();
            return std::nullopt;
        }
    }
    if (!curl.waitForProcessToFinish(5000) || curl.getExitCode() != 0)
        return std::nullopt;
   #else
    auto stream = juce::URL(url).createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(15000));
    if (stream == nullptr)
        return std::nullopt;
    stream->readIntoMemoryBlock(data, maxBytes + 1);
   #endif
    if (data.isEmpty() || static_cast<juce::int64>(data.getSize()) > maxBytes)
        return std::nullopt;
    return data;
}
