# 9. El puente MCP (asistentes de IA)

Animatek NME puede exponer un pequeño canal de control local que permite a un
**cliente MCP** (Claude Code, Claude Desktop, OpenCode y otros) trabajar el
patch como si fueras tú en el canvas: añadir y mover módulos, conectar y cortar
cables, ajustar parámetros, mutar un patch, abrir patches de tu librería,
guardarlos y almacenarlos en un banco del sintetizador.

Todo pasa por el sistema de deshacer normal del editor, así que las ediciones de
un asistente aparecen en el canvas al momento, suben al sintetizador si hay uno
conectado, y son revisables y deshacibles exactamente igual que las tuyas
(`Ctrl+Z`).

## Está desactivado por defecto

No se abre ningún puerto a menos que lo pidas. Actívalo en **Editor Options
(`Ctrl+,`) → MCP Bridge**. El panel muestra:

- el **interruptor de activación**,
- si el puente está **escuchando** (verde) o ha **fallado** (ámbar),
- el **comando stdio** exacto que registrar en tu cliente, con un botón de
  *Copiar*.

El puente escucha solo en `127.0.0.1`, nunca en la red, y está completamente
fuera de juego cuando el interruptor está apagado. También se puede excluir de
la compilación con `-DNME_MCP_BRIDGE=OFF`. Es **exclusivo de la app
standalone**: deliberadamente no se compila dentro del plugin, donde varias
instancias se pelearían por el puerto.

## Configurar un cliente

El editor escucha en el socket local (`127.0.0.1:51027` por defecto). El
servidor MCP con el que habla tu cliente es `mcp-bridge/server.py`, incluido en
el repositorio:

```bash
cd mcp-bridge
python3 -m venv .venv
.venv/bin/pip install -e .
```

Después regístralo. Para Claude Code:

```bash
claude mcp add animatek-nme -- /ruta/a/mcp-bridge/.venv/bin/python /ruta/a/mcp-bridge/server.py
```

Si el editor encuentra `mcp-bridge/` cerca de su propio ejecutable, rellena ese
comando exacto en el panel de Editor Options por ti; si no, usa las rutas de tu
propia copia. Animatek NME tiene que estar **abierto** con el puente activado;
las herramientas devuelven un error claro si no lo está.

## Qué puede hacer un asistente

| Herramienta | Para qué sirve |
|-------------|----------------|
| `list_module_types`, `describe_module_type` | Recorrer la paleta de más de 110 módulos; obtener los nombres exactos de conectores y parámetros de un tipo |
| `list_modules` | Leer los módulos, conectores y cables de un patch |
| `list_patches` | Slots cargados y tu librería en disco |
| `create_patch`, `open_patch` | Empezar un patch vacío, o cargar uno por nombre o ruta |
| `add_module`, `move_module`, `rename_module`, `delete_module` | Construir y organizar |
| `connect_cable`, `delete_cable` | Cablear |
| `set_parameter` | Fijar o ajustar un parámetro |
| `mutate_patch` | Ejecutar el propio Mutator del editor como un solo paso deshacible y regulado |
| `save_patch` | Escribir el patch de un slot (y su `.var`) a un `.pch` |
| `store_to_bank` | Subir el patch de un slot y almacenarlo en una posición de banco |

La mayoría de herramientas aceptan un `slot` (0–3 = A–D, por defecto la pestaña
activa) y una `section` (0 = común, 1 = poly). Las coordenadas de rejilla van en
unidades de columna de módulo, no en píxeles, y `add_module` puede colocar los
módulos por ti.

Referencia completa de parámetros: `mcp-bridge/README.md` en el repositorio.

## Notas prácticas

- **Míralo trabajar.** El canvas se actualiza en vivo, así que ves lo que está
  construyendo el asistente. Si un paso no te gusta, `Ctrl+Z`.
- **Primero `describe_module_type`.** Los nombres de conector del G1 son
  escuetos y no se adivinan; un asistente que cablee sin consultarlos fallará.
- **Mejor `mutate_patch`** que una ráfaga de llamadas a `set_parameter`: es un
  solo paso de deshacer y respeta bloqueos, módulos excluidos y módulos de
  salida.
- **`store_to_bank` necesita un sintetizador conectado** con su lista de patches
  cargada; sube primero y escribe en el banco solo cuando la subida se confirma.
