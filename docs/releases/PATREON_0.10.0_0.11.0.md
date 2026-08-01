# Patreon post: Animatek NME 0.10.0 + 0.11.0

Two language versions are included below. Publish one or both.

## English

# 🎛️ Animatek NME 0.11.0: two releases in one update

I never published the Patreon post for version 0.10.0, so this release catches up with
everything added in both **0.10.0 and 0.11.0**.

You only need to download **0.11.0**. It already includes all the improvements from
0.10.0. If you are coming from 0.9.0 or an earlier version, this post covers everything
that has changed since then.

## 🎨 Nord Classic and a complete visual overhaul

Version 0.10.0 introduced **Nord Classic**, a light, warm-grey theme inspired by the
original Clavia Nord Modular editor. Module bodies, labels, knobs, LCD-style values and
signal cable colours were sampled from the original look.

A subtle procedural grain now covers the patch canvas in every theme, giving it a softer
paper-like texture. The old Classic theme has been renamed **Nomad**, while the rarely used
Frost theme has been removed.

Light themes also received a full usability pass. The header, status bar, slot list,
Inspector, module browser, menus and settings dialogs now follow the selected palette.
Buttons, highlights, toggles and combo boxes look consistent across the entire application.

## 🤖 Build and edit patches with an AI assistant

The new **MCP Bridge** allows compatible local assistants such as Claude Code, Claude
Desktop and OpenCode to work with the editor. An assistant can create patches, add or move
modules, connect cables, change parameters and open presets through the normal editor
operations.

All edits pass through the regular undo system, so every change can be reviewed and
undone just like a manual edit.

The bridge is **disabled by default** and opens no port unless you enable it under
*Editor Options > MCP Bridge*. It listens only on `127.0.0.1`, shows the exact registration
command for your client and can be removed completely at compile time. It is available
only in the standalone application.

Version 0.11.0 completes that workflow with several new tools:

- **`save_patch`** writes a patch and its `.var` sidecar to disk.
- **`store_to_bank`** uploads a patch and stores it in the synth's internal memory after
  the upload is acknowledged.
- **`rename_module`** renames modules as an undoable operation.
- **`mutate_patch`** runs the editor's Mutator as one controlled, undoable step.
- Lighter browsing tools make it easier to inspect module types, modules and connectors.

## 🎚️ Morph an entire patch between A and B

A new **Morph A/B fader** now sits in the patch header between the eight snapshot buttons
and the MUT button. It is an editor-side performance control that can smoothly transform
the whole patch between two captured sounds.

Right-click the fader to set endpoint **A** and endpoint **B**. Each endpoint can come from
the current sound or from any of the eight filled snapshots. Once both sides are set, drag
the fader from A to B to hear the transformation live.

The editor compares both captures and streams only the parameters that actually differ.
Identical parameters are left untouched, locked parameters are respected, and the four
regular morph knob values can move as part of the transition. Rapid fader movements pass
through the same coalescing queue used by the editor, so the synth receives an efficient
stream instead of a flood of redundant changes.

The fader can also be controlled from the Nord Modular front panel:

- Choose a physical knob directly from the fader's **Knob** menu. The editor automatically
  uses a free morph group as a silent carrier and creates a real hardware assignment.
- Use **Learn already-assigned knob**, then turn a panel knob, to reuse an existing control
  as the fader source.
- A knob assigned directly appears in the Inspector as **Morph A/B Fader** and can be
  removed there or from the fader menu.

This makes it possible to prepare two very different sounds and perform the transition
from the hardware without touching the mouse. The A/B captures are currently temporary
and are reset when the active slot changes or a different patch is loaded.

## 🪟 Slot windows now feel fully connected

Popped-out slot windows now receive the live stream from the synthesizer. Physical knob
movements, parameter values, morph knobs, lights and meters animate in the correct slot
window as well as in the main editor.

Randomize and Save also work per window:

- **Ctrl+R / Ctrl+Shift+R** randomizes the patch or the selected modules in that window.
- **Ctrl+S / Ctrl+Shift+S** saves or saves as from that window's slot.

