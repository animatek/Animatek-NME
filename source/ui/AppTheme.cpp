#include "AppTheme.h"

namespace
{
AppThemeId current = AppThemeId::SoftDarkGrey;

AppThemePalette makeSoftDarkGrey()
{
    return {
        juce::Colour(0xff323232), // backgroundMain
        juce::Colour(0xff323232), // backgroundPanel
        juce::Colour(0xff2d2d2d), // backgroundSecondary
        juce::Colour(0xff343941), // backgroundElevated
        juce::Colour(0xff25282E), // inputBackground
        juce::Colour(0xff25282E), // buttonBackground
        juce::Colour(0xff343941), // buttonHover
        juce::Colour(0xff444A53), // buttonActive
        juce::Colour(0xff555B64), // borderColor
        juce::Colour(0xff2a2d33), // gridLine
        juce::Colour(0xff444A53), // gridLineStrong
        juce::Colour(0xffd0d4d8), // textPrimary
        juce::Colour(0xffcccccc), // textSecondary
        juce::Colour(0xff888899), // textMuted
        juce::Colour(0xffffcc44), // accentActive
        juce::Colour(0xffffaa44), // accentWarning
        juce::Colour(0xff44cc44), // accentSuccess
    };
}

AppThemePalette makeDeepDarkGrey()
{
    return {
        juce::Colour(0xff1A1A1A), // backgroundMain
        juce::Colour(0xff202020), // backgroundPanel
        juce::Colour(0xff242424), // backgroundSecondary
        juce::Colour(0xff2A2A2A), // backgroundElevated
        juce::Colour(0xff2B2B2B), // inputBackground
        juce::Colour(0xff303030), // buttonBackground
        juce::Colour(0xff3A3A3A), // buttonHover
        juce::Colour(0xff444444), // buttonActive
        juce::Colour(0xff3A3A3A), // borderColor
        juce::Colour(0xff242424), // gridLine
        juce::Colour(0xff303030), // gridLineStrong
        juce::Colour(0xffDADADA), // textPrimary
        juce::Colour(0xffA8A8A8), // textSecondary
        juce::Colour(0xff777777), // textMuted
        juce::Colour(0xffffcc44), // accentActive
        juce::Colour(0xffffaa44), // accentWarning
        juce::Colour(0xff44cc44), // accentSuccess
    };
}

const AppThemePalette soft = makeSoftDarkGrey();
const AppThemePalette deep = makeDeepDarkGrey();
AppThemePalette active = makeSoftDarkGrey();
}

AppThemeId AppTheme::currentTheme()
{
    return current;
}

const AppThemePalette& AppTheme::palette()
{
    return active;
}

const AppThemePalette& AppTheme::palette(AppThemeId id)
{
    return id == AppThemeId::DeepDarkGrey ? deep : soft;
}

juce::String AppTheme::displayName(AppThemeId id)
{
    return id == AppThemeId::DeepDarkGrey ? "Deep Dark Grey" : "Soft Dark Grey";
}

AppThemeId AppTheme::themeFromInt(int value)
{
    return value == static_cast<int>(AppThemeId::DeepDarkGrey)
        ? AppThemeId::DeepDarkGrey
        : AppThemeId::SoftDarkGrey;
}

void AppTheme::setTheme(AppThemeId id)
{
    current = id;
    active = palette(id);
    applyLookAndFeel();
}

void AppTheme::setPalette(const AppThemePalette& p)
{
    active = p;
    applyLookAndFeel();
}

void AppTheme::applyLookAndFeel()
{
    const auto& t = palette();
    auto& lf = juce::LookAndFeel::getDefaultLookAndFeel();

    lf.setColour(juce::PopupMenu::backgroundColourId,            t.backgroundPanel);
    lf.setColour(juce::PopupMenu::textColourId,                  t.textSecondary);
    lf.setColour(juce::PopupMenu::headerTextColourId,            t.accentActive);
    lf.setColour(juce::PopupMenu::highlightedBackgroundColourId, t.buttonActive);
    lf.setColour(juce::PopupMenu::highlightedTextColourId,       t.accentActive);

    lf.setColour(juce::ComboBox::backgroundColourId,             t.inputBackground);
    lf.setColour(juce::ComboBox::outlineColourId,                t.borderColor);
    lf.setColour(juce::ComboBox::textColourId,                   t.textSecondary);
    lf.setColour(juce::ComboBox::arrowColourId,                  t.accentWarning);
    lf.setColour(juce::ComboBox::focusedOutlineColourId,         t.accentActive);

    lf.setColour(juce::TextButton::buttonColourId,               t.buttonBackground);
    lf.setColour(juce::TextButton::buttonOnColourId,             t.buttonActive);
    lf.setColour(juce::TextButton::textColourOffId,              t.textSecondary);
    lf.setColour(juce::TextButton::textColourOnId,               t.accentActive);

    lf.setColour(juce::ScrollBar::thumbColourId,                 t.borderColor);
    lf.setColour(juce::ScrollBar::trackColourId,                 t.backgroundMain);

    lf.setColour(juce::ToggleButton::textColourId,               t.textSecondary);
    lf.setColour(juce::ToggleButton::tickColourId,               t.accentWarning);
    lf.setColour(juce::ToggleButton::tickDisabledColourId,       t.textMuted);

    lf.setColour(juce::AlertWindow::backgroundColourId,          t.backgroundMain);
    lf.setColour(juce::AlertWindow::textColourId,                t.textSecondary);
    lf.setColour(juce::AlertWindow::outlineColourId,             t.buttonActive);
}
