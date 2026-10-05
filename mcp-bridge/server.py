"""MCP bridge for Animatek NME.

Lets an MCP client add modules and connect cables in a *live, running*
Animatek NME editor - changes appear on the canvas immediately, and upload
to the real synth if one is connected.

This process is the actual MCP server an MCP client talks to over stdio; it
just forwards each tool call as one line of JSON to a tiny control socket
embedded in AnimatekNME itself (source/mcp/McpBridgeServer.h) and returns
the JSON response. AnimatekNME must be running, with the bridge enabled
(the default), for any of this to work.
"""

import json
import socket
from typing import Any, Optional

from mcp.server.fastmcp import FastMCP

HOST = "127.0.0.1"
PORT = 51027
TIMEOUT_SECONDS = 10

mcp = FastMCP("animatek-nme")


class BridgeError(RuntimeError):
    """The embedded C++ bridge returned an error, or couldn't be reached."""


def _call(method: str, params: Optional[dict[str, Any]] = None) -> Any:
    request = {"id": 1, "method": method, "params": params or {}}
    try:
        with socket.create_connection((HOST, PORT), timeout=TIMEOUT_SECONDS) as sock:
            sock.sendall((json.dumps(request) + "\n").encode("utf-8"))
            sock.settimeout(TIMEOUT_SECONDS)
            buf = b""
            while not buf.endswith(b"\n"):
                chunk = sock.recv(65536)
                if not chunk:
                    break
                buf += chunk
    except (ConnectionRefusedError, socket.timeout, OSError) as exc:
        raise BridgeError(
            f"Could not reach AnimatekNME's MCP bridge at {HOST}:{PORT} - "
            "is the editor running, with the bridge enabled?"
        ) from exc

    if not buf:
        raise BridgeError("AnimatekNME closed the connection without a response")

    response = json.loads(buf.decode("utf-8"))
    if not response.get("ok"):
        error = response.get("error", {})
        raise BridgeError(f"{error.get('code', 'unknown_error')}: {error.get('message', '')}")
    return response.get("result")


@mcp.tool()
def list_module_types(
    category: Optional[str] = None,
    include_connectors: bool = False,
) -> Any:
    """Browse the Nord Modular module types available to add.

    Returns typeId, name and category for each - enough to pick one, then call
    describe_module_type for its connectors and parameters.

    category: optional filter, e.g. "Oscillator", "Filter", "LFO", "Envelope",
    "Mixer", "Logic", "Audio", "Control", "In/Out", "Seqencer" (sic).
    include_connectors: return every connector of every listed type. Large -
    prefer describe_module_type, or narrow with category first.
    """
    params: dict[str, Any] = {}
    if category is not None:
        params["category"] = category
    if include_connectors:
        params["includeConnectors"] = True
    return _call("list_module_types", params)


@mcp.tool()
def describe_module_type(
    type_id: Optional[int] = None,
    type_name: Optional[str] = None,
    include_morph: bool = False,
) -> Any:
    """Describe one module type: its connectors and its parameters.

    This is how to find the exact connector names connect_cable expects (they
    are terse and not guessable - "slv", "mst", "freq mod", "out left"), and
    what set_parameter will accept, without adding the module first.

    Provide exactly one of type_id or type_name (case-insensitive).
    include_morph: also list the "morph:" parameter twins, omitted by default.
    """
    params: dict[str, Any] = {}
    if type_id is not None:
        params["typeId"] = type_id
    if type_name is not None:
        params["typeName"] = type_name
    if include_morph:
        params["includeMorph"] = True
    return _call("describe_module_type", params)


