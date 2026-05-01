#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "../model/Patch.h"
#include "../model/ModuleDescriptions.h"
#include "../model/SnipFileIO.h"
#include "../midi/ConnectionManager.h"
#include "../sync/PatchSynchronizer.h"
#include "../protocol/MorphAssignmentMessage.h"
#include "../protocol/MorphRangeChangeMessage.h"
#include "../protocol/KnobAssignmentMessage.h"
#include "../protocol/MidiCtrlAssignmentMessage.h"
#include <functional>

/**
 * Helper RAII guard to suppress PatchSynchronizer during undo/redo model mutations.
 * The action sends SysEx explicitly, so the synchronizer must not double-send.
 */
struct SyncSuppressor
{
    PatchSynchronizer* sync;
    SyncSuppressor(std::unique_ptr<PatchSynchronizer>& s) : sync(s.get()) { if (sync) sync->setSuppressed(true); }
    ~SyncSuppressor() { if (sync) sync->setSuppressed(false); }
};

// Forward: MainComponent provides these via a simple struct
struct UndoContext
{
    Patch& patch;
    ConnectionManager& connMgr;
    std::unique_ptr<PatchSynchronizer>& syncPtr;  // may be null
    const ModuleDescriptions& descs;
    std::function<void()> repaint;         // repaint canvas + inspector refresh
    std::function<void()> syncToSynth;    // full patch upload (may be null if not connected)
};

// ============================================================================
// AddModuleAction
// ============================================================================
class AddModuleAction : public juce::UndoableAction
{
public:
    AddModuleAction(UndoContext& ctx, int section, int typeId,
                    int gridX, int gridY, const juce::String& name)
        : ctx_(ctx), section_(section), typeId_(typeId),
          gridX_(gridX), gridY_(gridY), name_(name) {}

    bool perform() override
    {
        auto* mod = ctx_.patch.createModule(section_, typeId_, gridX_, gridY_, name_, ctx_.descs);
        if (!mod) return false;
        containerIndex_ = mod->getContainerIndex();
        ctx_.repaint();
        return true;
    }

    bool undo() override
    {
        auto& container = ctx_.patch.getContainer(section_);
        auto* mod = container.getModuleByIndex(containerIndex_);
        if (!mod) return false;

        {
            SyncSuppressor guard(ctx_.syncPtr);
            container.removeModule(mod);
        }
        ctx_.repaint();
        if (ctx_.syncToSynth) ctx_.syncToSynth();
        return true;
    }

    int getSizeInUnits() override { return 1; }

private:
    UndoContext& ctx_;
    int section_, typeId_, gridX_, gridY_;
    juce::String name_;
    int containerIndex_ = -1;
};

// ============================================================================
// DeleteModuleAction
// ============================================================================
struct StashedConnection
{
    int outModIndex, outConnIndex;
    bool outIsOutput;
    int inModIndex, inConnIndex;
    bool inIsOutput;
    SignalType color;
};

