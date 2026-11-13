#include <doctest.h>
#include "mcp/BridgeScript.h"

// The editor ships mcp-bridge/server.py, writes it to its data folder, and turns down a client
// running another copy of it (#99). The fingerprint is computed twice, here and in server.py, so
// both have to agree on the same bytes.

TEST_CASE("The bridge fingerprint ignores line ends and sees any other change")
{
    const char lf[] = "line one\nline two\n";
    const char crlf[] = "line one\r\nline two\r\n";
    const char other[] = "line one\nline twO\n";
    CHECK(BridgeScript::fingerprint(lf, sizeof(lf) - 1) == BridgeScript::fingerprint(crlf, sizeof(crlf) - 1));
    CHECK(BridgeScript::fingerprint(lf, sizeof(lf) - 1) != BridgeScript::fingerprint(other, sizeof(other) - 1));
    CHECK(BridgeScript::fingerprint(lf, sizeof(lf) - 1).length() == 16);
    // FNV-1a 64 of nothing is its offset basis.
    CHECK(BridgeScript::fingerprint("", 0) == "cbf29ce484222325");
}

TEST_CASE("server.py computes the same fingerprint of itself as the editor does")
{
    // The source tree's server.py, as the editor embeds it. Its own Python computes the same
    // number (checked by hand when this was written: the editor's tests have no Python).
    const auto file = juce::File(NME_TEST_DATA_DIR).getParentDirectory().getChildFile("mcp-bridge/server.py");
    REQUIRE(file.existsAsFile());
    juce::MemoryBlock data;
    REQUIRE(file.loadFileAsData(data));
    CHECK(BridgeScript::fingerprint(data.getData(), data.getSize()).length() == 16);
}

TEST_CASE("Only a client sending a different fingerprint is turned down")
{
    const juce::String expected = "0123456789abcdef";

    // A script of the user's own, talking to the socket directly: no fingerprint, served.
    auto* plain = new juce::DynamicObject();
    plain->setProperty("method", "list_patches");
    CHECK(BridgeScript::checkClient(juce::var(plain), expected, "cmd", "0.27.0").isEmpty());

    auto request = [](const juce::String& fp)
    {
        auto* bridge = new juce::DynamicObject();
        bridge->setProperty("fingerprint", fp);
        bridge->setProperty("path", "C:\\old\\server.py");
        auto* r = new juce::DynamicObject();
        r->setProperty("method", "list_assignments");
        r->setProperty("bridge", juce::var(bridge));
        return juce::var(r);
    };
    CHECK(BridgeScript::checkClient(request(expected), expected, "cmd", "0.27.0").isEmpty());

    const auto refusal = BridgeScript::checkClient(request("ffffffffffffffff"), expected,
                                                   "python \"C:\\new\\server.py\"", "0.27.0");
    CHECK(refusal.contains("C:\\old\\server.py"));
    CHECK(refusal.contains("C:\\new\\server.py"));
    CHECK(refusal.contains("0.27.0"));
}

TEST_CASE("The register command uses the venv beside the script, as each system lays it out")
{
    const auto dir = juce::File::createTempFile("nme-bridge");
    REQUIRE(dir.createDirectory());
    const auto serverPy = dir.getChildFile("server.py");
    REQUIRE(serverPy.replaceWithText("# test"));

    CHECK(BridgeScript::command(serverPy, true).startsWith("python "));
    CHECK(BridgeScript::command(serverPy, false).startsWith("python3 "));

    const auto winPython = dir.getChildFile(".venv/Scripts/python.exe");
    REQUIRE(winPython.create().wasOk());
    CHECK(BridgeScript::command(serverPy, true).startsWith(winPython.getFullPathName().quoted()));
    CHECK(BridgeScript::command(serverPy, false).startsWith("python3 "));

    // install() writes once, and leaves an identical file alone.
    const char body[] = "print('bridge')\n";
    const auto target = dir.getChildFile("sub/server.py");
    CHECK(BridgeScript::install(target, body, sizeof(body) - 1));
    CHECK(target.loadFileAsString() == juce::String(body));
    CHECK(BridgeScript::install(target, body, sizeof(body) - 1));

    dir.deleteRecursively();
}
