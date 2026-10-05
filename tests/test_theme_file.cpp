#include <doctest.h>
#include "ui/ThemeFile.h"

// Issue #90: themes as files.

TEST_CASE("colour strings: #rrggbb and #aarrggbb, nothing else")
{
    juce::Colour c;
    CHECK(ThemeFile::colourFromString("#2c7fff", c));
    CHECK(c == juce::Colour(0xff2c7fff));
    CHECK(ThemeFile::colourFromString("  #802c7fff ", c));
    CHECK(c.getAlpha() == 0x80);
    CHECK(ThemeFile::colourFromString("2C7FFF", c));        // the # is optional

    CHECK_FALSE(ThemeFile::colourFromString("#2c7ff", c));  // 5 digits
    CHECK_FALSE(ThemeFile::colourFromString("#gg7fff", c));
    CHECK_FALSE(ThemeFile::colourFromString("", c));

    CHECK(ThemeFile::colourToString(juce::Colour(0xff2c7fff)) == "#2c7fff");
    CHECK(ThemeFile::colourToString(juce::Colour(0x802c7fff)) == "#802c7fff");
    CHECK(ThemeFile::colourToString(juce::Colour(0x00000000)) == "#00000000");   // transparent moduleBg
}

TEST_CASE("every built-in theme survives being written and read back")
{
    for (int i = 0; i < ThemeRegistry::builtinCount(); ++i)
    {
        const auto& t = ThemeRegistry::get(i);
        const auto json = ThemeFile::toJson(t);

        EditorTheme back;
        juce::String error;
        REQUIRE_MESSAGE(ThemeFile::fromJson(json, t, back, error), t.name << ": " << error);
        CHECK(back.name == t.name);
        // Same text again means every colour, the morph colours and the texture flag came back.
        CHECK_MESSAGE(ThemeFile::toJson(back) == json, t.name);
    }
}

TEST_CASE("a half-written theme file keeps the base's values for what it leaves out")
{
    const auto* dark = ThemeRegistry::findBuiltin("Dark");
    REQUIRE(dark != nullptr);

    EditorTheme t;
    juce::String error;
    const juce::String text = R"({
        "format": 1, "name": "Mine", "base": "Dark",
        "app":    { "backgroundMain": "#102030" },
        "canvas": { "knobArc": "#ff8800", "knobBase": "not a colour",
                    "morphColor": ["#010203"] }
    })";
    REQUIRE(ThemeFile::fromJson(text, *dark, t, error));
    CHECK(t.name == "Mine");
    CHECK(t.app.backgroundMain == juce::Colour(0xff102030));
    CHECK(t.app.textPrimary == dark->app.textPrimary);

    const auto saved = AppTheme::palette();
    AppTheme::setPalette(dark->app);
    const auto base = dark->makeCanvas();
    AppTheme::setPalette(saved);

    const auto cs = t.makeCanvas();
    CHECK(cs.knobArc == juce::Colour(0xffff8800));
    CHECK(cs.knobBase == base.knobBase);            // bad colour: skipped
    CHECK(cs.morphColor[0] == juce::Colour(0xff010203));
    CHECK(cs.morphColor[1] == base.morphColor[1]);  // short array: the rest kept
    CHECK(cs.gridBackground == base.gridBackground);
}

TEST_CASE("the file's base picks the built-in it falls back to")
{
    const auto* classic = ThemeRegistry::findBuiltin("Nord Classic");
    const auto* dark = ThemeRegistry::findBuiltin("Dark");
    REQUIRE(classic != nullptr);
    REQUIRE(dark != nullptr);

    EditorTheme t;
    juce::String error;
    REQUIRE(ThemeFile::fromJson(R"({"format":1,"name":"X","base":"Nord Classic"})", *dark, t, error));
    CHECK(t.app.textPrimary == classic->app.textPrimary);

    // An unknown base falls back to the one the caller gives.
    REQUIRE(ThemeFile::fromJson(R"({"format":1,"name":"Y","base":"Nope"})", *dark, t, error));
    CHECK(t.app.textPrimary == dark->app.textPrimary);
}

TEST_CASE("files that are not themes are refused")
{
    const auto* dark = ThemeRegistry::findBuiltin("Dark");
    EditorTheme t;
    juce::String error;
    CHECK_FALSE(ThemeFile::fromJson("not json", *dark, t, error));
    CHECK_FALSE(ThemeFile::fromJson("[1,2]", *dark, t, error));
    CHECK_FALSE(ThemeFile::fromJson(R"({"format":99,"name":"X"})", *dark, t, error));
    CHECK_FALSE(ThemeFile::fromJson(R"({"format":1})", *dark, t, error));   // no name
}

TEST_CASE("themes are found by name, the saved choice")
{
    CHECK(ThemeRegistry::indexOfName("Dark") == 1);
    CHECK(ThemeRegistry::indexOfName("Animatek Rack") == ThemeRegistry::builtinCount() - 1);
    CHECK(ThemeRegistry::indexOfName("No such theme") == -1);
}

TEST_CASE("the theme editor's entries edit a doc and the sequencer colours survive a file")
{
    auto doc = ThemeFile::makeDoc(ThemeRegistry::get(0));
    for (const auto& e : ThemeFile::entries())
        CHECK_NOTHROW(ThemeFile::colourRef(doc, e));

    doc.name = "Edited";
    doc.canvas.stepOn = juce::Colour(0xffff8800);
    doc.canvas.seqNote = juce::Colour(0xff00aaff);
    doc.canvas.morphColor[2] = juce::Colour(0xffabcdef);
    const auto theme = ThemeFile::makeTheme(doc);

    EditorTheme back;
    juce::String error;
    REQUIRE(ThemeFile::fromJson(ThemeFile::toJson(theme, ThemeRegistry::get(0).name), ThemeRegistry::get(0), back, error));
    const auto cs = back.makeCanvas();
    CHECK(cs.stepOn == juce::Colour(0xffff8800));
    CHECK(cs.seqNote == juce::Colour(0xff00aaff));
    CHECK(cs.morphColor[2] == juce::Colour(0xffabcdef));
    CHECK(cs.sliderGrip.isTransparent());   // left automatic
}