Editing one slot no longer waits for another slot to finish a transfer. This was verified
on real hardware with two slot windows open and a patch at 100% DSP load.

## 📂 Choose where a patch opens, or inspect it locally

Opening a `.pch` no longer silently overwrites the active hardware slot. A new chooser
lists slots A, B, C and D with the patch currently loaded in each one.

The new **Local** option opens the patch in the editor without sending anything to the
synth. A **LOCAL** badge clearly marks any editor patch that is not known to match the
hardware. The badge disappears after the patch is uploaded or fetched from the synth.

## 🎚️ Better hardware feedback and daily workflow

A compact 18-LED diagram in the Inspector mirrors the physical Nord Modular knob layout.
Assigned knobs glow and free assignments stay dark. The Knob Floater uses the same visual
language.

There are also many smaller workflow improvements:

- DSP load is displayed with one decimal, such as `47.6%`.
- Long editing sessions reuse free module indices instead of eventually exhausting them.
- Sequencer step numbers line up correctly with their LEDs.
- Reversed vertical selectors display the correct labels.
- Quick Add opens on the monitor where the mouse is located.
- The Disk browser includes a PCH2 filter for hiding legacy 2.10 patches.
- Editor Options can be opened with **Ctrl+,**.

## 📖 The complete manual is online

The Animatek NME manual is now available in English and Spanish:

**https://animatek.net/animatek-nme/manual/**

It covers installation, MIDI connection, the interface, patch editing, the four slots,
floating tools, file formats, keyboard shortcuts, troubleshooting and the MCP Bridge.

## 🩹 Important safety and reliability fixes

- **Rapid Voices changes no longer corrupt a synth slot.** Fast clicks previously launched
  overlapping patch uploads that could leave the slot as a patch named "Error" with no
  modules. Voice changes are now grouped and uploads never overlap. Verified on hardware.
- **The front-panel Voices arrows now reach the synth.** They previously changed only the
  on-screen value.
- **The Patch Mutator no longer destroys pitch and cutoff values.** Oscillator frequency,
  slave detune, filter cutoff and some LFO rates were using the wrong internal range.
- **Module renaming is fully undoable** from the canvas, Inspector and slot windows.
- **The Windows build is fixed** after an MSVC-only compilation error in Editor Options.
- Background transfers no longer block parameter changes in unrelated slots.
- A background slot acknowledgement can no longer overwrite the focused slot identifier.

Special thanks to **Nocticore** for the detailed reports behind several of these fixes.

## 📦 Downloads

Download **Animatek NME 0.11.0** below for Linux, Windows or macOS. There is no need to
install 0.10.0 first.

Official SHA-256 checksums:

```text
Linux:  65528b280881dd2c29527ed5f6b06bb3192fc5c4bd4bf393cda6b789fd5025eb
macOS:  64f49045b92605fd7351545863e9c47a0f7c23ed0dedf6745ce075cea8c60256
Windows: 8670a78b5703ec31fdd4b63e903c48a4869e980c870807af3e751f1f75724c90
```

Back up your patch libraries before upgrading. Keep each patch's `.var` sidecar next to
its `.pch` file to preserve variations and mutation exclusions.

Thank you for supporting Animatek NME. 🙏

## Español

# 🎛️ Animatek NME 0.11.0: dos versiones en una sola actualización

Nunca llegué a publicar en Patreon las novedades de la versión 0.10.0, así que esta entrega
reúne todo lo añadido en **0.10.0 y 0.11.0**.

Solo necesitas descargar **0.11.0**, que ya incluye todas las mejoras de 0.10.0. Si vienes
de 0.9.0 o de una versión anterior, este post cubre todos los cambios desde entonces.

## 🎨 Nord Classic y una renovación visual completa

La versión 0.10.0 introdujo **Nord Classic**, un tema claro de grises cálidos inspirado en
el editor original del Clavia Nord Modular. Los cuerpos de los módulos, etiquetas, knobs,
valores tipo LCD y colores de los cables se tomaron del aspecto original.

