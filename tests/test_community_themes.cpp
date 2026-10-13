#include <doctest.h>
#include "ui/CommunityThemes.h"
#include "ui/ThemeFile.h"

// Issue #91: community themes from animatek/nme-themes.

namespace
{
const juce::String kSha = juce::String::repeatedString("a", 64); // not a real hash; only its shape is checked
}

TEST_CASE("the index keeps well-formed entries and drops the ones that could escape the folder")
{
    const auto text = juce::String(R"({ "format": 1, "themes": [
        { "name": "Good", "file": "good-one.json", "author": "Someone", "sha256": "SHA" },
        { "name": "Path", "file": "../settings.json", "sha256": "SHA" },
        { "name": "Slash", "file": "a/b.json", "sha256": "SHA" },
        { "name": "Upper", "file": "Upper.json", "sha256": "SHA" },
        { "name": "No hash", "file": "no-hash.json" },
        { "name": "", "file": "no-name.json", "sha256": "SHA" }
    ] })").replace("SHA", juce::String::repeatedString("Ab", 32));

    std::vector<CommunityThemes::Entry> list;
    juce::String error;
    REQUIRE(CommunityThemes::parseIndex(text, list, error));
    REQUIRE(list.size() == 1);
    CHECK(list[0].name == "Good");
    CHECK(list[0].author == "Someone");
    CHECK(list[0].sha256 == juce::String::repeatedString("ab", 32));   // lower-cased

    CHECK_FALSE(CommunityThemes::parseIndex("not json", list, error));
    CHECK_FALSE(CommunityThemes::parseIndex(R"({ "format": 2, "themes": [] })", list, error));
    CHECK(error.contains("newer"));
}

TEST_CASE("a theme is new until seen or installed, and new again when its file changes")
{
    const auto dir = juce::File::createTempFile("nme-community");
    REQUIRE(dir.createDirectory());

    const juce::String content = R"({ "format": 1, "name": "Mine" })";
    juce::MemoryBlock bytes(content.toRawUTF8(), content.getNumBytesAsUTF8());

    CommunityThemes::Entry e;
    e.name = "Mine";
    e.file = "mine.json";
    e.sha256 = CommunityThemes::sha256Of(bytes);
    CHECK(e.sha256.length() == 64);

    using S = CommunityThemes::State;
    CHECK(CommunityThemes::stateOf(e, dir) == S::NotInstalled);
    juce::StringArray seen;
    CHECK(CommunityThemes::isNew(e, S::NotInstalled, seen));
    seen.add(CommunityThemes::seenKey(e));
    CHECK_FALSE(CommunityThemes::isNew(e, S::NotInstalled, seen));

    REQUIRE(dir.getChildFile("mine.json").replaceWithData(bytes.getData(), bytes.getSize()));
    CHECK(CommunityThemes::stateOf(e, dir) == S::Installed);
    CHECK_FALSE(CommunityThemes::isNew(e, S::Installed, {}));

    // The repository's file changes: a new hash, so it is new again and the local one is stale.
    auto changed = e;
    changed.sha256 = kSha;
    CHECK(CommunityThemes::stateOf(changed, dir) == S::Changed);
    CHECK(CommunityThemes::isNew(changed, S::Changed, seen));

    dir.deleteRecursively();
}

TEST_CASE("NME's own themes are official files: built in while the repository matches, an update when it changes")
{
    const auto* nord = ThemeRegistry::findBuiltin("Nord");
    REQUIRE(nord != nullptr);
    const auto text = ThemeFile::officialJson(*nord);
    CHECK_FALSE(text.contains("\r"));
    CHECK(ThemeFile::slugOf("Gruvbox Dark") == "gruvbox-dark");
    CHECK(ThemeFile::slugOf("MothWig") == "mothwig");

    EditorTheme back;
    juce::String error;
    REQUIRE(ThemeFile::fromJson(text, *nord, back, error));
    CHECK(back.official);
    CHECK(ThemeFile::toJson(back) == ThemeFile::toJson(*nord));   // same colours exactly

    const auto sha = CommunityThemes::sha256Of(juce::MemoryBlock(text.toRawUTF8(), text.getNumBytesAsUTF8()));
    CommunityThemes::Entry e;
    e.name = "Nord";
    e.file = "nord.json";
    e.official = true;
    e.sha256 = sha;
    const auto empty = juce::File::createTempFile("nme-official");
    using S = CommunityThemes::State;
    CHECK(CommunityThemes::stateOf(e, empty, sha) == S::BuiltIn);
    CHECK_FALSE(CommunityThemes::isNew(e, S::BuiltIn, {}));
    e.sha256 = kSha;   // touched in the repository
    CHECK(CommunityThemes::stateOf(e, empty, sha) == S::Changed);
    CHECK(CommunityThemes::isNew(e, S::Changed, {}));
}

TEST_CASE("an official file replaces its built-in in place; anything else with a taken name is (user)")
{
    std::vector<EditorTheme> all;
    for (int i = 0; i < ThemeRegistry::builtinCount(); ++i)
        all.push_back(*ThemeRegistry::findBuiltin(ThemeRegistry::names()[i]));
    const int nordIndex = 6;
    REQUIRE(all[nordIndex].name == "Nord");

    EditorTheme official = all[nordIndex];
    official.official = true;
    official.app.backgroundMain = juce::Colour(0xff123456);
    EditorTheme copy = all[nordIndex];   // a user's file that only shares the name
    EditorTheme mine;
    mine.name = "Mine";
    mine.makeCanvas = all[1].makeCanvas;

    const auto before = all.size();
    ThemeRegistry::mergeUserThemes(all, { official, copy, mine });
    REQUIRE(all.size() == before + 2);
    CHECK(all[nordIndex].name == "Nord");
    CHECK(all[nordIndex].app.backgroundMain == juce::Colour(0xff123456));
    CHECK(all[before].name == "Nord (user)");
    CHECK(all[before + 1].name == "Mine");
}
