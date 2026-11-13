#pragma once

#include <juce_core/juce_core.h>

// The MCP bridge (mcp-bridge/server.py) is what an MCP client runs; it talks to the editor's socket.
// It used to live only in the source repository, so people downloaded a copy once and kept running
// it long after it had been fixed (#99). The editor now carries the script and writes it to its own
// data folder on every start, so the copy there is always the one this editor ships; Editor Options
// gives the command that registers it. The script sends a fingerprint of itself with each request,
// and a request from a different copy is turned down with that command in the message.
namespace BridgeScript
{
    // FNV-1a, 64 bits, over the bytes with every '\r' left out (a checkout or an editor on Windows
    // may have turned the line ends into CRLF), as 16 hex digits. server.py computes the same.
    juce::String fingerprint(const void* data, size_t size);

    // <user data>/AnimatekNME/mcp-bridge: next to the editor's logs.
    juce::File folder();

    // Writes `data` to `file` unless it already holds exactly that. False when it could not.
    bool install(const juce::File& file, const void* data, size_t size);

    // The command that registers `serverPy` with a client: the venv's Python beside it if there is
    // one, else the system's, as each system names it.
    juce::String command(const juce::File& serverPy, bool windows);

    // Empty when the request may go ahead: it carries no fingerprint (a script of the user's own
    // that talks to the socket directly), or the one expected. Otherwise, what to tell the client.
    juce::String checkClient(const juce::var& request, const juce::String& expected,
                             const juce::String& registerCommand, const juce::String& editorVersion);
}
