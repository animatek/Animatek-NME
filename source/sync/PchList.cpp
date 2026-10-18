#include "PchList.h"

namespace PchList
{

juce::String write(const Entries& entries)
{
    juce::String text;
    int expected = -1;   // position the next plain name would land on
    for (int pos = 0; pos < positions; ++pos)
    {
        if (entries[static_cast<size_t>(pos)].isEmpty())
            continue;
        if (pos != expected)
            text << juce::String(pos + 1) << " \r\n";
        text << entries[static_cast<size_t>(pos)] << "\r\n";
        expected = pos + 1;
    }
    return text;
}

bool parse(const juce::String& text, Entries& entries, juce::String& error)
{
    entries.fill({});
    int pos = 0;
    for (auto line : juce::StringArray::fromLines(text))
    {
        line = line.trim();
        if (line.isEmpty())
            continue;

        if (line.endsWithIgnoreCase(".pch"))
        {
            if (pos >= positions)
            {
                error = "List file corrupt: more than 99 patches";
                return false;
            }
            entries[static_cast<size_t>(pos++)] = line;
            continue;
        }

        if (line.containsOnly("0123456789"))
        {
            const int number = line.getIntValue();
            if (number < 1 || number > positions)
            {
                error = "List file corrupt: position " + line;
                return false;
            }
            pos = number - 1;
            continue;
        }

        error = "List file corrupt: \"" + line + "\"";
        return false;
    }
    return true;
}

juce::Array<juce::File> findIn(const juce::File& folder)
{
    auto lists = folder.findChildFiles(juce::File::findFiles, false, "*.pchList;*.pchlist");
    lists.sort();
    return lists;
}

}  // namespace PchList
