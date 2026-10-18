# 9. The MCP Bridge (AI assistants)

Animatek NME can expose a small local control channel that lets an **MCP client**
(Claude Code, Claude Desktop, OpenCode and others) work the patch as if it were
you at the canvas: adding and moving modules, connecting and cutting cables,
setting parameters, mutating a patch, opening patches from your library, saving
them, and storing them to a synth bank.

Everything goes through the editor's normal undo system, so an assistant's edits
appear on the canvas immediately, upload to the synth if one is connected, and
are reviewable and undoable exactly like your own (`Ctrl+Z`).

## It is off by default

No port is opened unless you ask for it. Turn it on in **Editor Options
(`Ctrl+,`) → MCP Bridge**. The panel shows:

- the **enable toggle**,
- whether the bridge is **listening** (green) or **failed** (amber),
- the exact **stdio command** to register with your client, with a *Copy*
  button.

The bridge listens on `127.0.0.1` only, never on the network, and is off the
wire entirely when the toggle is off. It can also be compiled out completely
with `-DNME_MCP_BRIDGE=OFF`. It is **standalone-app only**: it is deliberately
not built into the plugin, where several instances would fight over the port.

## Setting up a client

The editor itself listens on the local socket (`127.0.0.1:51027` by default).
The MCP server your client talks to is `mcp-bridge/server.py`, shipped with the
source repository:

```bash
cd mcp-bridge
python3 -m venv .venv
.venv/bin/pip install -e .
```

Then register it. For Claude Code:

```bash
claude mcp add animatek-nme -- /path/to/mcp-bridge/.venv/bin/python /path/to/mcp-bridge/server.py
```

If the editor can find `mcp-bridge/` near its own executable it fills that exact
command into the Editor Options panel for you; otherwise use the paths of your
own copy. Animatek NME must be **running** with the bridge enabled; the tools
return a clear error if it isn't.

## What an assistant can do

| Tool | Purpose |
|------|---------|
| `list_module_types`, `describe_module_type` | Browse the 110+ module palette; get one type's exact connector and parameter names |
| `list_modules` | Read a patch's modules, connectors and cables |
| `list_patches` | Loaded slots plus your disk library |
| `create_patch`, `open_patch` | Start an empty patch, or load one by name/path |
| `add_module`, `move_module`, `rename_module`, `delete_module`, `replace_module` | Build and arrange; swap a module for another of its family keeping its cables |
| `connect_cable`, `delete_cable` | Wire it up |
| `set_parameter` | Set or nudge one parameter |
| `mutate_patch` | Run the editor's own Mutator as one undoable, throttled step |
| `save_patch` | Write a slot's patch (and its `.var`) to a `.pch` |
| `store_to_bank` | Upload a slot's patch and store it to a synth bank position |
| `save_bank`, `send_bank`, `get_bank_transfer`, `cancel_bank_transfer` | A whole bank to a folder and back (`.pch` files plus a `.pchList`), straight to and from the synth's memory; start one, then follow it |
| `get_synth_status`, `get_events`, `read_lights`, `list_bank` | Read the synth back: connection and slots, what happened since the last look, LEDs and meters, what a bank holds |
| `list_assignments`, `assign_knob`, `assign_morph`, `assign_midi_cc` (and their `unassign_` twins) | Put parameters under front-panel knobs, morph groups and MIDI CCs |
| `set_morph_value`, `play_note` | Turn a morph dial; play a note to hear or meter the result |

Most tools take a `slot` (0–3 = A–D, default: the active tab) and a `section`
(0 = common, 1 = poly). Grid coordinates are in module-column units, not pixels,
and `add_module` can place modules for you.

Full parameter reference: `mcp-bridge/README.md` in the repository.

## Practical notes

- **Watch it work.** The canvas updates live, so you can see what the assistant
  is building. If you don't like a step, `Ctrl+Z`.
- **`describe_module_type` first.** Connector names on the G1 are terse and not
  guessable; an assistant that cables without looking them up will fail.
- **Prefer `mutate_patch`** to a burst of `set_parameter` calls: it is one undo
  step and respects locks, excluded modules and Output modules.
- **Parameter names can change.** `set_parameter` takes either `parameterId`,
  which is stable, or `parameterName`, which is a label and may be corrected:
  the two mixers' input attenuators were renamed from `in sense N` to
  `in level N`. Retired names keep working, so an existing script does not
  break, but `parameterId` is the safer thing to store.
- **`store_to_bank` needs a connected synth** with its patch list loaded; it
  uploads first and writes to the bank only once the upload is acknowledged.
- **Let it check its work on the synth.** An edit call only says the editor
  accepted it. `get_events` says what the synth did afterwards (an error, a
  dropped connection, a knob you turned), and `play_note` followed by
  `read_lights` shows whether the patch actually makes signal. None of the
  reading tools send anything to the synth.
- **Knobs are named as on the panel.** `assign_knob` takes `"Knob 7"`,
  `"Pedal"`, `"After touch"` or `"On/Off switch"`, or an index 0-22. A bare
  `"7"` is refused, since it could mean Knob 7 or index 7. A knob that already
  drives something is left alone unless the assistant asks to replace it.
- **LOCAL is not a sandbox yet.** While the synth is connected, edits to a slot
  marked LOCAL still reach it. Test in a slot whose patch you do not mind
  losing, with the banks backed up.