class DeleteModuleAction : public juce::UndoableAction
{
public:
    DeleteModuleAction(UndoContext& ctx, int section, Module* module)
        : ctx_(ctx), section_(section)
    {
        containerIndex_ = module->getContainerIndex();
        typeId_ = module->getDescriptor()->index;
        name_ = module->getTitle();
        position_ = module->getPosition();

        // Save parameter values
        for (auto& p : module->getParameters())
            paramValues_.push_back(p.getValue());

        // Save morph assignments for this module
        for (auto& ma : ctx_.patch.morphAssignments)
            if (ma.section == section_ && ma.module == containerIndex_)
                stashedMorphs_.push_back(ma);

        // Save knob assignments
        for (int k = 0; k < 23; ++k)
        {
            auto& ka = ctx_.patch.knobAssignments[static_cast<size_t>(k)];
            if (ka.assigned && ka.section == section_ && ka.module == containerIndex_)
                stashedKnobs_.push_back({ k, ka });
        }

        // Save MIDI CC assignments
        for (auto& ca : ctx_.patch.ctrlAssignments)
            if (ca.section == section_ && ca.module == containerIndex_)
                stashedCtrls_.push_back(ca);

        // Save cables involving this module
        auto& container = ctx_.patch.getContainer(section_);
        for (auto& conn : container.getConnections())
        {
            // Find which modules own these connectors
            for (auto& modPtr : container.getModules())
            {
                for (auto& c : modPtr->getConnectors())
                {
                    if (&c == conn.output || &c == conn.input)
                    {
                        // Check if this cable involves our module
                        bool involvesOurs = false;
                        for (auto& mc : module->getConnectors())
                            if (&mc == conn.output || &mc == conn.input)
                            { involvesOurs = true; break; }

                        if (involvesOurs)
                        {
                            // Find full info for both ends
                            StashedConnection sc;
                            // We need to identify both endpoints by module index + connector index
                            for (auto& m2 : container.getModules())
                            {
                                for (auto& c2 : m2->getConnectors())
                                {
                                    if (&c2 == conn.output)
                                    {
                                        sc.outModIndex = m2->getContainerIndex();
                                        sc.outConnIndex = c2.getDescriptor()->index;
                                        sc.outIsOutput = c2.getDescriptor()->isOutput;
                                        sc.color = c2.getDescriptor()->signalType;
                                    }
                                    if (&c2 == conn.input)
                                    {
                                        sc.inModIndex = m2->getContainerIndex();
                                        sc.inConnIndex = c2.getDescriptor()->index;
                                        sc.inIsOutput = c2.getDescriptor()->isOutput;
                                    }
                                }
                            }
                            stashedCables_.push_back(sc);
                        }
                    }
                }
            }
        }
        // Deduplicate cables
        std::sort(stashedCables_.begin(), stashedCables_.end(),
            [](const StashedConnection& a, const StashedConnection& b) {
                return std::tie(a.outModIndex, a.outConnIndex, a.inModIndex, a.inConnIndex) <
                       std::tie(b.outModIndex, b.outConnIndex, b.inModIndex, b.inConnIndex);
            });
        stashedCables_.erase(std::unique(stashedCables_.begin(), stashedCables_.end(),
            [](const StashedConnection& a, const StashedConnection& b) {
                return a.outModIndex == b.outModIndex && a.outConnIndex == b.outConnIndex
                    && a.inModIndex == b.inModIndex && a.inConnIndex == b.inConnIndex;
            }), stashedCables_.end());
    }

    bool perform() override
    {
        auto& container = ctx_.patch.getContainer(section_);
        auto* mod = container.getModuleByIndex(containerIndex_);
        if (!mod) return false;

        // Remove assignments from patch model
        auto& morphs = ctx_.patch.morphAssignments;
        morphs.erase(std::remove_if(morphs.begin(), morphs.end(),
            [this](const MorphAssignment& ma) {
                return ma.section == section_ && ma.module == containerIndex_;
            }), morphs.end());

        for (int k = 0; k < 23; ++k)
        {
            auto& ka = ctx_.patch.knobAssignments[static_cast<size_t>(k)];
            if (ka.assigned && ka.section == section_ && ka.module == containerIndex_)
                ka.assigned = false;
        }

        auto& ctrls = ctx_.patch.ctrlAssignments;
        ctrls.erase(std::remove_if(ctrls.begin(), ctrls.end(),
            [this](const CtrlAssignment& ca) {
                return ca.section == section_ && ca.module == containerIndex_;
            }), ctrls.end());

        // removeModule fires the sync callback which sends DeleteModule to synth
        container.removeModule(mod);
        ctx_.repaint();
        return true;
    }