@mcp.tool()
def list_modules(
    slot: Optional[int] = None,
    port: Optional[int] = None,
    section: Optional[int] = None,
    container_index: Optional[int | list[int]] = None,
    include_parameters: bool = True,
    include_morph: bool = False,
    include_connectors: bool = True,
    verbose_parameters: bool = False,
) -> Any:
    """List the modules and cables currently in a patch slot.

    slot: 0-3 (A-D); defaults to whichever slot's tab is currently active
    port: 1-4, the synth (as MIDI Setup numbers its ports); defaults to the one being edited.
    in the editor.
    section: 0=common, 1=poly; omit to list both.
    container_index: one index or a list of them - return only those modules,
    plus the cables touching them.

    By default each module reports its connectors (so its cables can be wired
    without a separate lookup) and its non-morph parameters with current values.

    A large patch is still a large response. The pattern that scales is to call
    once with include_parameters=False for the structure, then again with
    container_index for the handful of modules you actually care about.
    include_morph adds the "morph:" twins and verbose_parameters adds each
    parameter's min/max; both roughly double the size.
    """
    params: dict[str, Any] = {}
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    if section is not None:
        params["section"] = section
    if container_index is not None:
        params["containerIndex"] = container_index
    params["includeParameters"] = include_parameters
    params["includeMorph"] = include_morph
    params["includeConnectors"] = include_connectors
    params["verboseParameters"] = verbose_parameters
    return _call("list_modules", params)


@mcp.tool()
def mutate_patch(
    operation: str = "mutate",
    probability: float = 0.5,
    range: float = 0.25,
    slot: Optional[int] = None,
    port: Optional[int] = None,
) -> Any:
    """Mutate or randomize a patch's parameters using the editor's own engine.

    Prefer this over a series of set_parameter calls: it is one undoable step,
    it is delivered through the connection's throttled queue instead of a burst
    of sends, and it applies the editor's musical rules rather than raw random
    values.

    operation: "mutate" (random offsets from the current sound) or "randomize"
    (completely new values).
    probability: chance per parameter of being touched, 0..1. Mutate only.
    range: maximum offset as a fraction of each parameter's span, 0..1. Mutate
    only - small values (0.1-0.2) explore around the current sound, large ones
    depart from it.
    slot: 0-3 (A-D); defaults to the currently active slot.
    port: 1-4, the synth (as MIDI Setup numbers its ports); defaults to the one being edited.

    Parameters that are locked, in a module excluded from mutation, or in an
    Output module are never touched. Interpolate and cross are not exposed:
    they need a second parent snapshot this API has no way to name yet.

    Returns how many parameters actually changed.
    """
    params: dict[str, Any] = {
        "operation": operation,
        "probability": probability,
        "range": range,
    }
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    return _call("mutate_patch", params)


@mcp.tool()
def list_patches(query: Optional[str] = None) -> Any:
    """List patches loaded in slots and .pch files in the configured library.

    query: optional case-insensitive name/path filter for library patches.
    Results include exact paths, which disambiguate duplicate patch names.
    """
    params: dict[str, Any] = {}
    if query is not None:
        params["query"] = query
    return _call("list_patches", params)


@mcp.tool()
def add_module(
    section: int,
    grid_x: Optional[int] = None,
    grid_y: Optional[int] = None,
    auto_place: bool = True,
    type_id: Optional[int] = None,
    type_name: Optional[str] = None,
    name: Optional[str] = None,
    slot: Optional[int] = None,
    port: Optional[int] = None,
) -> Any:
    """Add a module to a patch - it appears immediately on the editor's canvas.

    section: 0=common, 1=poly (most modules go in poly).
    auto_place: true by default. Modules are stacked vertically in the same
    column (same grid_x, increasing grid_y), with one clear row between them.
    After 8 modules, or when the column runs out of vertical room, placement
    continues at the top of the next adjacent column.
    grid_x/grid_y: ignored while auto_place is true. Set auto_place=false and
    provide both coordinates only when an exact manual position is required.
    type_id or type_name: identifies the module type - get these from
    list_module_types first (typeId is the more reliable of the two).
    slot: 0-3 (A-D); defaults to the currently active slot.
    port: 1-4, the synth (as MIDI Setup numbers its ports); defaults to the one being edited.

    Returns the new module's containerIndex and actual position.
    """
    params: dict[str, Any] = {"section": section, "autoPlace": auto_place}
    if grid_x is not None:
        params["gridX"] = grid_x
    if grid_y is not None:
        params["gridY"] = grid_y
    if type_id is not None:
        params["typeId"] = type_id
    if type_name is not None:
        params["typeName"] = type_name
    if name is not None:
        params["name"] = name
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    return _call("add_module", params)


