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