    bool undo() override
    {
        // Re-create module from descriptor
        auto* desc = ctx_.descs.getModuleByIndex(typeId_);
        if (!desc) return false;

        auto module = Module::createFromDescriptor(*desc);
        module->setContainerIndex(containerIndex_);
        module->setPosition(position_);
        module->setTitle(name_);

        // Restore parameter values
        auto& params = module->getParameters();
        for (size_t i = 0; i < params.size() && i < paramValues_.size(); ++i)
            params[i].setValue(paramValues_[i]);

        auto& container = ctx_.patch.getContainer(section_);

        // All model changes suppressed — a full patch upload follows via syncToSynth.
        // Incremental NewModuleMessage is unreliable when multiple modules are
        // restored at once (grouped transaction) or the synth state is unknown.
        {
        SyncSuppressor guard(ctx_.syncPtr);
        container.addModule(std::move(module));

        // Restore cables (model only)
        for (auto& sc : stashedCables_)
            {
                auto* outMod = container.getModuleByIndex(sc.outModIndex);
                auto* inMod = container.getModuleByIndex(sc.inModIndex);
                if (!outMod || !inMod) continue;

                Connector* outConn = nullptr;
                for (auto& c : outMod->getConnectors())
                    if (c.getDescriptor()->index == sc.outConnIndex && c.getDescriptor()->isOutput == sc.outIsOutput)
                    { outConn = &c; break; }

                Connector* inConn = nullptr;
                for (auto& c : inMod->getConnectors())
                    if (c.getDescriptor()->index == sc.inConnIndex && c.getDescriptor()->isOutput == sc.inIsOutput)
                    { inConn = &c; break; }

                if (outConn && inConn)
                    container.addConnection(outConn, inConn);
            }
        }

        // Restore morph assignments
        for (auto& ma : stashedMorphs_)
            ctx_.patch.morphAssignments.push_back(ma);

        // Restore knob assignments
        for (auto& [k, ka] : stashedKnobs_)
            ctx_.patch.knobAssignments[static_cast<size_t>(k)] = ka;

        // Restore ctrl assignments
        for (auto& ca : stashedCtrls_)
            ctx_.patch.ctrlAssignments.push_back(ca);

        ctx_.repaint();
        if (ctx_.syncToSynth) ctx_.syncToSynth();
        return true;
    }

    int getSizeInUnits() override { return 1; }

private:
    UndoContext& ctx_;
    int section_, containerIndex_, typeId_;
    juce::String name_;
    juce::Point<int> position_;
    std::vector<int> paramValues_;
    std::vector<StashedConnection> stashedCables_;
    std::vector<MorphAssignment> stashedMorphs_;
    std::vector<std::pair<int, KnobAssignment>> stashedKnobs_;
    std::vector<CtrlAssignment> stashedCtrls_;
};

// ============================================================================
// MoveModuleAction
// ============================================================================
class MoveModuleAction : public juce::UndoableAction
{
public:
    MoveModuleAction(UndoContext& ctx, int section, int moduleIndex,
                     juce::Point<int> oldPos, juce::Point<int> newPos)
        : ctx_(ctx), section_(section), moduleIndex_(moduleIndex),
          oldPos_(oldPos), newPos_(newPos) {}

    bool perform() override
    {
        auto& container = ctx_.patch.getContainer(section_);
        auto* mod = container.getModuleByIndex(moduleIndex_);
        if (!mod) return false;
        // setPosition fires the sync callback
        mod->setPosition(newPos_);
        ctx_.repaint();
        return true;
    }

    bool undo() override
    {
        auto& container = ctx_.patch.getContainer(section_);
        auto* mod = container.getModuleByIndex(moduleIndex_);
        if (!mod) return false;
        mod->setPosition(oldPos_);
        ctx_.repaint();
        return true;
    }

    int getSizeInUnits() override { return 1; }

private:
    UndoContext& ctx_;
    int section_, moduleIndex_;
    juce::Point<int> oldPos_, newPos_;
};

// ============================================================================
// AddCableAction
// ============================================================================
class AddCableAction : public juce::UndoableAction
{
public:
    AddCableAction(UndoContext& ctx, int section,
                   int outModIndex, int outConnIndex, bool outIsOutput,
                   int inModIndex, int inConnIndex, bool inIsOutput,
                   bool alreadyDone = false)
        : ctx_(ctx), section_(section),
          outModIndex_(outModIndex), outConnIndex_(outConnIndex), outIsOutput_(outIsOutput),
          inModIndex_(inModIndex), inConnIndex_(inConnIndex), inIsOutput_(inIsOutput),
          alreadyDone_(alreadyDone) {}

    bool perform() override
    {
        if (alreadyDone_) { alreadyDone_ = false; return true; }
        auto& container = ctx_.patch.getContainer(section_);
        auto [outConn, inConn] = findConnectors(container);
        if (!outConn || !inConn) return false;
        container.addConnection(outConn, inConn);
        ctx_.repaint();
        return true;
    }

    bool undo() override
    {
        auto& container = ctx_.patch.getContainer(section_);
        auto [outConn, inConn] = findConnectors(container);
        if (!outConn || !inConn) return false;
        container.removeConnection(outConn, inConn);
        ctx_.repaint();
        return true;
    }

