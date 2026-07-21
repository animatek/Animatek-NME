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
def list_module_types() -> Any:
    """List every Nord Modular module type available to add, with its
    typeId, name, category, and connectors (name, direction, signal type)."""
    return _call("list_module_types")


@mcp.tool()
def list_modules(slot: Optional[int] = None, section: Optional[int] = None) -> Any:
    """List the modules and cables currently in a patch slot.

    slot: 0-3 (A-D); defaults to whichever slot's tab is currently active
    in the editor.
    section: 0=common, 1=poly; omit to list both.
    """
    params: dict[str, Any] = {}
    if slot is not None:
        params["slot"] = slot
    if section is not None:
        params["section"] = section
    return _call("list_modules", params)


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


if __name__ == "__main__":
    mcp.run()
