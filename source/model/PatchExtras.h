#pragma once

#include "Patch.h"
#include "PatchVariations.h"
#include <juce_core/juce_core.h>
#include <vector>

// Everything the editor knows about a patch that the G1 does not: the text
// notes on the canvas, the patch notes, the eight variations and the Mutator's
// exclusions. None of it fits on the wire, so a patch read back from the synth
// arrives without any of it, and until now that meant losing it.
//
// The answer is a small library of extras kept beside the settings, one file
// per patch, found again by two routes:
//
//   * By id. Patches saved by this editor carry an `[NME]` section holding the
//     id, so opening the file says outright which extras are its own. That is
//     also what makes extras survive being passed to another user together with
//     the .pch: the id travels inside it.
//
//   * By fingerprint, which is what a patch coming off the wire has to be
//     recognised by. The fingerprint covers the patch name, its modules (type,
//     area and grid position) and its cables, and deliberately NOT the
//     parameter values: turning a knob is the commonest edit there is and must
//     not cost a patch its notes.
//
// A patch grows modules over its life, so its fingerprint changes. An entry
// therefore remembers every fingerprint it has answered to, and the newest is
// added whenever the editor sees the patch it is bound to. That is what lets
// the link survive editing without the user ever being asked about it.

juce::String patchFingerprint(const Patch& patch);

struct PatchExtras
{
    juce::String id;
    // Newest first. Bounded: a patch edited for years should not grow a file.
    juce::StringArray fingerprints;
    juce::String name;              // last patch name seen, to make the files readable
    juce::int64 lastUsed = 0;

    std::vector<PatchComment> comments;
    juce::String notes;
    PatchVariations variations;

    bool isEmpty() const
    {
        return comments.empty() && notes.trim().isEmpty()
            && !variations.anyFilled() && variations.mutationExcluded.empty();
    }

    /** Puts `fingerprint` at the front, dropping any older copy of it. */
    void rememberFingerprint(const juce::String& fingerprint);

    static constexpr int maxFingerprints = 12;
};

class PatchExtrasStore
{
public:
    /** Where the entries live; reads whatever is already there. */
    void setFolder(const juce::File& folder);

    /** The entry a patch already carries the id of, or nullptr. */
    PatchExtras* findById(const juce::String& id);
    /** The entry that has worn this fingerprint, most recently used first.
        This is how a patch arriving from the synth finds its own extras. */
    PatchExtras* findByFingerprint(const juce::String& fingerprint);

    /** The entry for this id, creating an empty one if it is new. */
    PatchExtras& obtain(const juce::String& id);

    /** Writes one entry to disk, or deletes its file once it holds nothing:
        an empty entry is noise that would otherwise be matched forever. */
    void write(const PatchExtras& extras);

    static juce::String newId();

private:
    juce::File fileFor(const juce::String& id) const;

    juce::File folder;
    std::vector<PatchExtras> entries;
};

// Text form of one entry, shared by the store and by anything that wants to
// look at what was written.
juce::String extrasToText(const PatchExtras& extras);
bool extrasFromText(PatchExtras& extras, const juce::String& text);