    int getSizeInUnits() override { return 1; }

private:
    std::pair<Connector*, Connector*> findConnectors(ModuleContainer& container)
    {
        auto* outMod = container.getModuleByIndex(outModIndex_);
        auto* inMod = container.getModuleByIndex(inModIndex_);
        if (!outMod || !inMod) return { nullptr, nullptr };

        Connector* outConn = nullptr;
        for (auto& c : outMod->getConnectors())
            if (c.getDescriptor()->index == outConnIndex_ && c.getDescriptor()->isOutput == outIsOutput_)
            { outConn = &c; break; }

        Connector* inConn = nullptr;
        for (auto& c : inMod->getConnectors())
            if (c.getDescriptor()->index == inConnIndex_ && c.getDescriptor()->isOutput == inIsOutput_)
            { inConn = &c; break; }

        return { outConn, inConn };
    }

    UndoContext& ctx_;
    int section_;
    int outModIndex_, outConnIndex_; bool outIsOutput_;
    int inModIndex_, inConnIndex_; bool inIsOutput_;
    bool alreadyDone_ = false;
};

// ============================================================================
// DeleteCableAction — inverse of AddCableAction
// ============================================================================
class DeleteCableAction : public juce::UndoableAction
{
public:
    DeleteCableAction(UndoContext& ctx, int section,
                      int outModIndex, int outConnIndex, bool outIsOutput,
                      int inModIndex, int inConnIndex, bool inIsOutput,
                      bool alreadyDone = false)
        : ctx_(ctx), section_(section),
          outModIndex_(outModIndex), outConnIndex_(outConnIndex), outIsOutput_(outIsOutput),
          inModIndex_(inModIndex), inConnIndex_(inConnIndex), inIsOutput_(inIsOutput),
          alreadyDone_(alreadyDone) {}

    bool perform() override
    {
        if (alreadyDone_) { alreadyDone_ = false; return true; }
        auto& container = ctx_.patch.getContainer(section_);
        auto [outConn, inConn] = findConnectors(container);
        if (!outConn || !inConn) return false;
        container.removeConnection(outConn, inConn);
        ctx_.repaint();
        return true;
    }

    bool undo() override
    {
        auto& container = ctx_.patch.getContainer(section_);
        auto [outConn, inConn] = findConnectors(container);
        if (!outConn || !inConn) return false;
        container.addConnection(outConn, inConn);
        ctx_.repaint();
        return true;
    }

    int getSizeInUnits() override { return 1; }

private:
    std::pair<Connector*, Connector*> findConnectors(ModuleContainer& container)
    {
        auto* outMod = container.getModuleByIndex(outModIndex_);
        auto* inMod = container.getModuleByIndex(inModIndex_);
        if (!outMod || !inMod) return { nullptr, nullptr };

        Connector* outConn = nullptr;
        for (auto& c : outMod->getConnectors())
            if (c.getDescriptor()->index == outConnIndex_ && c.getDescriptor()->isOutput == outIsOutput_)
            { outConn = &c; break; }

        Connector* inConn = nullptr;
        for (auto& c : inMod->getConnectors())
            if (c.getDescriptor()->index == inConnIndex_ && c.getDescriptor()->isOutput == inIsOutput_)
            { inConn = &c; break; }

        return { outConn, inConn };
    }

    UndoContext& ctx_;
    int section_;
    int outModIndex_, outConnIndex_; bool outIsOutput_;
    int inModIndex_, inConnIndex_; bool inIsOutput_;
    bool alreadyDone_ = false;
};

// ============================================================================
// ParameterChangeAction — with coalescing for rapid knob turns
// ============================================================================
class ParameterChangeAction : public juce::UndoableAction
{
public:
    ParameterChangeAction(UndoContext& ctx, int section, int moduleId,
                          int paramId, int oldValue, int newValue)
        : ctx_(ctx), section_(section), moduleId_(moduleId),
          paramId_(paramId), oldValue_(oldValue), newValue_(newValue) {}

    bool perform() override
    {
        auto& container = ctx_.patch.getContainer(section_);
        auto* mod = container.getModuleByIndex(moduleId_);
        if (!mod) return false;
        auto* param = mod->getParameter(paramId_);
        if (!param) return false;
        param->setValue(newValue_);
        ctx_.connMgr.sendParameter(section_, moduleId_, paramId_, newValue_);
        ctx_.repaint();
        return true;
    }

