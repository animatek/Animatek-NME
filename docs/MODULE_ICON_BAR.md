# Module Icon Bar — design notes

Proposed feature, requested by Nocticore on 2026-07-23. Not yet implemented. This document
exists so the research done up front is not lost; the task itself is tracked as
[issue #17](https://github.com/animatek/Animatek-NME/issues/17) and in
[ROADMAP.md](ROADMAP.md).

## What is being asked for

The original editors (Clavia's own, and Nomad/nmedit) present the module palette as a
horizontal **bar of module icons** grouped by category, from which you drag a module
straight onto the patch area. Long-time users navigate that bar by muscle memory —
they recognise the pictogram, not the module name. ANME currently only offers a text
tree (`ModuleBrowserPanel`) and Quick Add (`Enter`), so those users lose the fastest
route they know.

Requirement agreed with Javier: the bar must be **optional and hideable**, so users who
prefer the current text browser or Quick Add are not forced into extra screen furniture.

## The artwork already exists

nmedit shipped a complete, hand-drawn icon set. Present locally (gitignored) at:

| Path | Content |
|------|---------|
| `nmedit/libs/nordmodular/data/module-descriptions/img/icons/16x16/module-id-N.png` | 109 small icons, 32-bit RGBA |
| `nmedit/libs/nordmodular/data/module-descriptions/img/icons/32x32/module-id-N.png` | 109 large icons, 32-bit RGBA |
| `nmedit/libs/nordmodular/data/module-descriptions/img/modules/module-id-N.png` | 109 full module renderings |
| `nmedit/nomad/artwork/xcf/module-icons/` | GIMP sources (`all.xcf`, per-category templates) |

The same three sets are also embedded in
`nomad-0-3_2/plugins/net.sf.nmedit.nordmodular/classes.jar` under
`data/module-descriptions/img/`, byte-identical in count.

`N` in `module-id-N.png` is the module's **`index` attribute in `modules.xml`** — the same
id ANME already uses as `ModuleDescriptor::index`. Verified: the 109 icon ids map exactly
onto the 110 modules in `data/modules.xml`, with the single missing one being index 0
(`Morph`), which is not a placeable module. **No artwork needs to be drawn.**

### Licensing

nmedit is GPL and ANME is GPL-3 (`LICENSE`), so reuse is compatible in principle.
Before shipping, confirm the icon set's specific licence header in the nmedit source
tree and add the attribution to the About dialog / `README.md`.

## What ANME already has

The plumbing is done — an icon bar is a second drag *source* onto an existing target:

- `ModuleBrowserPanel::ModuleItem::getDragSourceDescription()`
  (`source/ui/ModuleBrowserPanel.cpp:43`) already packages a drag as a `DynamicObject`
  with `type="module"`, `descriptorPtr` (int64), `typeId`, `name`.
- `PatchCanvas` is a `juce::DragAndDropTarget` and accepts exactly that payload
  (`source/ui/PatchCanvasComponent.cpp:6265`), including drop positioning and undo.
- `ModuleDescriptions` already exposes category and index for every module, and
  `ModuleTags.cpp` supplies search tags.

So the new component only has to render icons and emit the *same* drag description.

## Proposed implementation

**New component** `source/ui/ModuleIconBar.{h,cpp}`, a `juce::Component` living in
`MainLayout` above the patch canvas (and, for consistency, inside `SlotWindowContent`).

1. **Assets** — copy the 32x32 set into `data/images/module-icons/module-id-N.png` and
   add them to the `juce_add_binary_data(NmeData ...)` list in `CMakeLists.txt:196`.
   109 PNGs of ~1–2 KB each is roughly 150 KB of binary data; acceptable. Load lazily
   into a `std::unordered_map<int, juce::Image>` keyed by descriptor index, and cache.
   Take the 16x16 set too if a compact bar height is offered.
2. **Layout** — one row per the original: category groups separated by a thin divider,
   icons in `modules.xml` order within each group. The 11 categories are Morph, In/Out,
   Oscillator, Sequencer, Control, Mixer, Envelope, Audio, LFO, Filter, Logic. With 109
   icons the row will not fit most windows, so either scroll horizontally
   (`juce::Viewport`) or expose category tabs/segments — decide against the original's
   look before coding.
3. **Interaction** — hover tooltip with the module's full name and cycles cost;
   click-to-add at canvas centre (matching Quick Add) *and* drag-and-drop via
   `startDragging()` with the description above; right-click opens the same context menu
   as the tree (module help).
4. **Hideable** — a `View → Module Icon Bar` toggle plus a keyboard shortcut, persisted
   in the properties file next to the other View toggles, defaulting to **visible** (it
   is the feature users are asking for) but trivially dismissed. Hidden state must not
   cost layout or paint time.
5. **Theming** — icons are fixed-palette bitmaps drawn for a light grey background. On
   the dark themes they may need a subtle backing plate or slight brightening; check
   against Deep Dark and Nord Classic before shipping. Wireframe mode (`Ctrl+W`) should
   leave the bar alone.

## Open questions

- Original look (single scrolling row) vs. a more practical category-segmented bar.
- Whether to also use the icons in `ModuleBrowserPanel`'s tree rows and in Quick Add
  results — likely a cheap win once the image cache exists, and it makes the icons
  learnable for users who did not come from the original editor.
- Whether the 16x16 or 32x32 set (or a user-selectable size) is the default.
