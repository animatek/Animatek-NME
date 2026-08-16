#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "../model/Patch.h"
#include "../model/ModuleDescriptions.h"
#include "../model/ModulePlacement.h"
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
    // Same redraw for an edit that changes a value and nothing else. A parameter
    // is not a module: the DSP figures and the morph/knob assignment list are
    // exactly as they were, and rebuilding them on every click is part of why
    // buttons felt heavier than knobs (issue #37) — a knob pays that cost once
    // per drag, a button paid it on every press.
    std::function<void()> repaintValues;
    std::function<void()> syncToSynth;    // full patch upload (may be null if not connected)
    // Live parameter edits write through into the active patch variation (may be null).
    // Deliberately NOT fired by bulk actions (recall/randomize) so undoing a
    // variation recall can't overwrite the stored variation itself.
    std::function<void(int section, int moduleId, int paramId, int value)> onParamEdited;
    // Fired by an undo that puts a deleted module back, so the canvas can put
    // the selection back with it: undoing a delete should hand you back what
    // you had, not an empty selection over the module you just recovered.
    // May be null.
    std::function<void(int section, int containerIndex)> onModuleRestored;
    int slot;  // which slot this context (and any edits sent below) belongs to
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
        // The new module keeps the spot it was dropped on; whatever was already
        // there moves down the column (issue #36). A column that cannot absorb
        // the push refuses the drop instead of burying what is at the bottom
        // (issue #54).
        pushed_.clear();
        auto& container = ctx_.patch.getContainer(section_);
        if (auto* desc = ctx_.descs.getModuleByIndex(typeId_))
        {
            if (container.canAdd(*desc))
            {
                if (!canMakeRoomForModule(container, section_, gridX_, gridY_, desc->height,
                                          {}, &ctx_.patch.getComments()))
                    return false;
                pushed_ = makeRoomForModule(container, section_, gridX_, gridY_, desc->height,
                                            {}, &ctx_.patch.getComments());
            }
        }

        auto* mod = ctx_.patch.createModule(section_, typeId_, gridX_, gridY_, name_, ctx_.descs);
        if (!mod)
        {
            restorePushedModules(ctx_.patch, pushed_);
            pushed_.clear();
            return false;
        }
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
        restorePushedModules(ctx_.patch, pushed_);
        ctx_.repaint();
        if (ctx_.syncToSynth) ctx_.syncToSynth();
        return true;
    }

    int getSizeInUnits() override { return 1; }

    // containerIndex assigned by createModule(), valid after a successful
    // perform() - lets callers that don't already hold the Module* (e.g. the
    // MCP bridge) report which index the new module landed on.
    int getContainerIndex() const { return containerIndex_; }

private:
    UndoContext& ctx_;
    int section_, typeId_, gridX_, gridY_;
    juce::String name_;
    int containerIndex_ = -1;
    std::vector<PushedModule> pushed_;
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
        if (ctx_.onModuleRestored) ctx_.onModuleRestored(section_, containerIndex_);
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
// RenameModuleAction — change a module's title (undoable). The name is pushed
// to the synth right away with SetModuleTitle: relying on the NameDump section
// of a full upload was not enough, because Store to Bank saves what the synth
// already holds, so renames never made it into the stored patch.
// ============================================================================
class RenameModuleAction : public juce::UndoableAction
{
public:
    RenameModuleAction(UndoContext& ctx, int section, int moduleIndex,
                       const juce::String& oldName, const juce::String& newName)
        : ctx_(ctx), section_(section), moduleIndex_(moduleIndex),
          oldName_(oldName), newName_(newName) {}

    bool perform() override { return apply(newName_); }
    bool undo()    override { return apply(oldName_); }

    int getSizeInUnits() override { return 1; }

private:
    bool apply(const juce::String& name)
    {
        auto* mod = ctx_.patch.getContainer(section_).getModuleByIndex(moduleIndex_);
        if (!mod) return false;
        mod->setTitle(name);
        ctx_.connMgr.sendModuleTitle(ctx_.slot, section_, moduleIndex_, name);
        ctx_.repaint();
        return true;
    }

