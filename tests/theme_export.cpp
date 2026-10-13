// nme_theme_export <folder>: writes every built-in theme as the official file the
// animatek/nme-themes repository carries (ThemeFile::officialJson), one per theme,
// named by ThemeFile::slugOf. Run it after changing a built-in theme, copy the files
// into the repository's themes/ and push: its CI rebuilds index.json, and every NME
// that ships the old colours offers the new ones as an update.
#include "ui/ThemeFile.h"

#include <iostream>

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "usage: nme_theme_export <folder>\n";
        return 2;
    }
    juce::ScopedJuceInitialiser_GUI juce;
    const juce::File folder = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    if (!folder.createDirectory())
    {
        std::cerr << "cannot create " << folder.getFullPathName() << "\n";
        return 1;
    }
    for (int i = 0; i < ThemeRegistry::builtinCount(); ++i)
    {
        const auto* t = ThemeRegistry::findBuiltin(ThemeRegistry::names()[i]);
        const auto file = folder.getChildFile(ThemeFile::slugOf(t->name) + ".json");
        // Bytes as they are: the hash NME compares is of exactly this text.
        const auto text = ThemeFile::officialJson(*t);
        if (!file.replaceWithData(text.toRawUTF8(), text.getNumBytesAsUTF8()))
        {
            std::cerr << "cannot write " << file.getFullPathName() << "\n";
            return 1;
        }
        std::cout << file.getFileName() << "  " << t->name << "\n";
    }
    return 0;
}
