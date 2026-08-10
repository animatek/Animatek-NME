# Animatek NME 0.15.1: deleting a module no longer takes the editor with it

A hotfix, and only that. If you are on macOS and 0.15.0, this one is worth taking straight away.

## What's fixed

### 💥 Deleting a highlighted module crashed the editor on macOS

Click a module so it takes the yellow border, press **Del** or pick **Delete Module** from the
right-click menu, and the editor quit on the spot. Deleting a module you had *not* highlighted
worked, which is the detail that gave the fault away. Reported by **Nocticore** (#61), with a
crash log that pointed straight at it.

The Inspector keeps a reference to whatever module is selected so it can show its name, its cost
and its assignments. Deleting one destroyed the module and then redrew the Inspector, which went
looking at a module that no longer existed. On Linux and Windows that memory happens to still be
readable, so nothing showed and the bug sat there unseen since the Inspector was written; macOS
does not extend that courtesy and the editor stopped there and then.

The selection is now let go of before anything is deleted, and both the Inspector and the canvas
check with the patch that a module is still there before touching it.

### 💥 The same crash, undoing an Add Module or a paste

Nobody had reported this one, because it takes the same combination: the modules a paste or an
Add Module just created are left selected, so **Ctrl+Z** destroyed them out from under the
Inspector by exactly the same route. Also fixed, and it was another hard crash on macOS.

## Nothing else changed

0.15.1 is 0.15.0 plus the fix above. Everything in
[0.15.0's notes](RELEASE_NOTES_0.15.0.md) still applies.

## Thanks

To **Nocticore**, again, for a report precise enough to name the trigger ("if you don't have the
module highlighted, deleting works") and for sending the macOS crash log with it. That is what
turned a bug that cannot be reproduced on Linux into a fifteen-minute diagnosis.
