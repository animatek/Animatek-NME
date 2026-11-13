#include "BridgeScript.h"

namespace BridgeScript
{

juce::String fingerprint(const void* data, size_t size)
{
    juce::uint64 h = 0xcbf29ce484222325ULL;
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < size; ++i)
    {
        if (bytes[i] == '\r')
            continue;
        h ^= bytes[i];
        h *= 0x100000001b3ULL;
    }
    return juce::String::toHexString(static_cast<juce::int64>(h)).paddedLeft('0', 16);
}

juce::File folder()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
#if JUCE_MAC
        .getChildFile("Application Support")
#endif
        .getChildFile("AnimatekNME").getChildFile("mcp-bridge");
}

bool install(const juce::File& file, const void* data, size_t size)
{
    juce::MemoryBlock existing;
    if (file.existsAsFile() && file.loadFileAsData(existing)
        && existing.getSize() == size && std::memcmp(existing.getData(), data, size) == 0)
        return true;
    if (!file.getParentDirectory().createDirectory())
        return false;
    return file.replaceWithData(data, size);
}

juce::String command(const juce::File& serverPy, bool windows)
{
    const auto dir = serverPy.getParentDirectory();
    const auto venv = windows ? dir.getChildFile(".venv").getChildFile("Scripts").getChildFile("python.exe")
                              : dir.getChildFile(".venv").getChildFile("bin").getChildFile("python");
    const juce::String python = venv.existsAsFile() ? venv.getFullPathName().quoted()
                                                    : juce::String(windows ? "python" : "python3");
    return python + " " + serverPy.getFullPathName().quoted();
}

juce::String checkClient(const juce::var& request, const juce::String& expected,
                         const juce::String& registerCommand, const juce::String& editorVersion)
{
    const auto bridge = request.getProperty("bridge", juce::var());
    if (!bridge.isObject() || expected.isEmpty())
        return {};
    const auto got = bridge.getProperty("fingerprint", juce::var()).toString();
    if (got == expected)
        return {};
    const auto path = bridge.getProperty("path", juce::var()).toString();
    return "This MCP bridge (" + (path.isNotEmpty() ? path : juce::String("server.py"))
         + ") is not the one Animatek NME " + editorVersion + " ships, so its tools may not match "
           "the editor. Register the copy the editor keeps up to date instead, and restart the MCP "
           "client: " + registerCommand
         + " (Editor Options > MCP Bridge shows this command; it needs the Python package "
           "'mcp>=1.2,<2').";
}

} // namespace BridgeScript
