#include "VersionCheck.h"

namespace
{
// The three numbers of a plain version, or false.
bool numbersOf(const juce::String& v, int out[3])
{
    const auto parts = juce::StringArray::fromTokens(v, ".", "");
    if (parts.size() != 3)
        return false;
    for (int i = 0; i < 3; ++i)
    {
        if (parts[i].isEmpty() || parts[i].length() > 6 || !parts[i].containsOnly("0123456789"))
            return false;
        out[i] = parts[i].getIntValue();
    }
    return true;
}
}

juce::String VersionCheck::plainVersion(const juce::String& tag)
{
    const auto v = tag.trim().startsWithChar('v') ? tag.trim().substring(1) : tag.trim();
    int n[3];
    return numbersOf(v, n) ? v : juce::String();
}

bool VersionCheck::isNewer(const juce::String& candidate, const juce::String& running)
{
    int a[3], b[3];
    if (!numbersOf(candidate, a) || !numbersOf(running, b))
        return false;
    for (int i = 0; i < 3; ++i)
        if (a[i] != b[i])
            return a[i] > b[i];
    return false;
}

juce::String VersionCheck::latestRelease(const juce::String& tagsJson)
{
    juce::var json;
    if (juce::JSON::parse(tagsJson, json).failed())
        return {};
    const auto* tags = json.getArray();
    if (tags == nullptr)
        return {};
    juce::String best;
    for (const auto& t : *tags)
    {
        const auto v = plainVersion(t.getProperty("name", {}).toString());
        if (v.isNotEmpty() && (best.isEmpty() || isNewer(v, best)))
            best = v;
    }
    return best;
}
