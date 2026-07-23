# Patreon post — Animatek NME 0.10.0

Two language versions below. Publish one, delete the other (or post both).

---

## English

# 🎛️ Animatek NME 0.10.0 — Nord Classic, and an editor your AI can drive

The biggest visual pass since the editor got themes at all, plus a genuinely new way to
work with it.

### 🎨 "Nord Classic" — the original editor's look

A new light, warm-grey theme that echoes the Clavia Nord Modular editor: flat grey module
bodies over a darker lavender-grey canvas, black labels, grey knobs, indigo LCD-style value
readouts and the classic cable colours — all sampled from the original.

A subtle grain now sits over the patch canvas on **every** theme, giving it a soft paper
feel instead of a flat fill.

(Housekeeping: the old "Classic" theme used Nomad's own colours rather than Clavia's, so it
is now called **"Nomad"**. The little-used "Frost" theme is gone.)

### 🤖 Drive the editor from an AI assistant

This one is new territory. The editor can now expose a local control channel that lets an
MCP client — Claude Code, Claude Desktop, OpenCode — work the patch as if it were you at
the canvas: adding and moving modules, connecting and cutting cables, setting parameters,
creating patches and opening them from your preset library.

Everything goes through the normal undo system, so an assistant's edits are reviewable and
undoable exactly like your own. Ask it to sketch a patch and then take over by hand.

**It is off by default and opens no port unless you ask.** Turn it on in
*Editor Options → MCP Bridge*, which shows whether the bridge is listening and gives you the
exact command to register it with your client. It binds to `127.0.0.1` only and can be
compiled out entirely. Standalone app only — not in the plugin.

### 🖥️ Light themes that actually work

A lot of the chrome had hard-coded light greys that vanished on a light background. The
header, status bar, slot list, inspector, module browser and every settings dialog follow
the palette now. The menu bar repaints when you switch themes, and the Theme submenu's
checkmark finally follows the theme you're actually using.

Buttons, menu highlights, combo arrows and toggles across every dialog dropped their
mismatched green/orange/red accents, so the whole application reads as one piece.

### 🎚️ See your knob assignments at a glance

A compact 18-LED diagram in the Inspector mirrors the physical Nord Modular knob layout.
Assigned knobs glow; free ones stay dark. The Knob Floater matches.

### 🩹 Fixes

- **Editing one slot no longer waits on another slot's transfer.** With a slot window open,
  turning a knob in slot A while slot B was uploading or downloading used to do nothing.
  Verified on hardware with a patch at 100% DSP load and two windows open.
- **DSP load reads to one decimal** (`47.6%`) like the original editor — a 99.5% patch no
  longer rounds up to a misleading `100%`.
- **Long editing sessions no longer run out of module slots.** Enough add/delete cycles used
  to push an internal counter past its limit and produce patches the synth rejected.
- Sequencer step numbers line up with their LEDs; reversed vertical selectors show the
  correct labels; Quick Add opens on the monitor your mouse is on; a new PCH2 filter hides
  legacy 2.10 patches in the Disk browser.

Special thanks to **Nocticore** for the detailed reports behind several of these.

---

*Back up your patch libraries before upgrading, and keep each patch's `.var` sidecar next to
its `.pch` to preserve variations and mutation exclusions.*

---

## Español

# 🎛️ Animatek NME 0.10.0 — Nord Classic, y un editor que tu IA puede manejar

El mayor repaso visual desde que el editor tiene temas, y además una forma nueva de
trabajar con él.

### 🎨 "Nord Classic" — el aspecto del editor original

Un tema nuevo, claro y de grises cálidos, que evoca el editor de Clavia: cuerpos de módulo
grises planos sobre un lienzo gris lavanda más oscuro, etiquetas negras, knobs grises,
lecturas tipo LCD índigo y los colores de cable clásicos, todo tomado del original.

Además, un grano muy sutil cubre ahora el lienzo en **todos** los temas, dándole tacto de
papel en vez de un relleno plano.

(Aviso: el antiguo tema "Classic" usaba los colores de Nomad, no los de Clavia, así que
ahora se llama **"Nomad"**. El poco usado "Frost" desaparece.)

### 🤖 Maneja el editor desde un asistente de IA

Esto es territorio nuevo. El editor puede abrir un canal de control local que permite a un
cliente MCP —Claude Code, Claude Desktop, OpenCode— trabajar el patch como si fueras tú
delante del lienzo: añadir y mover módulos, conectar y cortar cables, ajustar parámetros,
crear patches y abrirlos desde tu biblioteca.

Todo pasa por el sistema de undo normal, así que las ediciones del asistente se revisan y
se deshacen igual que las tuyas. Puedes pedirle que esboce un patch y seguir tú a mano.

**Está desactivado por defecto y no abre ningún puerto salvo que se lo pidas.** Se activa en
*Editor Options → MCP Bridge*, que indica si el puente está escuchando y te da el comando
exacto para registrarlo en tu cliente. Solo escucha en `127.0.0.1` y se puede compilar
fuera por completo. Solo en la app independiente, no en el plugin.

### 🖥️ Temas claros que por fin funcionan

Buena parte del chrome tenía grises claros fijos que desaparecían sobre fondo claro. La
cabecera, la barra de estado, la lista de slots, el inspector, el navegador de módulos y
todos los diálogos siguen ya la paleta. La barra de menús se repinta al cambiar de tema, y
la marca del submenú Theme por fin señala el tema que estás usando de verdad.

Botones, resaltados de menú, flechas de desplegable y conmutadores de todos los diálogos
han soltado sus acentos verdes/naranjas/rojos descoordinados, así que la aplicación entera
se lee como una sola pieza.

### 🎚️ Tus asignaciones de knobs de un vistazo

Un diagrama compacto de 18 LEDs en el Inspector reproduce la disposición física de knobs
del Nord Modular. Los asignados se encienden; los libres quedan apagados. El Knob Floater
hace juego.

### 🩹 Correcciones

- **Editar un slot ya no espera a la transferencia de otro.** Con una ventana de slot
  abierta, mover un knob del slot A mientras el B subía o bajaba no hacía nada. Verificado
  en hardware con un patch al 100% de carga de DSP y dos ventanas abiertas.
- **La carga de DSP se muestra con un decimal** (`47.6%`) como en el editor original: un
  patch al 99,5% ya no se redondea a un engañoso `100%`.
- **Las sesiones largas de edición ya no se quedan sin slots de módulo.** Suficientes ciclos
  de añadir y borrar empujaban un contador interno más allá de su límite y generaban patches
  que el sinte rechazaba.
- Los números de paso del secuenciador se alinean con sus LEDs; los selectores verticales
  invertidos muestran las etiquetas correctas; Quick Add se abre en el monitor donde está el
  ratón; y un filtro PCH2 nuevo oculta los patches 2.10 antiguos en el navegador de disco.

Gracias en especial a **Nocticore** por los informes detallados detrás de varias de estas.

---

*Haz copia de seguridad de tus bibliotecas antes de actualizar, y mantén el `.var` de cada
patch junto a su `.pch` para conservar variaciones y exclusiones de mutación.*
