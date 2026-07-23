# Patreon post — Animatek NME 0.9.0 (covers 0.8.2 + 0.9.0)

Two language versions below. Publish one, delete the other (or post both).

---

## English

# 🎛️ Animatek NME 0.9.0 — Multi-Window Editing is here

Two releases in one drop: **0.8.2** brought the biggest feature since the project
started, and **0.9.0** polishes it and fixes everything the post-release review turned
up. If you're coming from 0.8.1, this post covers both.

### 🪟 Edit two (or four) patches at once

Right-click any slot row (A–D) and that slot's patch opens in **its own window**.
Cables, modules, parameters, morph/knob/MIDI-CC assignment, renaming, undo/redo — all
fully independent, side by side with the main window's tabs, which keep working exactly
as before.

This isn't a read-only second view. Edits made in a background slot's window land on the
synth correctly **even when that slot doesn't have front-panel focus** — confirmed on
real hardware. And when you press a slot button on the rack, the matching window comes
forward and its title says "- Focused", just like the original editor highlighted the
active patch.

New in 0.9.0: each slot window can **hide its Inspector panel** to give the canvas the
full width — click the arrow strip at the left edge of the canvas, or hit `Ctrl+I`. Very
handy once you've got a window sized narrow.

### 🐢 Big patches don't lose cables anymore

This was the one that hurt. Two separate bugs, both fixed:

- **A busy rack answers slowly.** At 99–100% DSP load the synth takes its time replying
  to a patch download. The editor used to time out and silently keep whatever partial
  data had arrived — missing cables, a desynced rack, and `.pch` files saved with
  invalid connections. It now re-requests *only* the sections that didn't arrive, and
  warns you clearly if a load is still incomplete after retrying, instead of pretending
  everything is fine.
- **Linux MIDI was truncating large messages.** Any SysEx bigger than one MIDI packet —
  which a patch with 65+ cables needs routinely — was being silently cut off after the
  first chunk by JUCE's MIDI input layer. I patched the vendored JUCE library to
  reassemble chunked SysEx properly. Previously-broken patches now load with every
  single cable and module intact, verified on hardware.

Huge thanks to everyone who reported this with real patches — especially the ones who
sent me their actual `.pch` files. That's what cracked it.

### ⚡ Instant slot switching

Switching between A–D no longer re-downloads the patch when the editor already has a
matching model. And on connect, the editor now quietly fetches **every** enabled slot in
the background — so even your first switch to each one is instant. Same behaviour as the
original Nomad.

### 🎨 Morph colors everywhere

4-1 selectors, toggles, increment buttons and sliders assigned to a morph group now show
that group's color, the way knobs always did.

### 🔈 Legacy 2.10 patches finally make sound

Old Nord Modular 2.10 files store their output routing differently, so importing them
was sending every legacy patch to outputs 3/4 instead of 1/2 — they looked fine and
played silence. Fixed: **all 857 known factory patches** now import with correct routing.

### Also fixed

- Preset browser sometimes only showed the first few banks after connecting — it now
  resumes the interrupted name fetch automatically.
- Opening a `.pch` while viewing an unfocused slot could upload it to the wrong physical
  slot.
- A queued batch of parameter changes for one slot (e.g. a Mutator audition in a
  background window) could be wiped by an unrelated fetch on another slot.
- Morph keyboard (velocity/note) assignment could target the wrong slot — and importing
  a 2.10 patch that had one set could corrupt it.
- Console log now marks each patch load with a clear boundary line, so bug reports are
  much easier to read.
- **Windows:** the MSVC runtime is statically linked now — no more Visual C++
  Redistributable needed on a fresh machine.

---

**Downloads below** for Linux, Windows and macOS. Back up your patch libraries before
upgrading, and keep each patch's `.var` sidecar next to its `.pch` so variations and
mutation exclusions survive.

Thanks for supporting Animatek NME. 🙏

---

## Español

# 🎛️ Animatek NME 0.9.0 — Edición multiventana

Dos versiones en una entrega: **0.8.2** trajo la función más grande desde que empezó el
proyecto, y **0.9.0** la pule y corrige todo lo que apareció en la revisión posterior. Si
vienes de 0.8.1, este post cubre las dos.