    bool undo() override
    {
        auto& container = ctx_.patch.getContainer(section_);
        auto* mod = container.getModuleByIndex(moduleId_);
        if (!mod) return false;
        auto* param = mod->getParameter(paramId_);
        if (!param) return false;
        param->setValue(oldValue_);
        ctx_.connMgr.sendParameter(section_, moduleId_, paramId_, oldValue_);
        ctx_.repaint();
        return true;
    }

    /** Coalesce rapid changes to the same parameter into one undo step */
    UndoableAction* createCoalescedAction(UndoableAction* next) override
    {
        if (auto* other = dynamic_cast<ParameterChangeAction*>(next))
        {
            if (other->section_ == section_ && other->moduleId_ == moduleId_
                && other->paramId_ == paramId_)
            {
                return new ParameterChangeAction(ctx_, section_, moduleId_,
                                                  paramId_, oldValue_, other->newValue_);
            }
        }
        return nullptr;
    }

    int getSizeInUnits() override { return 1; }

private:
    UndoContext& ctx_;
    int section_, moduleId_, paramId_;
    int oldValue_, newValue_;
};

// ============================================================================
// MorphAssignAction
// ============================================================================
class MorphAssignAction : public juce::UndoableAction
{
public:
    MorphAssignAction(UndoContext& ctx, int section, int moduleId, int paramId,
                      int newGroup, int oldGroup, int oldRange)
        : ctx_(ctx), section_(section), moduleId_(moduleId), paramId_(paramId),
          newGroup_(newGroup), oldGroup_(oldGroup), oldRange_(oldRange) {}

    bool perform() override
    {
        applyMorph(newGroup_, 0);
        return true;
    }

    bool undo() override
    {
        applyMorph(oldGroup_, oldRange_);
        return true;
    }

    int getSizeInUnits() override { return 1; }

private:
    void applyMorph(int group, int range)
    {
        auto& assignments = ctx_.patch.morphAssignments;
        assignments.erase(
            std::remove_if(assignments.begin(), assignments.end(),
                [this](const MorphAssignment& ma) {
                    return ma.section == section_ && ma.module == moduleId_ && ma.param == paramId_;
                }),
            assignments.end());

        if (group >= 0)
        {
            MorphAssignment ma;
            ma.section = section_; ma.module = moduleId_;
            ma.param = paramId_; ma.morph = group; ma.range = range;
            assignments.push_back(ma);

            if (ctx_.connMgr.isConnected())
            {
                int pid = ctx_.connMgr.getCurrentPatchId();
                int slot = ctx_.connMgr.getCurrentSlot();
                MorphAssignmentMessage msg(pid, section_, moduleId_, paramId_, group);
                ctx_.connMgr.sendRawSysEx(msg.toSysEx(slot));
                MorphRangeChangeMessage rangeMsg(pid, section_, moduleId_, paramId_,
                                                  std::abs(range), range < 0 ? 1 : 0);
                ctx_.connMgr.sendRawSysEx(rangeMsg.toSysEx(slot));
            }
        }
        ctx_.repaint();
    }

    UndoContext& ctx_;
    int section_, moduleId_, paramId_;
    int newGroup_, oldGroup_, oldRange_;
};

// ============================================================================
// MorphRangeChangeAction — with coalescing
// ============================================================================
class MorphRangeChangeAction : public juce::UndoableAction
{
public:
    MorphRangeChangeAction(UndoContext& ctx, int section, int moduleId, int paramId,
                           int oldSignedRange, int newSignedRange)
        : ctx_(ctx), section_(section), moduleId_(moduleId), paramId_(paramId),
          oldRange_(oldSignedRange), newRange_(newSignedRange) {}

    bool perform() override { return applyRange(newRange_); }
    bool undo() override { return applyRange(oldRange_); }

    UndoableAction* createCoalescedAction(UndoableAction* next) override
    {
        if (auto* other = dynamic_cast<MorphRangeChangeAction*>(next))
        {
            if (other->section_ == section_ && other->moduleId_ == moduleId_
                && other->paramId_ == paramId_)
            {
                return new MorphRangeChangeAction(ctx_, section_, moduleId_,
                                                   paramId_, oldRange_, other->newRange_);
            }
        }
        return nullptr;
    }