    UndoContext& ctx_;
    int section_, moduleIndex_;
    juce::String oldName_, newName_;
};

// ============================================================================
// Comment actions - the editor's own text notes on the canvas. They never touch
// the synth (it has no such module), so none of these send anything: they only
// move text around the patch model and ask for a repaint.
// ============================================================================
class AddCommentAction : public juce::UndoableAction
{
public:
    AddCommentAction(UndoContext& ctx, int section, int gridX, int gridY,
                     int height, const juce::String& text, int width = 1)
        : ctx_(ctx), section_(section), gridX_(gridX), gridY_(gridY),
          width_(juce::jmax(1, width)), height_(height), text_(text) {}

    bool perform() override
    {
        // Same courtesy a module gets: whatever is under it moves down, in each
        // of the columns the note covers, and the same refusal when a column
        // has no room left (issue #54).
        pushed_.clear();
        for (int col = 0; col < width_; ++col)
            if (!canMakeRoomForModule(ctx_.patch.getContainer(section_), section_,
                                      gridX_ + col, gridY_, height_, {},
                                      &ctx_.patch.getComments(), commentId_))
                return false;
        for (int col = 0; col < width_; ++col)
        {
            auto made = makeRoomForModule(ctx_.patch.getContainer(section_), section_,
                                          gridX_ + col, gridY_, height_, {},
                                          &ctx_.patch.getComments(), commentId_);
            pushed_.insert(pushed_.end(), made.begin(), made.end());
        }

        PatchComment c;
        c.section = section_;
        c.x = gridX_;
        c.y = gridY_;
        c.width = width_;
        c.height = height_;
        c.text = text_;
        // Re-adding after an undo keeps the original id, so a redo of anything
        // that referred to this note still finds it.
        if (commentId_ > 0)
        {
            c.id = commentId_;
            ctx_.patch.restoreComment(c);
        }
        else
        {
            commentId_ = ctx_.patch.addComment(c);
        }
        ctx_.repaint();
        return true;
    }

    bool undo() override
    {
        ctx_.patch.removeCommentById(commentId_);
        restorePushedModules(ctx_.patch, pushed_);
        ctx_.repaint();
        return true;
    }

    int getSizeInUnits() override { return 1; }
    int getCommentId() const { return commentId_; }

private:
    UndoContext& ctx_;
    int section_, gridX_, gridY_, width_, height_;
    juce::String text_;
    int commentId_ = 0;
    std::vector<PushedModule> pushed_;
};

class DeleteCommentAction : public juce::UndoableAction
{
public:
    DeleteCommentAction(UndoContext& ctx, int commentId) : ctx_(ctx)
    {
        if (auto* c = ctx_.patch.getCommentById(commentId))
            stashed_ = *c;
    }

    bool perform() override
    {
        if (stashed_.id == 0) return false;
        ctx_.patch.removeCommentById(stashed_.id);
        ctx_.repaint();
        return true;
    }

    bool undo() override
    {
        if (stashed_.id == 0) return false;
        ctx_.patch.restoreComment(stashed_);
        ctx_.repaint();
        return true;
    }

    int getSizeInUnits() override { return 1; }

private:
    UndoContext& ctx_;
    PatchComment stashed_;
};

class MoveCommentAction : public juce::UndoableAction
{
public:
    MoveCommentAction(UndoContext& ctx, int commentId,
                      juce::Point<int> oldPos, juce::Point<int> newPos)
        : ctx_(ctx), commentId_(commentId), oldPos_(oldPos), newPos_(newPos) {}

    bool perform() override { return apply(newPos_); }
    bool undo()    override { return apply(oldPos_); }
    int getSizeInUnits() override { return 1; }

private:
    bool apply(juce::Point<int> pos)
    {
        auto* c = ctx_.patch.getCommentById(commentId_);
        if (!c) return false;
        c->x = pos.x;
        c->y = pos.y;
        ctx_.repaint();
        return true;
    }

