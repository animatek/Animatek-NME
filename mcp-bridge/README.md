# Animatek NME MCP bridge

Lets an MCP client (e.g. Claude Code) create and load patches, add and arrange
modules, edit parameters, and manage cables in a
**live, running** Animatek NME editor — changes appear on the canvas
immediately, and upload to the real synth if one is connected.

## How it works

AnimatekNME itself listens on a small local control socket
(`127.0.0.1:51027` by default, gated by the `NME_MCP_BRIDGE` CMake option,
`ON` by default). This script is the actual MCP server an MCP client talks
to over stdio; it just forwards each tool call to that socket as one line
of JSON and returns the JSON response — see `source/mcp/McpBridgeServer.h`
for the embedded side.

## Setup

```bash
cd mcp-bridge
python3 -m venv .venv
.venv/bin/pip install -e .
```

Then register it with Claude Code:

```bash
claude mcp add animatek-nme -- /path/to/mcp-bridge/.venv/bin/python /path/to/mcp-bridge/server.py
```

AnimatekNME must be running (with the bridge enabled — the default) for the
tools to work; they return a clear error if it isn't.

## Tools

- `list_module_types(category?, include_connectors?)` — catalog of module types.
  Compact by default (typeId, name, category); filter by category or ask for
  connectors when the full dump is genuinely wanted.
- `describe_module_type(type_id?, type_name?, include_morph?)` — one type's
  connectors and parameters. The way to find the exact connector names
  `connect_cable` expects, which are terse and not guessable.
- `list_modules(slot?, section?, include_parameters?, include_morph?,
  include_connectors?, verbose_parameters?)` — modules and cables currently in
  a patch. Defaults omit the `morph:` parameter twins and per-parameter
  min/max, which together dominate the payload on a large patch.
- `list_patches(query?)` — loaded slots and disk-library patch names/paths.
- `create_patch(slot?, name?, activate?)` — start an empty patch in a slot.
- `open_patch(slot?, name?, path?, activate?)` — load a library `.pch` by
  unique name or exact path.
- `save_patch(path, slot?, overwrite?)` — write a slot's patch (and its `.var`
  sidecar) to a `.pch`. Absolute paths are used as-is, relative ones resolve
  inside the configured patches folder, a missing extension defaults to `.pch`.
- `store_to_bank(bank, position, slot?)` — upload a slot's patch and store it to
  a synth bank (1-9) position (1-99) once the upload is ACKed. Needs a connected
  synth with its patch list loaded.
- `add_module(section, grid_x?, grid_y?, auto_place?, type_id?, type_name?, name?, slot?)`
- `move_module(section, container_index, grid_x, grid_y, slot?)`
- `rename_module(section, container_index, name, slot?)` — undoable, like every
  other structural edit.
- `delete_module(section, container_index, slot?)`
- `replace_module(section, container_index, type_name?, type_id?, slot?)` — swap a
  module for another of its family in place, as one undo step. Keeps position,
  a user-given name, cables on connectors that exist on the new type, values of
  parameters with the same name and range, and the assignments on them; reports
  what was kept and dropped.
- `connect_cable(section, out_container_index, out_connector,
  in_container_index, in_connector, out_is_output?, in_is_output?, slot?)`
- `delete_cable(section, out_container_index, out_connector,
  in_container_index, in_connector, out_is_output?, in_is_output?, slot?)`
- `mutate_patch(operation?, probability?, range?, slot?)` — mutate or randomize
  a patch through the editor's own Mutator engine. One undo step, delivered via
  the throttled parameter queue; respects locks, module exclusions and Output
  modules. Prefer it to a run of `set_parameter` calls.
- `set_parameter(section, container_index, parameter_name?, parameter_id?,
  value?, delta?, slot?)`

Choosing the synth and reloading from it:

- `list_midi_ports()` — the MIDI inputs and outputs with id and name, and which
  ones the editor is on.
- `connect_midi(input, output)` and `disconnect_midi()` — what the MIDI settings
  dialog does. A port is named by id, exact name or a part only one port has;
  anything ambiguous is refused. Then poll `get_synth_status` until `synthName`
  is not null and check it: G1-Emu and a real G1 can both be on the machine.
- `fetch_patch(slot?)` — ask the synth for a slot's patch again, replacing the
  editor's copy. With `save_patch`, this compares what the synth holds with what
  was sent.

Reading the synth back (these send nothing):

- `get_synth_status()` — connection, synth name (null until it has answered) and OS version, the slot the synth
  has focused versus the editor's active tab, transfers in flight, and per slot:
  patch name, LOCAL, enabled, voice count, bank location.
- `get_events(after?, limit?, types?)` — what the synth and the connection did
  since a given event: synth errors, connection changes, values the synth
  reported itself (a front-panel knob, a morph dial, a MIDI CC), slot focus and
  enable changes, patches received or incomplete, voice counts. Resume from the
  returned `latestSeq`; `truncated` says the 512-event log wrapped in between.
- `read_lights(section?, container_index?, slot?)` — per-module LEDs (0-3) and
  meters, for the slot the synth has focused (the only one it streams).
- `list_bank(bank, include_empty?)` — the patch name in each position of a synth
  bank, or null when empty, from the list fetched on connect. `store_to_bank`
  overwrites without asking: look here first.

Assignments (undoable, through the same actions as the canvas):

- `list_assignments(slot?)` — knob, morph group and MIDI CC assignments.
- `assign_knob(knob, section?, container_index?, parameter_name?, parameter_id?,
  morph_group?, replace?, slot?)` and `unassign_knob(knob, slot?)` — `knob` is an
  index 0-22 or a panel name (`"Knob 7"`, `"Pedal"`, `"After touch"`,
  `"On/Off switch"`); a bare `"7"` is refused as ambiguous. The target is a module
  parameter or a morph group's dial. A knob already in use fails with
  `knob_in_use` unless `replace=true`, and one undo then gives it back.
- `assign_morph(section, container_index, group, range?, parameter_name?,
  parameter_id?, slot?)` and `unassign_morph(section, container_index, ...)` —
  group 0-3, signed range -127..127, at most 25 morph assignments per patch.
- `assign_midi_cc(cc, <target as for assign_knob>, replace?, slot?)` and
  `unassign_midi_cc(cc, slot?)` — CC 0-119.

Playing it:

- `set_morph_value(morph_group, value?, delta?, slot?)` — turn a morph dial, as
  the header bar does (not an undo step).
- `play_note(note, duration_ms?)` — sound a note on the synth's focused slot.

## Testing against a real G1

The read-back tools are there so an assistant can check its own work on the
hardware: `get_synth_status` before starting, then an edit, then
`get_events(after=...)` to see whether the synth answered with an error, and
`play_note` with `read_lights` to confirm the patch makes signal.

Work in a slot you do not mind losing, and back the banks up first. Edits to a
LOCAL slot are still sent to the synth while it is connected (plan item S4), so
LOCAL is not yet a safe sandbox.

`slot` is 0-3 (A-D), defaulting to whichever tab is currently active in the
editor. `section` is 0 (common) or 1 (poly) — most modules go in poly.

Grid coordinates are in **module-column units**, not pixels — each column is
a full module's width. `add_module` automatically stacks up to 8 modules
vertically in each column, with a one-row gap and module-height collision
checks, before continuing in the next adjacent column. Coordinates are used
only with `auto_place=false`. Explicit add/move positions are rejected if they
overlap or leave the 40-column by 128-row grid.

Structural edits and parameter changes use the editor's normal undo actions,
so canvas repaint, undo/redo, variation updates, and synth synchronization use
the same paths as manual editing.