@mcp.tool()
def move_module(
    section: int,
    container_index: int,
    grid_x: int,
    grid_y: int,
    slot: Optional[int] = None,
    port: Optional[int] = None,
) -> Any:
    """Move an existing module to a new grid position.

    section: 0=common, 1=poly.
    container_index: module identifier returned by add_module/list_modules.
    grid_x/grid_y: non-negative grid coordinates. Account for each module's
    height when stacking modules vertically so they do not overlap.
    slot: 0-3 (A-D); defaults to the currently active slot.
    port: 1-4, the synth (as MIDI Setup numbers its ports); defaults to the one being edited.
    """
    params: dict[str, Any] = {
        "section": section,
        "containerIndex": container_index,
        "gridX": grid_x,
        "gridY": grid_y,
    }
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    return _call("move_module", params)


@mcp.tool()
def rename_module(
    section: int,
    container_index: int,
    name: str,
    slot: Optional[int] = None,
    port: Optional[int] = None,
) -> Any:
    """Rename an existing module (undoable).

    section: 0=common, 1=poly.
    container_index: module identifier returned by add_module/list_modules.
    name: new title, 1-16 characters (the G1 module-name limit). The name lives
    in the patch/editor and reaches the synth on the next full patch upload.
    slot: 0-3 (A-D); defaults to the currently active slot.
    port: 1-4, the synth (as MIDI Setup numbers its ports); defaults to the one being edited.
    Returns the new name and the previous one.
    """
    params: dict[str, Any] = {
        "section": section,
        "containerIndex": container_index,
        "name": name,
    }
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    return _call("rename_module", params)


@mcp.tool()
def delete_module(
    section: int,
    container_index: int,
    slot: Optional[int] = None,
    port: Optional[int] = None,
) -> Any:
    """Delete a module and its attached cables as one undoable operation."""
    params: dict[str, Any] = {
        "section": section,
        "containerIndex": container_index,
    }
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    return _call("delete_module", params)


@mcp.tool()
def replace_module(
    section: int,
    container_index: int,
    type_name: Optional[str] = None,
    type_id: Optional[int] = None,
    slot: Optional[int] = None,
    port: Optional[int] = None,
) -> Any:
    """Swap a module for another of its family in place, as one undoable step.

    The new type must be in the same category (a FilterE can become a FilterD,
    not an LFO); the error lists the family otherwise. The module keeps its
    position and containerIndex, its name if it was renamed, the cables whose
    connector exists on the new type (matched by name, or where only one choice
    is possible), the parameter values whose name and range match, and the
    knob, morph and MIDI CC assignments on those parameters. Everything else is
    dropped, never guessed: the result counts keptCables, droppedCables,
    keptParameters and droppedAssignments. A taller module pushes the ones below
    it down its column, and fails if the column has no room.
    """
    params: dict[str, Any] = {"section": section, "containerIndex": container_index}
    if type_name is not None:
        params["typeName"] = type_name
    if type_id is not None:
        params["typeId"] = type_id
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    return _call("replace_module", params)


@mcp.tool()
def connect_cable(
    section: int,
    out_container_index: int,
    out_connector: str,
    in_container_index: int,
    in_connector: str,
    out_is_output: Optional[bool] = None,
    in_is_output: Optional[bool] = None,
    slot: Optional[int] = None,
    port: Optional[int] = None,
) -> Any:
    """Connect a cable between two modules' connectors in a patch.

    section: 0=common, 1=poly - both endpoints must be in the same section.
    out_container_index/out_connector, in_container_index/in_connector:
    identify each endpoint by the module's containerIndex (from add_module's
    result or list_modules) and the connector's name (from
    list_module_types or list_modules). Despite the "out"/"in" naming these
    are just the two endpoints being joined, not a required direction - the
    editor works out the actual signal polarity itself, and rejects two
    outputs joined together or two already-differently-driven cable nets
    being merged.
    out_is_output/in_is_output: only needed if a connector name is
    ambiguous (a handful of module types reuse the same name for an input
    and an output) - the error message says so if this is required.
    slot: 0-3 (A-D); defaults to the currently active slot.
    port: 1-4, the synth (as MIDI Setup numbers its ports); defaults to the one being edited.
    """
    params: dict[str, Any] = {
        "section": section,
        "out": {"containerIndex": out_container_index, "connector": out_connector},
        "in": {"containerIndex": in_container_index, "connector": in_connector},
    }
    if out_is_output is not None:
        params["out"]["isOutput"] = out_is_output
    if in_is_output is not None:
        params["in"]["isOutput"] = in_is_output
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    return _call("connect_cable", params)