### 🪟 Edita dos (o cuatro) patches a la vez

Click derecho en cualquier slot (A–D) y ese patch se abre en **su propia ventana**.
Cables, módulos, parámetros, asignación de morph/knob/MIDI-CC, renombrado,
deshacer/rehacer — todo independiente, junto a las pestañas de la ventana principal, que
siguen funcionando igual que siempre.

No es una vista de solo lectura: las ediciones hechas en la ventana de un slot en segundo
plano llegan bien al sintetizador **aunque ese slot no tenga el foco del panel frontal** —
confirmado en hardware real. Y cuando pulsas un botón de slot en el rack, la ventana
correspondiente pasa al frente y su título marca "- Focused", igual que el editor
original resaltaba el patch activo.

Nuevo en 0.9.0: cada ventana de slot puede **ocultar su panel Inspector** para dar al
lienzo todo el ancho — click en la tira de flecha del borde izquierdo, o `Ctrl+I`. Muy
útil con ventanas estrechas.

### 🐢 Los patches grandes ya no pierden cables

Este dolía. Dos bugs distintos, los dos corregidos:

- **Un rack ocupado responde lento.** Al 99–100% de carga de DSP el sintetizador tarda en
  contestar a una descarga de patch. El editor hacía timeout y se quedaba en silencio con
  los datos parciales que hubieran llegado — cables perdidos, rack desincronizado y
  archivos `.pch` guardados con conexiones inválidas. Ahora vuelve a pedir *solo* las
  secciones que faltan, y avisa claramente si la carga sigue incompleta tras reintentar,
  en vez de fingir que todo va bien.
- **El MIDI en Linux truncaba mensajes grandes.** Cualquier SysEx mayor que un paquete
  MIDI — algo habitual en un patch con más de 65 cables — se cortaba en silencio tras el
  primer trozo, dentro de la capa de entrada MIDI de JUCE. He parcheado la librería JUCE
  incluida para reensamblar el SysEx correctamente. Patches que antes se rompían ahora
  cargan con todos los cables y módulos intactos, verificado en hardware.

Gracias enormes a quienes reportaron esto con patches reales, y en especial a los que me
enviaron sus `.pch`. Eso fue lo que lo destapó.

### ⚡ Cambio de slot instantáneo

Cambiar entre A–D ya no vuelve a descargar el patch si el editor ya tiene un modelo que
coincide. Y al conectar, ahora descarga **todos** los slots activos en segundo plano —
así el primer cambio a cualquiera de ellos también es instantáneo. Igual que el Nomad
original.

### 🎨 Colores de morph en todos los controles

Selectores 4-1, interruptores, botones de incremento y sliders asignados a un grupo de
morph ahora muestran su color, como ya hacían los knobs.

### 🔈 Los patches 2.10 antiguos por fin suenan

Los archivos Nord Modular 2.10 guardan el ruteo de salida de otra forma, así que al
importarlos todo iba a las salidas 3/4 en vez de 1/2 — se veían bien y sonaban a nada.
Corregido: **los 857 patches de fábrica conocidos** importan ahora con el ruteo correcto.

### También corregido

- El explorador de presets a veces solo mostraba los primeros bancos al conectar — ahora
  reanuda automáticamente la lectura de nombres interrumpida.
- Abrir un `.pch` viendo un slot sin foco podía subirlo al slot físico equivocado.
- Un lote de cambios de parámetros en cola para un slot (p. ej. una audición del Mutator
  en una ventana de fondo) podía borrarse por una descarga de otro slot.
- La asignación de teclado de morph (velocidad/nota) podía ir al slot equivocado — y al
  importar un patch 2.10 que la tuviera puesta, podía corromperse.
- El log de consola marca cada carga de patch con una línea separadora clara, para
  reportes de bugs mucho más legibles.
- **Windows:** el runtime de MSVC va ahora enlazado estáticamente — ya no hace falta el
  Visual C++ Redistributable en una máquina limpia.

---

**Descargas abajo** para Linux, Windows y macOS. Haz copia de tus librerías de patches
antes de actualizar, y mantén el `.var` de cada patch junto a su `.pch` para conservar
variaciones y exclusiones de mutación.

Gracias por apoyar Animatek NME. 🙏