    int getSizeInUnits() override { return 1; }

private:
    bool applyRange(int signedRange)
    {
        for (auto& ma : ctx_.patch.morphAssignments)
        {
            if (ma.section == section_ && ma.module == moduleId_ && ma.param == paramId_)
            {
                ma.range = signedRange;
                if (ctx_.connMgr.isConnected())
                {
                    int pid = ctx_.connMgr.getCurrentPatchId();
                    int slot = ctx_.connMgr.getCurrentSlot();
                    int span = std::abs(signedRange);
                    int dir = signedRange < 0 ? 1 : 0;
                    MorphRangeChangeMessage msg(pid, section_, moduleId_, paramId_, span, dir);
                    ctx_.connMgr.sendRawSysEx(msg.toSysEx(slot));
                }
                break;
            }
        }
        ctx_.repaint();
        return true;
    }

    UndoContext& ctx_;
    int section_, moduleId_, paramId_;
    int oldRange_, newRange_;
};

// ============================================================================
// KnobAssignAction
// ============================================================================
class KnobAssignAction : public juce::UndoableAction
{
public:
    // newKnob=-1 means deassign; prevKnob=-1 means was unassigned
    KnobAssignAction(UndoContext& ctx, int section, int moduleId, int paramId,
                     int newKnob, int prevKnob)
        : ctx_(ctx), section_(section), moduleId_(moduleId), paramId_(paramId),
          newKnob_(newKnob), prevKnob_(prevKnob) {}

    bool perform() override { return applyKnob(newKnob_, prevKnob_); }
    bool undo() override { return applyKnob(prevKnob_, newKnob_); }
    int getSizeInUnits() override { return 1; }

private:
    bool applyKnob(int targetKnob, int fromKnob)
    {
        int pid = ctx_.connMgr.getCurrentPatchId();
        int slot = ctx_.connMgr.getCurrentSlot();

        if (fromKnob >= 0)
            ctx_.patch.knobAssignments[static_cast<size_t>(fromKnob)].assigned = false;

        if (targetKnob >= 0)
        {
            ctx_.patch.knobAssignments[static_cast<size_t>(targetKnob)] =
                { true, section_, moduleId_, paramId_ };

            if (ctx_.connMgr.isConnected())
            {
                if (fromKnob >= 0)
                    ctx_.connMgr.sendRawSysEx(
                        KnobAssignmentMessage::reassign(pid, fromKnob, targetKnob, section_, moduleId_, paramId_, slot));
                else
                    ctx_.connMgr.sendRawSysEx(
                        KnobAssignmentMessage::assign(pid, targetKnob, section_, moduleId_, paramId_, slot));
            }
        }
        else if (fromKnob >= 0 && ctx_.connMgr.isConnected())
        {
            ctx_.connMgr.sendRawSysEx(KnobAssignmentMessage::deassign(pid, fromKnob, slot));
        }
        ctx_.repaint();
        return true;
    }

    UndoContext& ctx_;
    int section_, moduleId_, paramId_;
    int newKnob_, prevKnob_;
};

// ============================================================================
// MidiCtrlAssignAction
// ============================================================================
class MidiCtrlAssignAction : public juce::UndoableAction
{
public:
    // newCC=-1 means deassign; prevCC=-1 means was unassigned
    MidiCtrlAssignAction(UndoContext& ctx, int section, int moduleId, int paramId,
                         int newCC, int prevCC)
        : ctx_(ctx), section_(section), moduleId_(moduleId), paramId_(paramId),
          newCC_(newCC), prevCC_(prevCC) {}

    bool perform() override { return applyCC(newCC_, prevCC_); }
    bool undo() override { return applyCC(prevCC_, newCC_); }
    int getSizeInUnits() override { return 1; }

private:
    bool applyCC(int targetCC, int fromCC)
    {
        int pid = ctx_.connMgr.getCurrentPatchId();
        int slot = ctx_.connMgr.getCurrentSlot();
        auto& ctrls = ctx_.patch.ctrlAssignments;

        // Remove old
        if (fromCC >= 0)
        {
            ctrls.erase(std::remove_if(ctrls.begin(), ctrls.end(),
                [this](const CtrlAssignment& ca) {
                    return ca.section == section_ && ca.module == moduleId_ && ca.param == paramId_;
                }), ctrls.end());
        }

        if (targetCC >= 0)
        {
            CtrlAssignment ca;
            ca.control = targetCC; ca.section = section_;
            ca.module = moduleId_; ca.param = paramId_;
            ctrls.push_back(ca);

            if (ctx_.connMgr.isConnected())
            {
                if (fromCC >= 0)
                    ctx_.connMgr.sendRawSysEx(
                        MidiCtrlAssignmentMessage::reassign(pid, fromCC, targetCC, section_, moduleId_, paramId_, slot));
                else
                    ctx_.connMgr.sendRawSysEx(
                        MidiCtrlAssignmentMessage::assign(pid, targetCC, section_, moduleId_, paramId_, slot));
            }
        }
        else if (fromCC >= 0 && ctx_.connMgr.isConnected())
        {
            ctx_.connMgr.sendRawSysEx(MidiCtrlAssignmentMessage::deassign(pid, fromCC, slot));
        }
        ctx_.repaint();
        return true;
    }