@mcp.tool()
def delete_cable(
    section: int,
    out_container_index: int,
    out_connector: str,
    in_container_index: int,
    in_connector: str,
    out_is_output: Optional[bool] = None,
    in_is_output: Optional[bool] = None,
    slot: Optional[int] = None,
    port: Optional[int] = None,
) -> Any:
    """Delete one direct cable identified by the endpoint module/connectors."""
    params: dict[str, Any] = {
        "section": section,
        "out": {"containerIndex": out_container_index, "connector": out_connector},
        "in": {"containerIndex": in_container_index, "connector": in_connector},
    }
    if out_is_output is not None:
        params["out"]["isOutput"] = out_is_output
    if in_is_output is not None:
        params["in"]["isOutput"] = in_is_output
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    return _call("delete_cable", params)


@mcp.tool()
def set_parameter(
    section: int,
    container_index: int,
    parameter_name: Optional[str] = None,
    parameter_id: Optional[int] = None,
    value: Optional[int] = None,
    delta: Optional[int] = None,
    slot: Optional[int] = None,
    port: Optional[int] = None,
) -> Any:
    """Set or adjust one editable module parameter.

    Identify the parameter by its exact case-insensitive name or parameterId
    from list_modules. Provide exactly one of value (absolute) or delta
    (relative); values are clamped to the reported min/max. For descriptions
    such as "more percussive", adjust the relevant ADSR parameters separately.
    """
    params: dict[str, Any] = {
        "section": section,
        "containerIndex": container_index,
    }
    if parameter_name is not None:
        params["parameterName"] = parameter_name
    if parameter_id is not None:
        params["parameterId"] = parameter_id
    if value is not None:
        params["value"] = value
    if delta is not None:
        params["delta"] = delta
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    return _call("set_parameter", params)


@mcp.tool()
def create_patch(
    slot: Optional[int] = None,
    port: Optional[int] = None,
    name: Optional[str] = None,
    activate: bool = True,
) -> Any:
    """Replace a slot with a new empty patch and optionally make it active.

    This is destructive to the slot's current patch, so save it first if it
    must be kept. slot is 0-3 (A-D); omitted means the active slot.
    """
    params: dict[str, Any] = {"activate": activate}
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    if name is not None:
        params["name"] = name
    return _call("create_patch", params)


@mcp.tool()
def open_patch(
    slot: Optional[int] = None,
    port: Optional[int] = None,
    name: Optional[str] = None,
    path: Optional[str] = None,
    activate: bool = True,
) -> Any:
    """Open a disk-library .pch in a chosen slot.

    Provide exactly one of name or path. Name matching is case-insensitive and
    exact; duplicate names return an ambiguity error requiring the exact path
    from list_patches. Relative paths resolve under the configured library.
    """
    params: dict[str, Any] = {"activate": activate}
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    if name is not None:
        params["name"] = name
    if path is not None:
        params["path"] = path
    return _call("open_patch", params)


@mcp.tool()
def save_patch(
    path: str,
    slot: Optional[int] = None,
    port: Optional[int] = None,
) -> Any:
    """Save a slot's patch to a .pch file on disk.

    path: destination. Absolute paths are used as-is; a relative path resolves
    under the configured patches folder and must stay inside it. A missing
    extension defaults to .pch. Parent folders are created as needed. This lets a
    patch built through the bridge be persisted instead of living only in memory.
    slot: 0-3 (A-D); defaults to the currently active slot.
    port: 1-4, the synth (as MIDI Setup numbers its ports); defaults to the one being edited.
    Returns the full saved path and the patch name.
    """
    params: dict[str, Any] = {"path": path}
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    return _call("save_patch", params)


@mcp.tool()
def store_to_bank(
    bank: int,
    position: int,
    slot: Optional[int] = None,
    port: Optional[int] = None,
) -> Any:
    """Store a slot's patch into a synth bank location (requires a connected synth).

    bank: 1-9. position: 1-99 (bank location = bank*100 + position, e.g. 101).
    slot: 0-3 (A-D); defaults to the currently active slot.
    port: 1-4, the synth (as MIDI Setup numbers its ports); defaults to the one being edited.
    The patch is uploaded to the synth working slot and then written to the bank,
    so this overwrites that working slot. The patch list must have finished
    loading. Returns the target bank/position/location.
    """
    params: dict[str, Any] = {"bank": bank, "position": position}
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    return _call("store_to_bank", params)