Ahora todos los temas incluyen un grano procedural muy sutil sobre el lienzo, que le da una
textura más cercana al papel. El antiguo tema Classic se llama ahora **Nomad**, mientras
que el poco utilizado Frost ha desaparecido.

Los temas claros también han recibido un repaso completo. La cabecera, barra de estado,
lista de slots, Inspector, navegador de módulos, menús y diálogos de configuración siguen
la paleta elegida. Botones, resaltados, conmutadores y desplegables mantienen un aspecto
coherente en toda la aplicación.

## 🤖 Crea y edita patches con un asistente de IA

El nuevo **puente MCP** permite que asistentes locales compatibles, como Claude Code,
Claude Desktop u OpenCode, trabajen con el editor. Un asistente puede crear patches,
añadir o mover módulos, conectar cables, cambiar parámetros y abrir presets utilizando
las operaciones normales del editor.

Todos los cambios pasan por el sistema normal de deshacer, así que puedes revisar y
deshacer cada edición igual que si la hubieras realizado manualmente.

El puente está **desactivado por defecto** y no abre ningún puerto hasta que lo activas en
*Editor Options > MCP Bridge*. Solo escucha en `127.0.0.1`, muestra el comando exacto para
registrarlo en el cliente y puede excluirse completamente durante la compilación. Solo
está disponible en la aplicación independiente.

La versión 0.11.0 completa ese flujo con nuevas herramientas:

- **`save_patch`** guarda un patch y su archivo `.var` en el disco.
- **`store_to_bank`** sube un patch y lo guarda en la memoria interna del sinte cuando la
  subida ha sido confirmada.
- **`rename_module`** renombra módulos como una operación deshacible.
- **`mutate_patch`** ejecuta el Mutator como un único paso controlado y deshacible.
- Las herramientas de consulta son más ligeras para explorar tipos, módulos y conectores.

## 🎚️ Transforma un patch completo entre A y B

Un nuevo **fader Morph A/B** aparece en la cabecera del patch, entre los ocho botones de
snapshot y el botón MUT. Es un control de interpretación creado en el editor que permite
transformar suavemente el patch completo entre dos sonidos capturados.

Haz click derecho sobre el fader para definir el extremo **A** y el extremo **B**. Cada
extremo puede tomarse del sonido actual o de cualquiera de los ocho snapshots ocupados.
Cuando ambos están definidos, mueve el fader de A a B para escuchar la transformación en
tiempo real.

El editor compara las dos capturas y envía únicamente los parámetros que cambian. Los
parámetros idénticos no se tocan, se respetan los parámetros bloqueados y los valores de
los cuatro knobs de morph normales también pueden formar parte de la transición. Los
movimientos rápidos pasan por la cola de agrupación del editor, así que el sinte recibe un
flujo eficiente en vez de cambios redundantes.

El fader también se puede controlar desde el panel frontal del Nord Modular:

- Elige directamente un knob físico desde el menú **Knob** del fader. El editor utiliza
  automáticamente un grupo de morph libre como portador silencioso y crea una asignación
  real en el hardware.
- Utiliza **Learn already-assigned knob** y gira un knob del panel para reutilizar un
  control ya asignado como fuente del fader.
- Un knob asignado directamente aparece en el Inspector como **Morph A/B Fader** y se puede
  retirar desde allí o desde el menú del propio fader.

Esto permite preparar dos sonidos muy diferentes y realizar la transición desde el
hardware sin tocar el ratón. Las capturas A/B son actualmente temporales y se reinician al
cambiar de slot activo o cargar otro patch.

## 🪟 Las ventanas de slot están totalmente conectadas

Las ventanas de slot independientes reciben ahora la información en vivo del sintetizador.
Los movimientos de knobs físicos, valores de parámetros, knobs de morph, luces y medidores
se animan en la ventana del slot correcto y también en el editor principal.

Randomize y Guardar funcionan también por ventana:

- **Ctrl+R / Ctrl+Shift+R** randomiza el patch o los módulos seleccionados de esa ventana.
- **Ctrl+S / Ctrl+Shift+S** guarda o utiliza Guardar como para el slot de esa ventana.

