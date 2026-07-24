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
    section: Optional[int] = None,
    container_index: Optional[int | list[int]] = None,
    include_parameters: bool = True,
    include_morph: bool = False,
    include_connectors: bool = True,
    verbose_parameters: bool = False,
) -> Any:
    """List the modules and cables currently in a patch slot.

    slot: 0-3 (A-D); defaults to whichever slot's tab is currently active
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
    return _call("add_module", params)


@mcp.tool()
def move_module(
    section: int,
    container_index: int,
    grid_x: int,
    grid_y: int,
    slot: Optional[int] = None,
) -> Any:
    """Move an existing module to a new grid position.

    section: 0=common, 1=poly.
    container_index: module identifier returned by add_module/list_modules.
    grid_x/grid_y: non-negative grid coordinates. Account for each module's
    height when stacking modules vertically so they do not overlap.
    slot: 0-3 (A-D); defaults to the currently active slot.
    """
    params: dict[str, Any] = {
        "section": section,
        "containerIndex": container_index,
        "gridX": grid_x,
        "gridY": grid_y,
    }
    if slot is not None:
        params["slot"] = slot
    return _call("move_module", params)


@mcp.tool()
def rename_module(
    section: int,
    container_index: int,
    name: str,
    slot: Optional[int] = None,
) -> Any:
    """Rename an existing module (undoable).

    section: 0=common, 1=poly.
    container_index: module identifier returned by add_module/list_modules.
    name: new title, 1-16 characters (the G1 module-name limit). The name lives
    in the patch/editor and reaches the synth on the next full patch upload.
    slot: 0-3 (A-D); defaults to the currently active slot.
    Returns the new name and the previous one.
    """
    params: dict[str, Any] = {
        "section": section,
        "containerIndex": container_index,
        "name": name,
    }
    if slot is not None:
        params["slot"] = slot
    return _call("rename_module", params)


@mcp.tool()
def delete_module(
    section: int,
    container_index: int,
    slot: Optional[int] = None,
) -> Any:
    """Delete a module and its attached cables as one undoable operation."""
    params: dict[str, Any] = {
        "section": section,
        "containerIndex": container_index,
    }
    if slot is not None:
        params["slot"] = slot
    return _call("delete_module", params)


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
    return _call("set_parameter", params)


@mcp.tool()
def create_patch(
    slot: Optional[int] = None,
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
    if name is not None:
        params["name"] = name
    return _call("create_patch", params)


@mcp.tool()
def open_patch(
    slot: Optional[int] = None,
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
    if name is not None:
        params["name"] = name
    if path is not None:
        params["path"] = path
    return _call("open_patch", params)


@mcp.tool()
def save_patch(
    path: str,
    slot: Optional[int] = None,
) -> Any:
    """Save a slot's patch to a .pch file on disk.

    path: destination. Absolute paths are used as-is; a relative path resolves
    under the configured patches folder and must stay inside it. A missing
    extension defaults to .pch. Parent folders are created as needed. This lets a
    patch built through the bridge be persisted instead of living only in memory.
    slot: 0-3 (A-D); defaults to the currently active slot.
    Returns the full saved path and the patch name.
    """
    params: dict[str, Any] = {"path": path}
    if slot is not None:
        params["slot"] = slot
    return _call("save_patch", params)


@mcp.tool()
def store_to_bank(
    bank: int,
    position: int,
    slot: Optional[int] = None,
) -> Any:
    """Store a slot's patch into a synth bank location (requires a connected synth).

    bank: 1-9. position: 1-99 (bank location = bank*100 + position, e.g. 101).
    slot: 0-3 (A-D); defaults to the currently active slot.
    The patch is uploaded to the synth working slot and then written to the bank,
    so this overwrites that working slot. The patch list must have finished
    loading. Returns the target bank/position/location.
    """
    params: dict[str, Any] = {"bank": bank, "position": position}
    if slot is not None:
        params["slot"] = slot
    return _call("store_to_bank", params)


if __name__ == "__main__":
    mcp.run()