# --- Reading the synth back ---------------------------------------------------


@mcp.tool()
def get_synth_status() -> Any:
    """What the editor knows about the synth right now. Sends nothing to the synth.

    Read it before testing against hardware, and again after anything that
    should have changed it. Reports the connection and synth OS version, the
    slot the synth has focused versus the editor's active tab, whether a patch
    fetch or upload is in flight and whether edits are still queued, and for
    each slot: the patch name, whether it is LOCAL (the editor's patch is not
    known to match the synth's - edits to it are still sent while connected),
    whether the synth has it enabled, its voice count and its bank location.

    latestEventSeq is the value to pass as `after` to get_events.
    """
    return _call("get_synth_status")


@mcp.tool()
def get_events(
    after: int = 0,
    limit: int = 100,
    types: Optional[list[str]] = None,
) -> Any:
    """What the synth and the connection did since event `after`, oldest first.

    A tool call only answers for itself. This is how to find out that the synth
    replied with an error, the connection dropped, or someone turned a
    front-panel knob in between. Pass the returned latestSeq as `after` on the
    next call. truncated=true means events were lost: the log keeps the last 512.

    Types: connection, synth_error (code, description), synth_parameter (a value
    the synth reported by itself: a front-panel knob, a morph dial - section 2,
    containerIndex 1 - or a MIDI CC), slot_focus, slots_enabled, patch_received,
    patch_incomplete, voice_count, upload_failed (slot, reason: the synth did not
    take the patch, so the slot is LOCAL). A connection event with state
    "disconnected" and "Synth not responding" means the synth stopped answering
    after a timeout; the editor reconnects by itself when it speaks again.
    types: optional filter, e.g. ["synth_error", "connection"]. limit: 1-500.
    """
    params: dict[str, Any] = {"after": after, "limit": limit}
    if types is not None:
        params["types"] = types
    return _call("get_events", params)


@mcp.tool()
def read_lights(
    section: Optional[int] = None,
    container_index: Optional[int | list[int]] = None,
    slot: Optional[int] = None,
    port: Optional[int] = None,
) -> Any:
    """Read the LEDs and meters the synth is streaming, per module.

    The G1 streams lights only for the slot it has focused, which is the
    editor's active slot; asking for another slot is an error. Only modules that
    have LEDs or meters are listed. LEDs are 0-3. Meters come in wire-order
    pairs (channel B, channel A): a single meter reads the first value, a
    stereo meter's left side the second.

    lastChangeAgeMs is the time since any value last changed. stale=true means
    no values have arrived for this slot yet.
    Pair it with play_note to check that a patch actually makes signal.
    """
    params: dict[str, Any] = {}
    if section is not None:
        params["section"] = section
    if container_index is not None:
        params["containerIndex"] = container_index
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    return _call("read_lights", params)


@mcp.tool()
def list_bank(bank: int, include_empty: bool = True) -> Any:
    """List what the synth holds in each position (1-99) of one bank.

    bank: 1-9. Each position reports its location (bank*100 + position, the
    number store_to_bank uses) and its patch name, or null when it is empty.
    store_to_bank overwrites a position without asking, so check here first.
    Comes from the patch list the editor fetched on connect: requires a
    connected synth and a finished list. include_empty=False lists only the
    used positions.
    """
    return _call("list_bank", {"bank": bank, "includeEmpty": include_empty})


# --- Knob, morph and MIDI CC assignments ----------------------------------------


def _assign_target(
    params: dict[str, Any],
    section: Optional[int],
    container_index: Optional[int],
    parameter_name: Optional[str],
    parameter_id: Optional[int],
    morph_group: Optional[int] = None,
) -> dict[str, Any]:
    if morph_group is not None:
        params["morphGroup"] = morph_group
    if section is not None:
        params["section"] = section
    if container_index is not None:
        params["containerIndex"] = container_index
    if parameter_name is not None:
        params["parameterName"] = parameter_name
    if parameter_id is not None:
        params["parameterId"] = parameter_id
    return params


