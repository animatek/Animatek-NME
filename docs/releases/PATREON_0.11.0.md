# Patreon post: Animatek NME 0.11.0

Two language versions below. Publish one, delete the other (or post both).

---

## English

# 🎛️ Animatek NME 0.11.0: slot windows come alive, and a manual to go with them

Multi-window polish, a safety fix worth upgrading for on its own, and the editor finally
has a proper manual.

### 🪟 Slot windows now follow the synth live

A popped-out slot window used to be edit-only. Turning a physical knob on the front panel,
or watching a light or meter move, animated the main window but never the slot window, even
when that window was showing the very slot you were touching. Now the live stream from the
synth reaches it too: parameter values, morph knobs, lights and meters all animate exactly
like the main canvas.

Two commands also work per-window now. **Ctrl+R / Ctrl+Shift+R** (Randomize, uniform or
Gaussian) and **Ctrl+S / Ctrl+Shift+S** (Save / Save As) act on *that window's* slot and
honour its own module selection, so you can randomize one patch without touching the other
three.

### 📂 Choosing where a patch goes, and a way to just look

Opening a `.pch` used to silently target the active slot and, when connected, always upload.
There was no way to browse a patch without overwriting what your rack was playing.

Opening a patch now asks. The chooser lists A/B/C/D with the patch currently in each,
defaults to the active slot, and adds a **Local** option that loads into the editor only and
sends nothing to the synth. Any slot whose patch isn't known to match the hardware, loaded
Local or built while disconnected, carries a **LOCAL** badge until it's uploaded or fetched.

### 🤖 The MCP bridge can now finish the job

Building a patch through the bridge used to leave it stranded in memory. Three new tools
close the loop: **`save_patch`** writes a slot's patch and its `.var` to disk,
**`store_to_bank`** uploads it and writes it into the synth's internal memory once the
upload is acknowledged, and **`rename_module`** renames a module as an undoable step.

There's also **`mutate_patch`**, which runs the editor's own Mutator as a single undoable,
throttled operation instead of a burst of parameter writes, and the browsing tools are much
lighter for assistants that aren't sitting on the source tree.

### 📖 New: the manual is online