    UndoContext& ctx_;
    int section_, moduleId_, paramId_;
    int newCC_, prevCC_;
};

// ============================================================================
// RenamePatchAction
// ============================================================================
class RenamePatchAction : public juce::UndoableAction
{
public:
    RenamePatchAction(UndoContext& ctx, const juce::String& oldName, const juce::String& newName)
        : ctx_(ctx), oldName_(oldName), newName_(newName) {}

    bool perform() override
    {
        ctx_.patch.setName(newName_);
        ctx_.connMgr.sendPatchTitle(newName_);
        ctx_.repaint();
        return true;
    }

    bool undo() override
    {
        ctx_.patch.setName(oldName_);
        ctx_.connMgr.sendPatchTitle(oldName_);
        ctx_.repaint();
        return true;
    }

    int getSizeInUnits() override { return 1; }

private:
    UndoContext& ctx_;
    juce::String oldName_, newName_;
};

// ============================================================================
// RandomizeAction — batches many parameter changes, single synth upload
// ============================================================================
class RandomizeAction : public juce::UndoableAction
{
public:
    struct ParamChange {
        int section, moduleId, paramId, oldValue, newValue;
    };

    RandomizeAction(UndoContext& ctx, std::vector<ParamChange> changes)
        : ctx_(ctx), changes_(std::move(changes)) {}

    bool perform() override
    {
        applyValues(true);
        return true;
    }

    bool undo() override
    {
        applyValues(false);
        return true;
    }

    int getSizeInUnits() override { return static_cast<int>(changes_.size()); }

    /** Throttled async send of parameter changes to synth (4 per 20ms) */
    static void sendBatch(std::shared_ptr<std::vector<ParamChange>> params,
                          std::shared_ptr<size_t> pos,
                          ConnectionManager& cm)
    {
        for (int i = 0; i < 4 && *pos < params->size(); ++i, ++(*pos))
        {
            auto& c = (*params)[*pos];
            cm.sendParameter(c.section, c.moduleId, c.paramId, c.newValue);
        }
        if (*pos < params->size())
        {
            juce::Timer::callAfterDelay(20, [params, pos, &cm]() {
                sendBatch(params, pos, cm);
            });
        }
    }

private:
    void applyValues(bool forward)
    {
        // 1. Apply all values in memory (no SysEx yet)
        for (auto& c : changes_)
        {
            auto& container = ctx_.patch.getContainer(c.section);
            auto* mod = container.getModuleByIndex(c.moduleId);
            if (!mod) continue;
            auto* param = mod->getParameter(c.paramId);
            if (!param) continue;
            param->setValue(forward ? c.newValue : c.oldValue);
        }
        ctx_.repaint();

        // 2. Send parameter changes to synth with throttled batches
        //    using callAfterDelay chain (no self-deleting timer).
        if (!ctx_.connMgr.isConnected()) return;

        auto pending = std::make_shared<std::vector<ParamChange>>(changes_);
        if (!forward) {
            for (auto& c : *pending)
                c.newValue = c.oldValue;
        }

        auto idx = std::make_shared<size_t>(0);
        sendBatch(pending, idx, ctx_.connMgr);
    }

    UndoContext& ctx_;
    std::vector<ParamChange> changes_;
};

// ============================================================================
// InsertSnippetAction
// ============================================================================
class InsertSnippetAction : public juce::UndoableAction
{
public:
    InsertSnippetAction(UndoContext& ctx, SnipData snip, int offsetX, int offsetY)
        : ctx_(ctx), snip_(std::move(snip)), offsetX_(offsetX), offsetY_(offsetY) {}