@mcp.tool()
def list_assignments(slot: Optional[int] = None) -> Any:
    """List a patch's front-panel knob, morph group and MIDI CC assignments.

    knobs: each assigned knob, by index and knobName ("Knob 7"), and its target.
    morphGroups: the four groups (0-3) with their dial value, keyboard setting
    and member parameters with their signed range. morphAssignmentLimit is the
    G1's 25 per patch. midiCcs: each assigned CC and its target.

    A target is either a module parameter (section, containerIndex, parameterId,
    moduleName, parameterName) or a morph group's dial (morphGroup).
    slot: 0-3 (A-D); defaults to the currently active slot.
    port: 1-4, the synth (as MIDI Setup numbers its ports); defaults to the one being edited.
    """
    params: dict[str, Any] = {}
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    return _call("list_assignments", params)


@mcp.tool()
def assign_knob(
    knob: int | str,
    section: Optional[int] = None,
    container_index: Optional[int] = None,
    parameter_name: Optional[str] = None,
    parameter_id: Optional[int] = None,
    morph_group: Optional[int] = None,
    replace: bool = False,
    slot: Optional[int] = None,
    port: Optional[int] = None,
) -> Any:
    """Put a parameter, or a morph group's dial, under a front-panel knob. Undoable.

    knob: an index 0-22 or a name - "Knob 1" to "Knob 18", "Pedal",
    "After touch", "On/Off switch". A bare number in a string ("7") is refused
    because it is ambiguous; use 6 or "Knob 7".
    Target: section + container_index + parameter_name or parameter_id, or
    morph_group (0-3) on its own.

    A parameter already on another knob moves to this one. If the knob already
    drives something else the call fails with knob_in_use, unless replace=True,
    which frees it first; one Ctrl+Z then gives it back.
    """
    params = _assign_target({"knob": knob, "replace": replace}, section,
                            container_index, parameter_name, parameter_id, morph_group)
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    return _call("assign_knob", params)


@mcp.tool()
def unassign_knob(knob: int | str, slot: Optional[int] = None) -> Any:
    """Free a front-panel knob, whatever it drives. Undoable.

    knob: an index 0-22 or a name, as for assign_knob.
    """
    params: dict[str, Any] = {"knob": knob}
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    return _call("unassign_knob", params)


@mcp.tool()
def assign_morph(
    section: int,
    container_index: int,
    group: int,
    range: int = 0,
    parameter_name: Optional[str] = None,
    parameter_id: Optional[int] = None,
    slot: Optional[int] = None,
    port: Optional[int] = None,
) -> Any:
    """Put a module parameter in a morph group, with a signed range. Undoable.

    group: 0-3. range: -127 to 127, the span the morph dial sweeps the
    parameter through (negative sweeps it down). 0 assigns it without movement,
    which is what the canvas menu does.
    Calling it on a parameter already in a group moves it to this group/range.
    A G1 patch holds at most 25 morph assignments (morph_limit_reached).
    Identify the parameter with parameter_name or parameter_id.
    """
    params = _assign_target({"group": group, "range": range}, section,
                            container_index, parameter_name, parameter_id)
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    return _call("assign_morph", params)


@mcp.tool()
def unassign_morph(
    section: int,
    container_index: int,
    parameter_name: Optional[str] = None,
    parameter_id: Optional[int] = None,
    slot: Optional[int] = None,
    port: Optional[int] = None,
) -> Any:
    """Take a module parameter out of its morph group. Undoable."""
    params = _assign_target({}, section, container_index, parameter_name, parameter_id)
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    return _call("unassign_morph", params)


@mcp.tool()
def assign_midi_cc(
    cc: int,
    section: Optional[int] = None,
    container_index: Optional[int] = None,
    parameter_name: Optional[str] = None,
    parameter_id: Optional[int] = None,
    morph_group: Optional[int] = None,
    replace: bool = False,
    slot: Optional[int] = None,
    port: Optional[int] = None,
) -> Any:
    """Put a parameter, or a morph group's dial, under a MIDI CC. Undoable.

    cc: 0-119 (120-127 are channel mode messages).
    Target: section + container_index + parameter_name or parameter_id, or
    morph_group (0-3) on its own. A CC already driving something else fails
    with cc_in_use unless replace=True.
    """
    params = _assign_target({"cc": cc, "replace": replace}, section,
                            container_index, parameter_name, parameter_id, morph_group)
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    return _call("assign_midi_cc", params)