The full user manual now lives at **animatek.net/animatek-nme/manual/**, in Spanish and
English: installing and connecting, a tour of the interface, editing patches, the four
slots, the floating tools, file formats, the complete shortcut reference, troubleshooting,
and a chapter on the MCP bridge.

It's generated straight from the manual in the repository, so it stays in step with the
editor instead of drifting a few versions behind.

### 🩹 Fixes

- **Rapid Voices changes no longer corrupt the slot (issue #28).** Every voice change
  re-uploads the whole patch, and tapping the arrows quickly fired overlapping uploads that
  interleaved on the wire, leaving the slot reading back as a patch named "Error" with no
  modules. Uploads are now debounced and never overlap. Verified on hardware.
- **The front-panel Voices arrows actually reach the synth (issue #25).** They moved the
  on-screen number but sent nothing, so the voice count only really changed through Ctrl+P.
- **The Patch Mutator no longer wrecks pitch and cutoff.** Mutate and Randomize slammed
  oscillator frequency, slave detune, filter cutoff and some LFO rates to almost nothing,
  because those parameters were read against the wrong internal range. Present since the
  Mutator shipped in 0.7.0; Interpolate and Cross were unaffected.
- **Renaming a module is undoable (issue #23)**, from the canvas menu, the Inspector or a
  slot window. It used to change the title outside the undo system.
- **The Windows build is fixed (issue #24)**, an MSVC-only parse error that left Linux and
  macOS building fine.

Special thanks to **Nocticore** for the reports behind several of these.

---

*Grab 0.11.0 from the downloads below. Back up your patch libraries before upgrading, and
keep each patch's `.var` sidecar next to its `.pch` to preserve variations and mutation
exclusions.*

Thanks for supporting Animatek NME.

---

## Español

# 🎛️ Animatek NME 0.11.0: las ventanas de slot cobran vida, y por fin hay manual

Repaso del trabajo multiventana, un fallo corregido que ya justifica actualizar por sí
solo, y el editor tiene por fin un manual como es debido.

### 🪟 Las ventanas de slot siguen al sinte en vivo

Una ventana de slot solo servía para editar. Girar un knob físico en el panel frontal, o el
movimiento de una luz o un medidor, animaba la ventana principal pero nunca la del slot,
aunque esa ventana estuviera mostrando justo el slot que estabas tocando. Ahora la señal en
vivo del sinte también le llega: valores de parámetro, knobs de morph, luces y medidores se
animan igual que en el lienzo principal.

Además, dos comandos funcionan ya por ventana. **Ctrl+R / Ctrl+Shift+R** (Randomize,
uniforme o gaussiano) y **Ctrl+S / Ctrl+Shift+S** (Guardar / Guardar como) actúan sobre el
slot *de esa ventana* y respetan su propia selección de módulos, así que puedes randomizar
un patch sin tocar los otros tres.

### 📂 Elegir dónde va un patch, y poder solo mirar

Abrir un `.pch` apuntaba en silencio al slot activo y, con el sinte conectado, siempre
subía. No había manera de curiosear un patch sin machacar lo que el rack estaba tocando.

Ahora abrir un patch pregunta. El selector lista A/B/C/D con el patch que hay en cada uno,
propone el slot activo, y añade una opción **Local** que carga solo en el editor y no envía
nada al sinte. Cualquier slot cuyo patch no se sepa que coincide con el hardware, cargado
como Local o construido sin conexión, lleva una insignia **LOCAL** hasta que se sube o se
descarga.

### 🤖 El puente MCP ya puede terminar el trabajo

Un patch construido desde el puente se quedaba varado en memoria. Tres herramientas nuevas
cierran el círculo: **`save_patch`** escribe el patch de un slot y su `.var` en disco,
**`store_to_bank`** lo sube y lo guarda en la memoria interna del sinte en cuanto la subida
se confirma, y **`rename_module`** renombra un módulo como paso deshacible.

También está **`mutate_patch`**, que ejecuta el propio Mutator del editor como una sola
operación deshacible y regulada en vez de una ráfaga de escrituras, y las herramientas de
consulta son mucho más ligeras para asistentes que no están sobre el código fuente.

### 📖 Novedad: el manual está en la web

El manual de usuario completo está ya en **animatek.net/animatek-nme/manual/**, en español
y en inglés: instalación y conexión, un recorrido por la interfaz, edición de patches, los
cuatro slots, las herramientas flotantes, los formatos de archivo, la referencia completa de
atajos, resolución de problemas, y un capítulo sobre el puente MCP.

Se genera directamente desde el manual del repositorio, así que se mantiene al día con el
editor en vez de quedarse varias versiones atrás.

### 🩹 Correcciones

- **Los cambios rápidos de voces ya no corrompen el slot (issue #28).** Cada cambio de voces
  vuelve a subir el patch entero, y pulsar las flechas deprisa lanzaba subidas solapadas que
  se entrelazaban en el cable, dejando el slot como un patch llamado "Error" y sin módulos.
  Las subidas ahora se agrupan y nunca se solapan. Verificado en hardware.
- **Las flechas de voces llegan de verdad al sinte (issue #25).** Movían el número en
  pantalla pero no enviaban nada, así que el número de voces solo cambiaba de verdad desde
  Ctrl+P.
- **El Patch Mutator ya no destroza pitch y cutoff.** Mutate y Randomize hundían la
  frecuencia del oscilador, el detune del slave, el cutoff del filtro y algunas velocidades
  de LFO, porque esos parámetros se leían contra el rango interno equivocado. Estaba desde
  que salió el Mutator en 0.7.0; Interpolate y Cross no se veían afectados.
- **Renombrar un módulo es deshacible (issue #23)**, desde el menú del lienzo, el Inspector
  o una ventana de slot. Antes cambiaba el título fuera del sistema de undo.
- **La build de Windows está arreglada (issue #24)**, un error de compilación solo de MSVC
  mientras Linux y macOS compilaban bien.

Gracias en especial a **Nocticore** por los informes detrás de varias de estas.

---

*Descarga la 0.11.0 desde los archivos de abajo. Haz copia de seguridad de tus bibliotecas
antes de actualizar, y mantén el `.var` de cada patch junto a su `.pch` para conservar
variaciones y exclusiones de mutación.*

Gracias por apoyar Animatek NME.