    UndoContext& ctx_;
    int commentId_;
    juce::Point<int> oldPos_, newPos_;
};

class EditCommentTextAction : public juce::UndoableAction
{
public:
    EditCommentTextAction(UndoContext& ctx, int commentId,
                          const juce::String& oldText, const juce::String& newText)
        : ctx_(ctx), commentId_(commentId), oldText_(oldText), newText_(newText) {}

    bool perform() override { return apply(newText_); }
    bool undo()    override { return apply(oldText_); }
    int getSizeInUnits() override { return 1; }

private:
    bool apply(const juce::String& text)
    {
        auto* c = ctx_.patch.getCommentById(commentId_);
        if (!c) return false;
        c->text = text;
        ctx_.repaint();
        return true;
    }

    UndoContext& ctx_;
    int commentId_;
    juce::String oldText_, newText_;
};

// Dragging a corner can move the note's left edge as well as its size, so the
// whole rectangle travels together and the gesture undoes in one step. The
// rectangle is in grid units: (column, row, columns, rows).
class ResizeCommentAction : public juce::UndoableAction
{
public:
    ResizeCommentAction(UndoContext& ctx, int commentId,
                        juce::Rectangle<int> oldRect, juce::Rectangle<int> newRect)
        : ctx_(ctx), commentId_(commentId), oldRect_(oldRect), newRect_(newRect) {}

    bool perform() override
    {
        // A note that just grew makes room the way a dropped module does, and
        // is refused the same way when a column cannot absorb it (issue #54).
        for (int col = 0; col < newRect_.getWidth(); ++col)
            if (!canMakeRoomForModule(ctx_.patch.getContainer(sectionOf()), sectionOf(),
                                      newRect_.getX() + col, newRect_.getY(),
                                      newRect_.getHeight(), {},
                                      &ctx_.patch.getComments(), commentId_))
                return false;

        if (!apply(newRect_))
            return false;

        pushed_.clear();
        for (int col = 0; col < newRect_.getWidth(); ++col)
        {
            auto made = makeRoomForModule(ctx_.patch.getContainer(sectionOf()), sectionOf(),
                                          newRect_.getX() + col, newRect_.getY(),
                                          newRect_.getHeight(), {},
                                          &ctx_.patch.getComments(), commentId_);
            pushed_.insert(pushed_.end(), made.begin(), made.end());
        }
        ctx_.repaint();
        return true;
    }

    bool undo() override
    {
        if (!apply(oldRect_))
            return false;
        restorePushedModules(ctx_.patch, pushed_);
        pushed_.clear();
        ctx_.repaint();
        return true;
    }

    int getSizeInUnits() override { return 1; }

private:
    int sectionOf() const
    {
        auto* c = ctx_.patch.getCommentById(commentId_);
        return c != nullptr ? c->section : 1;
    }

    bool apply(juce::Rectangle<int> r)
    {
        auto* c = ctx_.patch.getCommentById(commentId_);
        if (!c) return false;
        c->x      = juce::jlimit(0, 39, r.getX());
        c->y      = juce::jmax(0, r.getY());
        c->width  = juce::jlimit(1, 40 - c->x, r.getWidth());
        c->height = juce::jlimit(1, 64, r.getHeight());
        ctx_.repaint();
        return true;
    }

    UndoContext& ctx_;
    int commentId_;
    juce::Rectangle<int> oldRect_, newRect_;
    std::vector<PushedModule> pushed_;
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
        ctx_.connMgr.sendParameter(ctx_.slot, section_, moduleId_, paramId_, newValue_);
        if (ctx_.onParamEdited) ctx_.onParamEdited(section_, moduleId_, paramId_, newValue_);
        redraw();
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
        ctx_.connMgr.sendParameter(ctx_.slot, section_, moduleId_, paramId_, oldValue_);
        if (ctx_.onParamEdited) ctx_.onParamEdited(section_, moduleId_, paramId_, oldValue_);
        redraw();
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
    // Values-only redraw where the context offers one, so a single click does
    // not drag a morph-list rebuild and a DSP recount behind it (issue #37).
    void redraw() const
    {
        if (ctx_.repaintValues) ctx_.repaintValues();
        else if (ctx_.repaint)  ctx_.repaint();
    }