@mcp.tool()
def unassign_midi_cc(cc: int, slot: Optional[int] = None) -> Any:
    """Free a MIDI CC, whatever it drives. Undoable."""
    params: dict[str, Any] = {"cc": cc}
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    return _call("unassign_midi_cc", params)


# --- Playing the synth ------------------------------------------------------------


@mcp.tool()
def set_morph_value(
    morph_group: int,
    value: Optional[int] = None,
    delta: Optional[int] = None,
    slot: Optional[int] = None,
    port: Optional[int] = None,
) -> Any:
    """Turn a morph group's dial, as dragging it in the editor's header bar does.

    morph_group: 0-3. Provide exactly one of value (0-127, clamped) or delta.
    Every parameter in the group moves by its range. Like the dial, this is not
    an undo step.
    """
    params: dict[str, Any] = {"morphGroup": morph_group}
    if value is not None:
        params["value"] = value
    if delta is not None:
        params["delta"] = delta
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    return _call("set_morph_value", params)


# --- Connection -------------------------------------------------------------


@mcp.tool()
def list_midi_ports() -> Any:
    """The MIDI ports the editor can connect to, and which pair it is on now.

    Returns inputs and outputs (id and name each), whether a synth is connected,
    the status message, and the current input/output ids. Sends nothing.
    A real G1 usually sits behind a USB MIDI interface; the emulator publishes
    "G1-Emu PC Port" (the editor port) and "G1-Emu MIDI".
    """
    return _call("list_midi_ports")


@mcp.tool()
def connect_midi(input: str, output: str) -> Any:
    """Connect the editor to a MIDI input and output, as the MIDI settings dialog does.

    input/output: a port id, its exact name, or a part of the name only one port
    has, as list_midi_ports gives them. On Linux the names are the ALSA port's
    alone ("PC Port" for G1-Emu, "UM-ONE MIDI 1" for a real G1 on that cable),
    so "UM-ONE" works but "G1-Emu" does not. Ambiguous or unknown names are
    refused with the list of ports. The previous connection is dropped first and
    the handshake runs in the background: poll get_synth_status until connection
    is "connected" and synthName is not null (it stays null until the synth that
    answered sends its settings), and check that name before changing anything
    on it. The ports are remembered for the next start.
    """
    return _call("connect_midi", {"input": input, "output": output})


@mcp.tool()
def disconnect_midi() -> Any:
    """Disconnect the editor from the synth, as the MIDI settings dialog does."""
    return _call("disconnect_midi")


@mcp.tool()
def fetch_patch(slot: Optional[int] = None) -> Any:
    """Ask the synth for the patch in a slot again and load it into the editor.

    slot: 0-3 (A-D); defaults to the slot the synth has focused.
    port: 1-4, the synth (as MIDI Setup numbers its ports); defaults to the one being edited.
    The editor's copy of that slot is replaced when the patch arrives, so unsaved
    editor-only changes to it are lost; the synth itself is not changed. Poll
    get_synth_status until transfer.fetching is false, or read get_events for
    patch_received / patch_incomplete. Refused while another transfer is running.
    Pair it with save_patch to compare what the synth holds with what was sent.
    """
    params: dict[str, Any] = {}
    if slot is not None:
        params["slot"] = slot
    if port is not None:
        params["port"] = port
    return _call("fetch_patch", params)


@mcp.tool()
def play_note(note: int, duration_ms: int = 500) -> Any:
    """Play one note on the synth and release it after duration_ms (10-10000).

    note: 0-127, 60 = middle C. It sounds on the slot the synth has focused
    (synthFocusedSlot in get_synth_status). The editor protocol carries no
    velocity. Requires a connected synth.
    To check the note reached the patch, call read_lights while it is held: an
    envelope's gate LED reads 1. Voice counts are not a sound check - the G1
    does not report a change when a note plays.
    """
    return _call("play_note", {"note": note, "durationMs": duration_ms})


if __name__ == "__main__":
    mcp.run()
