#pragma once

#include <juce_core/juce_core.h>

// Search tags for module descriptors (synonyms and use-case words a player
// would type: "lp", "vca", "glide", "bitcrush"...). Kept as a hand-written
// table next to the descriptors so it can evolve without touching the
// third-party modules.xml. Returns a lowercase space-separated tag string,
// or empty for unknown modules.
juce::String getModuleTags(const juce::String& moduleName);
