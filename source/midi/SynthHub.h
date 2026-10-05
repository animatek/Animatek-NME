#pragma once

#include "ConnectionManager.h"
#include <array>
#include <memory>

// Several synths at once, the way the original editor does it (notes/13 of
// G1originaleditor): a fixed number of ports, each its own connection with its own
// four slots, and every patch window belonging to one synth and one slot.
//
// The editor numbers slots GLOBALLY: slot = synth * 4 + local slot, so everything
// that is already indexed by slot (patches, windows, undo, snapshots) keeps working
// with a bigger range. A ConnectionManager only knows its own four local slots; this
// class is the one place that turns a global slot into (synth, local slot).
//
// Calls that carry a slot are routed by it. Anything else has to say which synth it
// means: synth(i), or active() for what follows the user's focus (the bank, the
// patch list, the keyboard). There is deliberately no slot-less isConnected() or
// sendRawSysEx(): with two synths those are the calls that would reach the wrong one.
constexpr int kSlotsPerSynth = 4;
constexpr int kMaxSynths = 2;   // 2, then 4 or more (G1-Emu allows many instances)
constexpr int kTotalSlots = kSlotsPerSynth * kMaxSynths;

namespace SynthSlot
{
    constexpr int synthOf (int slot)  { return slot / kSlotsPerSynth; }
    constexpr int localOf (int slot)  { return slot % kSlotsPerSynth; }
    constexpr int global (int synth, int local) { return synth * kSlotsPerSynth + local; }
    constexpr bool valid (int slot)   { return slot >= 0 && slot < kTotalSlots; }

    // How a slot is named to the user: "A" with one synth, "1A" .. "2D" with several
    // (the original editor says "Port 2, Slot C").
    inline juce::String label (int slot)
    {
        const auto letter = juce::String::charToString (static_cast<juce::juce_wchar> ('A' + localOf (slot)));
        return kMaxSynths == 1 ? letter : juce::String (synthOf (slot) + 1) + letter;
    }

    // The same, for settings keys: the first synth keeps the bare letter, so the layout
    // saved before there were several synths is still found.
    inline juce::String key (int slot)
    {
        const auto letter = juce::String::charToString (static_cast<juce::juce_wchar> ('A' + localOf (slot)));
        return synthOf (slot) == 0 ? letter : juce::String (synthOf (slot) + 1) + letter;
    }
}

class SynthHub
{
public:
    SynthHub()
    {
        for (auto& s : synths)
            s = std::make_unique<ConnectionManager>();
    }

    ConnectionManager& synth (int i)             { return *synths[static_cast<size_t> (i)]; }
    const ConnectionManager& synth (int i) const { return *synths[static_cast<size_t> (i)]; }

    // The synth the user is working on: the one the focused slot belongs to.
    int activeSynth() const { return active_; }
    void setActiveSynth (int i) { if (i >= 0 && i < kMaxSynths) active_ = i; }
    ConnectionManager& active()             { return synth (active_); }
    const ConnectionManager& active() const { return synth (active_); }

    // The connection a slot lives on, and the slot's number on it.
    ConnectionManager& forSlot (int slot)             { return synth (SynthSlot::synthOf (slot)); }
    const ConnectionManager& forSlot (int slot) const { return synth (SynthSlot::synthOf (slot)); }
    static int local (int slot) { return SynthSlot::localOf (slot); }

    // ── Routed by slot ─────────────────────────────────────────────────────
    bool isConnectedSlot (int slot) const         { return SynthSlot::valid (slot) && forSlot (slot).isConnected(); }
    bool isFetchingPatch (int slot) const         { return forSlot (slot).isFetchingPatch(); }
    bool isUploadingPatch (int slot) const        { return forSlot (slot).isUploadingPatch(); }
    bool isAckedQueueIdle (int slot) const        { return forSlot (slot).isAckedQueueIdle(); }
    int  getPatchId (int slot) const              { return forSlot (slot).getPatchId (local (slot)); }

    void requestPatch (int slot)                  { forSlot (slot).requestPatch (local (slot)); }
    // slot < 0: the focused slot of the active synth, as ConnectionManager does.
    void loadPatchFromBank (int section, int position, int slot)
    {
        if (slot < 0) active().loadPatchFromBank (section, position, -1);
        else          forSlot (slot).loadPatchFromBank (section, position, local (slot));
    }
    void uploadPatch (int slot, const Patch& p)   { forSlot (slot).uploadPatch (local (slot), p); }
    void selectSlot (int slot)                    { forSlot (slot).selectSlot (local (slot)); }

    void sendParameter (int slot, int section, int module, int param, int value)
    { forSlot (slot).sendParameter (local (slot), section, module, param, value); }
    void sendParameterFromUser (int slot, int section, int module, int param, int value)
    { forSlot (slot).sendParameterFromUser (local (slot), section, module, param, value); }
    void queueParameter (int slot, int section, int module, int param, int value)
    { forSlot (slot).queueParameter (local (slot), section, module, param, value); }
    void sendPatchTitle (int slot, const juce::String& t)  { forSlot (slot).sendPatchTitle (local (slot), t); }
    void sendModuleTitle (int slot, int section, int index, const juce::String& t)
    { forSlot (slot).sendModuleTitle (local (slot), section, index, t); }

    // The SysEx a caller built already carries its slot's LOCAL number; the slot says where it goes.
    void sendRawSysEx (int slot, const std::vector<uint8_t>& sysex)  { forSlot (slot).sendRawSysEx (sysex); }
    void sendAckedSysEx (int slot, const std::vector<uint8_t>& sysex, bool allowNewPatchInSlotReply = false)
    { forSlot (slot).sendAckedSysEx (sysex, allowNewPatchInSlotReply); }

    void setSlotEnabled (int slot, bool enabled)  { forSlot (slot).setSlotEnabled (local (slot), enabled); }
    bool isSlotEnabled (int slot) const           { return forSlot (slot).isSlotEnabled (local (slot)); }

    int  getSlotBankSection (int slot) const      { return forSlot (slot).getSlotBankSection (local (slot)); }
    int  getSlotBankPosition (int slot) const     { return forSlot (slot).getSlotBankPosition (local (slot)); }
    void setSlotBankLocation (int slot, int section, int position)
    { forSlot (slot).setSlotBankLocation (local (slot), section, position); }
    void clearSlotBankLocation (int slot)         { forSlot (slot).clearSlotBankLocation (local (slot)); }

    void setUploadCompleteCallback (int slot, ConnectionManager::UploadCompleteCallback cb)
    { forSlot (slot).setUploadCompleteCallback (std::move (cb)); }

    // The slot that has focus on the active synth, as a global slot.
    int getCurrentSlot() const { return SynthSlot::global (active_, active().getCurrentSlot()); }

private:
    std::array<std::unique_ptr<ConnectionManager>, kMaxSynths> synths;
    int active_ = 0;
};