    bool perform() override
    {
        createdIndices_.clear();
        bool createdAny = false;

        for (auto& entry : snip_.entries)
        {
            if (isSnippetExcludedModuleType(entry.typeIndex))
            {
                createdIndices_.push_back({ entry.section, -1 });
                continue;
            }

            auto& container = ctx_.patch.getContainer(entry.section);
            int tx = entry.gridPos.x + offsetX_;
            int ty = entry.gridPos.y + offsetY_;
            if (tx < 0) tx = 0;
            if (ty < 0) ty = 0;

            // Simple Y overlap avoidance
            int h = 1;
            if (auto* desc = ctx_.descs.getModuleByIndex(entry.typeIndex))
                h = desc->height;
            for (int attempt = 0; attempt < 128; ++attempt)
            {
                bool occupied = false;
                for (auto& m : container.getModules())
                {
                    int mh = m->getDescriptor() ? m->getDescriptor()->height : 1;
                    if (m->getPosition().x == tx &&
                        std::abs(m->getPosition().y - ty) < std::max(h, mh))
                    { occupied = true; break; }
                }
                if (!occupied) break;
                ++ty;
            }

            auto* desc = ctx_.descs.getModuleByIndex(entry.typeIndex);
            if (!desc || !container.canAdd(*desc))
            {
                createdIndices_.push_back({ entry.section, -1 });
                continue;
            }

            auto module = Module::createFromDescriptor(*desc);
            if (!module)
            {
                createdIndices_.push_back({ entry.section, -1 });
                continue;
            }

            module->setContainerIndex(nextContainerIndex(container));
            module->setPosition({ tx, ty });
            module->setTitle(entry.name.isNotEmpty() ? entry.name : desc->name);

            auto& params = module->getParameters();
            for (size_t i = 0; i < entry.paramValues.size() && i < params.size(); ++i)
                params[i].setValue(entry.paramValues[i]);

            auto* mod = container.addModule(std::move(module));
            createdIndices_.push_back({ entry.section, mod->getContainerIndex() });
            createdAny = true;
        }

        // Let the normal synchronizer send the same ordered edit stream as the
        // original Java editor: all modules first, then cables. Avoiding a full
        // patch upload here keeps snippet insertion from locking the synth.
        for (auto& cb : snip_.cables)
        {
            if (cb.srcIdx < 0 || cb.srcIdx >= (int)createdIndices_.size()) continue;
            if (cb.dstIdx < 0 || cb.dstIdx >= (int)createdIndices_.size()) continue;
            auto [srcSec, srcCI] = createdIndices_[static_cast<size_t>(cb.srcIdx)];
            auto [dstSec, dstCI] = createdIndices_[static_cast<size_t>(cb.dstIdx)];
            if (srcCI < 0 || dstCI < 0 || srcSec != dstSec) continue;

            auto& container = ctx_.patch.getContainer(srcSec);
            auto* src = container.getModuleByIndex(srcCI);
            auto* dst = container.getModuleByIndex(dstCI);
            if (!src || !dst) continue;
            auto* sc = src->getConnector(cb.srcConn, cb.srcIsOutput);
            auto* dc = dst->getConnector(cb.dstConn, cb.dstIsOutput);
            if (sc && dc) container.addConnection(sc, dc);
        }

        ctx_.repaint();
        return createdAny;
    }

    bool undo() override
    {
        for (auto it = createdIndices_.rbegin(); it != createdIndices_.rend(); ++it)
        {
            auto [sec, cidx] = *it;
            if (cidx < 0) continue;
            auto& container = ctx_.patch.getContainer(sec);
            auto* mod = container.getModuleByIndex(cidx);
            if (mod)
                container.removeModule(mod);
        }
        ctx_.repaint();
        return true;
    }

    int getSizeInUnits() override { return (int)snip_.entries.size(); }

private:
    UndoContext& ctx_;
    SnipData snip_;
    int offsetX_, offsetY_;
    std::vector<std::pair<int, int>> createdIndices_;  // {section, containerIndex}

    static int nextContainerIndex(const ModuleContainer& container)
    {
        int maxIndex = 0;
        for (auto& m : container.getModules())
            maxIndex = std::max(maxIndex, m->getContainerIndex());
        return maxIndex + 1;
    }
};