Editar un slot ya no tiene que esperar a que termine la transferencia de otro. Está
verificado en hardware real con dos ventanas abiertas y un patch al 100% de carga DSP.

## 📂 Elige dónde abrir un patch o examínalo en local

Abrir un `.pch` ya no sobrescribe silenciosamente el slot activo del hardware. Un nuevo
selector muestra los slots A, B, C y D junto al patch cargado en cada uno.

La nueva opción **Local** abre el patch en el editor sin enviar nada al sinte. Una insignia
**LOCAL** marca claramente cualquier patch del editor que no se sepa que coincide con el
hardware. La insignia desaparece después de subir el patch o descargarlo desde el sinte.

## 🎚️ Mejor información del hardware y un flujo más cómodo

Un diagrama compacto de 18 LEDs en el Inspector reproduce la disposición de knobs del Nord
Modular físico. Los knobs asignados se iluminan y los libres permanecen apagados. El Knob
Floater utiliza el mismo lenguaje visual.

También hay muchas mejoras pequeñas para el trabajo diario:

- La carga DSP se muestra con un decimal, por ejemplo `47.6%`.
- Las sesiones largas reutilizan los índices de módulo libres en vez de agotarlos.
- Los números de paso del secuenciador vuelven a alinearse con sus LEDs.
- Los selectores verticales invertidos muestran las etiquetas correctas.
- Quick Add se abre en el monitor donde está el ratón.
- El navegador de disco incluye un filtro PCH2 para ocultar patches 2.10 antiguos.
- Editor Options se puede abrir con **Ctrl+,**.

## 📖 El manual completo está en la web

El manual de Animatek NME está disponible en español y en inglés:

**https://animatek.net/animatek-nme/manual/**

Incluye instalación, conexión MIDI, recorrido por la interfaz, edición de patches, los
cuatro slots, herramientas flotantes, formatos de archivo, atajos de teclado, resolución
de problemas y el puente MCP.

## 🩹 Correcciones importantes de seguridad y estabilidad

- **Los cambios rápidos de Voices ya no corrompen un slot.** Pulsar rápidamente lanzaba
  subidas solapadas que podían dejar el slot como un patch llamado "Error" y sin módulos.
  Los cambios se agrupan y las subidas nunca se solapan. Verificado en hardware.
- **Las flechas de Voices llegan ahora al sinte.** Antes solo cambiaban el valor en pantalla.
- **El Patch Mutator ya no destroza los valores de pitch y cutoff.** La frecuencia del
  oscilador, el detune del slave, el cutoff del filtro y algunas velocidades de LFO
  utilizaban un rango interno incorrecto.
- **Renombrar módulos es completamente deshacible** desde el lienzo, el Inspector y las
  ventanas de slot.
- **La compilación de Windows está corregida** después de un error exclusivo de MSVC en
  Editor Options.
- Las transferencias en segundo plano ya no bloquean cambios de parámetros en otros slots.
- La confirmación de un slot en segundo plano ya no puede sobrescribir el identificador del
  slot enfocado.

Gracias especialmente a **Nocticore** por los informes detallados que ayudaron a encontrar
varias de estas correcciones.

## 📦 Descargas

Descarga **Animatek NME 0.11.0** para Linux, Windows o macOS en los archivos adjuntos. No es
necesario instalar antes la versión 0.10.0.

Sumas de comprobación SHA-256 oficiales:

```text
Linux:  65528b280881dd2c29527ed5f6b06bb3192fc5c4bd4bf393cda6b789fd5025eb
macOS:  64f49045b92605fd7351545863e9c47a0f7c23ed0dedf6745ce075cea8c60256
Windows: 8670a78b5703ec31fdd4b63e903c48a4869e980c870807af3e751f1f75724c90
```

Haz una copia de seguridad de tus bibliotecas antes de actualizar. Mantén el archivo
`.var` de cada patch junto a su `.pch` para conservar las variaciones y exclusiones de
mutación.

Gracias por apoyar Animatek NME. 🙏