    UndoContext& ctx_;
    int section_, moduleId_, paramId_;
    int oldValue_, newValue_;
};

// ============================================================================
// CustomParameterChangeAction — a UI-only parameter, never sent to the synth
// ============================================================================
//
// modules.xml marks a handful of parameters class="custom" role="ui": the
// frequency display units, the note sequencer's zoom and scroll position. They
// are stored in the patch, in its CustomDump sections, but they mean nothing to
// the synth and must never be sent to it — their index counts from zero
// alongside the real parameters, so a ParameterChange carrying one would land
// on a completely different control (the sequencer's first note, an
// oscillator's coarse tune).
class CustomParameterChangeAction : public juce::UndoableAction
{
public:
    CustomParameterChangeAction(UndoContext& ctx, int section, int moduleId,
                                int paramId, int oldValue, int newValue)
        : ctx_(ctx), section_(section), moduleId_(moduleId),
          paramId_(paramId), oldValue_(oldValue), newValue_(newValue) {}

    bool perform() override { return apply(newValue_); }
    bool undo() override    { return apply(oldValue_); }

    int getSizeInUnits() override { return 1; }

private:
    bool apply(int value)
    {
        auto& container = ctx_.patch.getContainer(section_);
        auto* mod = container.getModuleByIndex(moduleId_);
        if (!mod) return false;

        // Custom parameters share the index space with the ordinary ones, so
        // they have to be matched on class as well as index.
        for (auto& p : mod->getParameters())
        {
            const auto* pd = p.getDescriptor();
            if (pd == nullptr || pd->paramClass != "custom" || pd->index != paramId_)
                continue;
            p.setValue(value);
            if (ctx_.repaintValues) ctx_.repaintValues();
            else if (ctx_.repaint)  ctx_.repaint();
            return true;
        }
        return false;
    }

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
                int pid = ctx_.connMgr.getPatchId(ctx_.slot);
                int slot = ctx_.slot;
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
                    int pid = ctx_.connMgr.getPatchId(ctx_.slot);
                    int slot = ctx_.slot;
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
        int pid = ctx_.connMgr.getPatchId(ctx_.slot);
        int slot = ctx_.slot;

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
        int pid = ctx_.connMgr.getPatchId(ctx_.slot);
        int slot = ctx_.slot;
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
        ctx_.connMgr.sendPatchTitle(ctx_.slot, newName_);
        ctx_.repaint();
        return true;
    }

    bool undo() override
    {
        ctx_.patch.setName(oldName_);
        ctx_.connMgr.sendPatchTitle(ctx_.slot, oldName_);
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

        // 2. Hand the changes to the connection's coalesced, throttled queue.
        //    One shared queue across all snapshot applies means rapid Mutator
        //    auditions on large patches can never overlap and flood the synth.
        if (!ctx_.connMgr.isConnected()) return;

        for (auto& c : changes_)
            ctx_.connMgr.queueParameter(ctx_.slot, c.section, c.moduleId, c.paramId,
                                        forward ? c.newValue : c.oldValue);
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
    /** `targetSection` of -1 keeps every module in the area it was saved from,
        which is what importing a snippet file wants. Naming an area instead
        drops the whole block there, which is how pasting can cross between poly
        and common (issue #42). */
    InsertSnippetAction(UndoContext& ctx, SnipData snip, int offsetX, int offsetY,
                        int targetSection = -1)
        : ctx_(ctx), snip_(std::move(snip)), offsetX_(offsetX), offsetY_(offsetY),
          targetSection_(targetSection) {}

    bool perform() override
    {
        createdIndices_.clear();
        pushed_.clear();
        bool createdAny = false;

        // Modules this insert has already placed keep their spot, so the block
        // arrives with the shape it was copied with and pushes the patch around
        // it out of the way rather than shuffling within itself.
        std::vector<int> ownIndices[2];

        for (auto& entry : snip_.entries)
        {
            const int section = (targetSection_ >= 0) ? targetSection_ : entry.section;

            if (isSnippetExcludedModuleType(entry.typeIndex))
            {
                createdIndices_.push_back({ section, -1 });
                continue;
            }

            auto& container = ctx_.patch.getContainer(section);
            int tx = entry.gridPos.x + offsetX_;
            int ty = entry.gridPos.y + offsetY_;
            tx = juce::jlimit(0, 39, tx);
            ty = juce::jlimit(0, modulePlacementRows - 1, ty);

            auto* desc = ctx_.descs.getModuleByIndex(entry.typeIndex);
            if (!desc || !container.canAdd(*desc))
            {
                createdIndices_.push_back({ section, -1 });
                continue;
            }

            auto module = Module::createFromDescriptor(*desc);
            if (!module)
            {
                createdIndices_.push_back({ section, -1 });
                continue;
            }

            const int newIndex = nextContainerIndex(container);
            if (newIndex < 0)
            {
                createdIndices_.push_back({ section, -1 });
                continue;
            }

            // A column with no room left refuses the whole insert: burying
            // whatever sits at the bottom is how pasting a block used to end
            // up stacked on the patch (issue #54). undo() knows how to take
            // back what this insert has done so far, so it rolls the block
            // back rather than leaving half of it placed.
            if (!canMakeRoomForModule(container, section, tx, ty, desc->height,
                                      ownIndices[section == 1 ? 1 : 0],
                                      &ctx_.patch.getComments()))
            {
                undo();
                createdIndices_.clear();
                pushed_.clear();
                return false;
            }

            auto roomMade = makeRoomForModule(container, section, tx, ty, desc->height,
                                              ownIndices[section == 1 ? 1 : 0],
                                              &ctx_.patch.getComments());
            pushed_.insert(pushed_.end(), roomMade.begin(), roomMade.end());

            module->setContainerIndex(newIndex);
            module->setPosition({ tx, ty });
            module->setTitle(entry.name.isNotEmpty() ? entry.name : desc->name);

            auto& params = module->getParameters();
            for (size_t i = 0; i < entry.paramValues.size() && i < params.size(); ++i)
                params[i].setValue(entry.paramValues[i]);

            auto* mod = container.addModule(std::move(module));
            createdIndices_.push_back({ section, mod->getContainerIndex() });
            ownIndices[section == 1 ? 1 : 0].push_back(mod->getContainerIndex());
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
        restorePushedModules(ctx_.patch, pushed_);
        ctx_.repaint();
        return true;
    }

    int getSizeInUnits() override { return (int)snip_.entries.size(); }

    /** {section, containerIndex} per snippet entry, -1 where nothing was
        created. Valid after perform(); lets the caller select what it inserted. */
    const std::vector<std::pair<int, int>>& getCreatedIndices() const { return createdIndices_; }

private:
    UndoContext& ctx_;
    SnipData snip_;
    int offsetX_, offsetY_;
    int targetSection_;
    std::vector<std::pair<int, int>> createdIndices_;  // {section, containerIndex}
    std::vector<PushedModule> pushed_;

    // Indices are serialized in seven bits, so counting up from the highest one
    // in use overflows after enough add/delete cycles. Reuse the lowest free
    // index instead, which is what Patch::createModule does.
    static int nextContainerIndex(const ModuleContainer& container)
    {
        std::array<bool, 128> used {};
        for (auto& m : container.getModules())
        {
            const int index = m->getContainerIndex();
            if (index >= 1 && index <= 127)
                used[static_cast<size_t>(index)] = true;
        }
        for (int index = 1; index <= 127; ++index)
            if (!used[static_cast<size_t>(index)])
                return index;
        return -1;   // full: nothing can be inserted here
    }
};
