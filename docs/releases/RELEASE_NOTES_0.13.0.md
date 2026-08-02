# Animatek NME 0.13.0: four slots, one window

The biggest structural change since the editor was renamed. The four slots stop being
separate operating-system windows and become sub-windows inside the main window, tiled the
way the original Clavia editor and Nomad arranged patches, and the way a tiling window
manager arranges anything else.

## What's new

### 🪟 The four slots live inside the main window

The per-slot pop-out windows are gone. They got lost behind other applications, could not be
arranged, and meant every feature that touched the canvas had to be written twice: once for
the main window and once for a pop-out. There is one canvas now, wired once per slot.

Open slots **tile themselves**. One fills the work area, two split it down the middle, three
go in thirds, four go 2x2, and the layout re-flows as you open and close them. Nothing to
arrange unless you want to: drag or resize a sub-window and the windows stay where you put
them from then on, with **View > Slots > Tile Slots** to re-flow.

- **Ctrl+Shift+1..4** shows or hides a slot's sub-window (so does right-clicking its row in
  the slot bar).
- **Ctrl+1..4** switches to a slot, opening it if it was closed.
- **F11**, or a sub-window's maximise button, blows the focused slot up to the whole area and
  back again for a closer look.
- **Ctrl+Shift+** an arrow moves the focused slot to the neighbouring tile, so the patch you
  are working on goes where you want it without closing and reopening anything.
  **View > Slots > Rotate Slots** shifts them all round at once.

The sub-windows slide to their new places rather than jumping. Turn that off with **Animate
Slot Tiling** in Editor Options if you prefer it instant.

The inspector, header bar, browsers and status bar stay shared and follow whichever slot has
focus, and the focused sub-window is edged in the theme's text colour so you can always see
which one you are editing. Each canvas keeps its own selection while it sits in the
background, so the inspector picks up where that slot left off instead of going blank.
Dragging a module out of the browser now works into any sub-window, which never worked in the
pop-outs.

Which slots you had open, which one had focus and how they were arranged all come back when
you reopen the editor. Connecting to the Nord then lines the work area up with the slots the
synth actually has enabled, once; after that the windows are yours alone, and pressing slot
buttons on the front panel moves focus without ever closing one.

Telling the synth which slot you moved to is now coalesced, so walking focus across four
windows no longer sprays slot messages down the MIDI cable, and a front-panel slot press will
not steal focus in the middle of a cable drag.

### 📥 Load a synth patch into a slot you name

Double-clicking a patch in the Synth browser still loads it into the slot you are on.
Right-clicking now offers **Load to Slot A..D**, so with several sub-windows open you can pull
a patch into a particular one without leaving the slot you are working in first.

### 🔠 Bigger text across the application

Panels, browsers, the inspector, the header bar, the status bar and every dialog had their
text sizes chosen by hand, file by file, and had drifted small next to the rest of the
desktop. They now share one scale, which lifts them together and keeps their relative weights
intact. Module bodies on the canvas are deliberately untouched: their text sits inside a fixed
grid that comes from the hardware.

## Fixed

- **The macro captions were painted in their own macro colour**, which made green M2
  unreadable on the light Nord Classic chrome. M1 to M4 above the morph dials, and the
  Macro 1..4 headers in the Inspector, now use the theme's text colour: light on the dark
  themes, dark on Nord Classic. The dials and the colour stripes still carry the colour, which
  is what identifies a macro in the first place.

- **"Press Enter to add modules" could pile up on itself** on an empty canvas. The hint was
  centred on whatever rectangle was being repainted rather than on the canvas, so a partial
  repaint left a copy behind at its own position.

- **Removed the "Recycle Windows" editor option**, which never did anything: it was saved,
  loaded and drawn as a checkbox, and nothing ever read it.

## Notes

Still beta. Back up any patch you care about before using this with your Nord Modular.
