#pragma once

#include "../model/Patch.h"
#include "../midi/ConnectionManager.h"

/**
 * PatchSynchronizer monitors a Patch object for changes and sends
 * corresponding SysEx messages to the synthesizer in real-time.
 *
 * This implements the "live editing" behavior where every cable add/delete,
 * module move, etc. is immediately reflected on the synth.
 *
 * NOTE: This does NOT save to permanent flash - use StorePatchMessage for that.
 */
class PatchSynchronizer
{
public:
    // slot is the slot this synchronizer's patch belongs to — sent edits are
    // always addressed to it, independent of which slot has hardware focus
    // (confirmed on real hardware that the G1 applies edits addressed to a
    // non-focused slot; this is what makes background-slot editing correct).
    PatchSynchronizer(Patch& patch, ConnectionManager& connMgr, int slot);
    ~PatchSynchronizer();

    void enable();
    void disable();
    bool isEnabled() const { return enabled_; }

    /** Suppress all SysEx sends (used during undo/redo to avoid double-sends) */
    void setSuppressed(bool s) { suppressed_ = s; }
    bool isSuppressed() const { return suppressed_; }

private:
    // Event handlers
    void onCableAdded(int section, Connector* output, Connector* input);
    void onCableRemoved(int section, Connector* output, Connector* input);
    void onModuleMoved(int section, Module* module, int oldX, int oldY);
    void onModuleAdded(int section, Module* module);
    void onModuleRemoved(int section, Module* module);

    // Helper: find module's container index
    int getModuleIndex(const ModuleContainer& container, const Module* m) const;

    // Helper: find connector's index within its module
    int getConnectorIndex(const Module& m, const Connector* c) const;

    // Helper: find module that owns a connector
    Module* findModuleForConnector(const ModuleContainer& container, Connector* conn) const;

    Patch& patch_;
    ConnectionManager& connMgr_;
    int slot_;
    bool enabled_ = false;
    bool suppressed_ = false;
};
