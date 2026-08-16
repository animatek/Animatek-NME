#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** A dialog that puts itself on the desktop and deletes itself when dismissed.
    Nobody owns one, which is convenient right up to the moment the editor quits
    with one still open: the component and every child it holds are leaked, and
    JUCE prints an assertion for each of them on the way out.

    Deriving from this keeps a register of the dialogs currently on screen, so
    the editor can take them down as it closes, and routes the self-delete
    through the message queue. That second part matters as much as the first:
    dismissing happens inside a button's click callback, and the button carries
    on touching itself after that callback returns, so deleting the dialog from
    inside it pulls the ground out from under the caller. */
class SelfOwnedDialog : public juce::Component
{
public:
    SelfOwnedDialog() { openDialogs().add (this); }

    ~SelfOwnedDialog() override { openDialogs().removeAllInstancesOf (this); }

    /** Takes down every dialog still on screen. The editor calls this as it
        closes, so quitting with one open costs nothing. */
    static void closeAllOpen()
    {
        auto doomed = openDialogs();   // a copy: deleting one edits the register
        openDialogs().clear();
        for (auto* d : doomed)
            delete d;
    }

protected:
    /** Dismisses and destroys the dialog. Safe to call from a button callback. */
    void closeSelf()
    {
        setVisible (false);
        removeFromDesktop();

        // Deleted once the click that asked for it has finished unwinding. The
        // SafePointer covers the case where the editor closes in between and
        // takes the dialog down first.
        juce::Component::SafePointer<SelfOwnedDialog> self (this);
        juce::MessageManager::callAsync ([self]() mutable { delete self.getComponent(); });
    }

private:
    static juce::Array<SelfOwnedDialog*>& openDialogs()
    {
        static juce::Array<SelfOwnedDialog*> dialogs;
        return dialogs;
    }
};
