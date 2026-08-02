# Patreon post: Animatek NME 0.12.0 + 0.13.0

Two language versions are included below. Publish one or both.

## English

# 🎛️ Animatek NME 0.13.0: four slots, one window

I never published the Patreon post for version 0.12.0, so this release catches up
with everything added in both **0.12.0 and 0.13.0**.

You only need to download **0.13.0**. It already includes every improvement from
0.12.0. If you are coming from 0.11.0 or earlier, this post covers everything
that has changed since then.

## 🪟 The four slots now live inside the main window

This is the biggest structural change since the editor was renamed.

The per-slot pop-out windows are gone. They got lost behind other applications,
could not be arranged, and meant every feature touching the canvas had to be
written twice. There is one canvas now, wired once per slot, and the four slots
are **sub-windows inside the main window**, tiled the way the original Clavia
editor and Nomad arranged patches.

Open slots **tile themselves**. One fills the work area, two split it down the
middle, three go in thirds, four go 2x2, and the layout re-flows as you open and
close them. Nothing to arrange unless you want to: drag or resize a sub-window
and the windows stay where you put them from then on, with **View > Slots > Tile
Slots** to re-flow.

- **Ctrl+Shift+1..4** shows or hides a slot's sub-window, and so does
  right-clicking its row in the slot bar.
- **Ctrl+1..4** switches to a slot, opening it if it was closed.
- **F11**, or a sub-window's maximise button, blows the focused slot up to the
  whole area and back again for a closer look.
- **Ctrl+Shift+arrow** moves the focused slot to the neighbouring tile, so the
  patch you are working on goes where you want it without closing and reopening
  anything. **View > Slots > Rotate Slots** shifts them all round at once.

The sub-windows slide to their new places rather than jumping. Turn that off with
**Animate Slot Tiling** in Editor Options if you prefer it instant.

The Inspector, header bar, browsers and status bar stay shared and follow
whichever slot has focus, and the focused sub-window is edged in the theme's text
colour so you can always see which one you are editing. Each canvas keeps its own
selection while it sits in the background, so the Inspector picks up where that
slot left off instead of going blank. Dragging a module out of the browser now
works into any sub-window, which never worked in the pop-outs.

Which slots you had open, which one had focus and how they were arranged all come
back when you reopen the editor. Connecting to the Nord then lines the work area
up with the slots the synth actually has enabled, once; after that the windows
are yours alone, and pressing slot buttons on the front panel moves focus without
ever closing one.

## 📊 What every module costs, everywhere you choose one

The Nord Modular's DSP budget is the constraint that shapes every patch, and
until 0.12.0 the editor gave you one number for the whole thing. Each module's
share now appears wherever a module is picked or inspected: in the right-click
**Add Module** menu ("Audio In (2.2%)"), in the module browser, on every Quick
Add row, and in the Inspector for the selected module.

The figures were rebuilt from Clavia's own numbers and are rounded to the two
significant figures the original editor prints, so a patch optimised against the
hardware editor reads the same here. The inherited table had 47 of 109 values
outside that rounding interval, nearly all too high, which is why a full patch
used to read just over 100%.

**Double-click a module** for its own cost, as the original editor does, or press
**F10** to label every module at once, which is the view you want when a patch is
over budget and you are looking for what to cut.

## 🔍 Values on hover, and the function-key readouts

Rest the cursor on a knob, slider, button or display box and its value appears,
in the parameter's own units, so a cutoff reads "440 Hz" rather than "64". Drag a
control and the value follows live with no delay, which is the moment you
actually want the number.

**F5** reads out the whole patch at once. It existed before but only drew
parameters assigned to a morph group, and showed the raw morph range instead of a
value, so what should have been a patch readout was in practice a morph
inspector. A morphed parameter now reads out the span the morph sweeps it across,
matching the original's "46Hz-2.30kHz".

**F7**, **F8** and **F9** complete the set the original editor documents: morph
groups, knob assignments and MIDI CC assignments, each labelled over the
parameter it belongs to. All of them toggle, so you can leave a readout open
while you work rather than holding a key down.

## 🎛️ Module presets became a library

Presets used to be a DrumSynth feature hidden behind a right-click on a preset
display box 57 pixels wide, and even there you could delete a preset but not
recall one.

Selecting any module now puts a **Presets** section in the Inspector, under its
assignments: click a name to recall, the x to delete, right-click to rename, and
**Save current settings** to capture the module as it stands. The section folds
away from a chevron in its title, and the same list is on the module's own
right-click menu. Recalling a preset is a single undo step rather than one per
parameter.

Underneath, a preset is now any module's named parameter snapshot rather than a
DrumSynth structure, so the sequencers or anything else can have presets without
new code. They live in a **Presets** folder in your patch library, next to
Patches, Snippets and Banks, as one `.pchp` pack per module type. The format is
plain text and meant to be edited by hand. Values are keyed by parameter rather
than by position, so a preset that names two parameters sets those two and leaves
the rest of the module alone. Presets saved by earlier versions are migrated on
first run.

## 📥 Load a synth patch into a slot you name

Double-clicking a patch in the Synth browser still loads it into the slot you are
on. Right-clicking now offers **Load to Slot A..D**, so with several sub-windows
open you can pull a patch into a particular one without leaving the slot you are
working in first.

## 🍎 macOS runs on High Sierra again

The macOS package claimed Catalina as its minimum, which left older Intel Macs
out for no technical reason. Release builds now target macOS 10.13, and packaging
verifies both the bundle's minimum version and the Intel binary's own so the
metadata cannot drift back.

## 🔠 Bigger text across the application

Panels, browsers, the Inspector, the header bar, the status bar and every dialog
had their text sizes chosen by hand, file by file, and had drifted small next to
the rest of the desktop. They now share one scale, which lifts them together and
keeps their relative weights intact. Module bodies on the canvas are deliberately
untouched: their text sits inside a fixed grid that comes from the hardware.

## 🩹 Fixes

- **The sequencers' arrow buttons step both ways again (issue #34).** The four
  sequencer modules draw their arrows as a left/right pair, but the click was
  split top against bottom, so the direction depended on which half of an arrow
  you hit. Only the sequencers were affected.
- **Clr clears the steps, not the sequencer (issue #34).** It reset every
  parameter to its minimum, taking the step count down to 1 along with the loop
  and transport settings. It now empties only the per-step values.
- **The Filter Bank's jacks and bypass are back inside the module (issue #35).**
  Its artwork was drawn taller than the module is, so the bottom row of controls
  fell onto the canvas behind it. A sweep of all 110 modules found this was the
  only one affected.
- **Ctrl+I works in the main window (issue #38).** It now collapses the left
  Inspector column, Ctrl+Shift+I collapses the right patch browser, and each
  panel remembers the width it was dragged to.
- **Replacing a patch in a slot no longer crashes the editor.** Creating an empty
  patch or loading a file into a slot holding a patch with knob or MIDI CC
  assignments killed the app.
- **The Load meter no longer shows the previous patch's cost** after an edit that
  did not come from the canvas. Adding modules over the MCP bridge read 0.0%.
- **The macro captions are readable on light themes.** M1 to M4 above the morph
  dials, and the Macro 1..4 headers in the Inspector, were painted in their own
  macro colour, which made green M2 unreadable on Nord Classic. They now use the
  theme's text colour; the dials and colour stripes still carry the colour.
- **"Press Enter to add modules" could pile up on itself** on an empty canvas.
  The hint was centred on whatever rectangle was being repainted rather than on
  the canvas.
- **Removed the "Recycle Windows" editor option**, which never did anything.

## 📖 The manual is up to date

The manual has been rewritten for the new workspace, in English and Spanish:

**https://animatek.net/animatek-nme/manual/**

## 📦 Downloads

Download **Animatek NME 0.13.0** below for Linux, Windows or macOS. There is no
need to install 0.12.0 first.

Official SHA-256 checksums:

```text
Linux:   788af109944ad09097e2f3df0445da992a230b30bb8c3da3524484e0e02b9502
macOS:   4dee0e94d9e76444e4d2ba2f01748ceb6c5f627e571fad59a3e109eca1d3327c
Windows: bf766a904390829bd35006b9c73803537d2590309e765ba8eafb4add29d8043a
```

Still beta. Back up any patch you care about before using this with your Nord
Modular, and keep each patch's `.var` sidecar next to its `.pch` file to preserve
variations and mutation exclusions.

Known limitations: stuck MIDI-IN notes are cleared with the synth's front-panel
panic; macOS builds are unsigned, so Gatekeeper needs the usual right-click →
Open on first launch; and F12 (current MIDI controller values) from the original
editor is not implemented yet.

Thank you for supporting Animatek NME. 🙏

## Español

# 🎛️ Animatek NME 0.13.0: cuatro slots, una ventana

Nunca llegué a publicar en Patreon las novedades de la versión 0.12.0, así que
esta entrega reúne todo lo añadido en **0.12.0 y 0.13.0**.

Solo necesitas descargar **0.13.0**, que ya incluye todas las mejoras de 0.12.0.
Si vienes de 0.11.0 o de una versión anterior, este post cubre todos los cambios
desde entonces.

## 🪟 Los cuatro slots viven ya dentro de la ventana principal

Es el mayor cambio estructural desde que el editor cambió de nombre.

Las ventanas independientes por slot han desaparecido. Se perdían detrás de otras
aplicaciones, no se podían organizar, y obligaban a escribir dos veces cada
función que tocara el canvas. Ahora hay un solo canvas, conectado una vez por
slot, y los cuatro slots son **subventanas dentro de la ventana principal**,
distribuidas en mosaico como colocaban los patches el editor original de Clavia y
Nomad.

Los slots abiertos **se reparten solos** el espacio. Uno ocupa toda el área de
trabajo, dos la parten por la mitad, tres van en tercios, cuatro en 2x2, y la
distribución se rehace al abrir y cerrar. No hay nada que colocar salvo que
quieras: arrastra o redimensiona una subventana y las ventanas se quedan donde
las pongas a partir de ese momento, con **View > Slots > Tile Slots** para volver
al mosaico.

- **Ctrl+Shift+1..4** muestra u oculta la subventana de un slot, igual que el
  clic derecho en su fila de la barra de slots.
- **Ctrl+1..4** cambia a un slot, abriéndolo si estaba cerrado.
- **F11**, o el botón de maximizar de una subventana, agranda el slot con foco a
  toda el área y lo devuelve, para mirarlo de cerca.
- **Ctrl+Shift+flecha** mueve el slot con foco a la casilla vecina, así el patch
  en el que trabajas va donde tú quieres sin cerrar y reabrir nada. **View >
  Slots > Rotate Slots** los desplaza todos a la vez.

Las subventanas se deslizan hasta su nuevo sitio en vez de saltar. Desactívalo con
**Animate Slot Tiling** en Editor Options si lo prefieres instantáneo.

El Inspector, la cabecera, los navegadores y la barra de estado siguen siendo
compartidos y acompañan al slot que tenga el foco, y la subventana enfocada lleva
un borde del color de texto del tema para que siempre veas cuál estás editando.
Cada canvas conserva su propia selección mientras está en segundo plano, así que
el Inspector retoma donde ese slot lo dejó en vez de quedarse en blanco. Arrastrar
un módulo desde el navegador funciona ya a cualquier subventana, algo que nunca
funcionó con las ventanas independientes.

Qué slots tenías abiertos, cuál tenía el foco y cómo estaban colocados vuelven al
reabrir el editor. Al conectar con el Nord, el área de trabajo se alinea una vez
con los slots que el sintetizador tiene activados; a partir de ahí las ventanas
son solo tuyas, y pulsar los botones de slot del panel frontal mueve el foco sin
cerrar ninguna.

## 📊 Lo que cuesta cada módulo, allí donde eliges uno

El presupuesto de DSP del Nord Modular es la restricción que da forma a cada
patch, y hasta la 0.12.0 el editor te daba un único número para el conjunto.
Ahora la parte de cada módulo aparece allí donde se elige o se inspecciona un
módulo: en el menú contextual **Add Module** ("Audio In (2.2%)"), en el navegador
de módulos, en cada fila de Quick Add y en el Inspector para el módulo
seleccionado.

Las cifras se han reconstruido a partir de los números de la propia Clavia y están
redondeadas a las dos cifras significativas que imprime el editor original, así
que un patch optimizado contra el editor de hardware se lee igual aquí. La tabla
heredada tenía 47 de 109 valores fuera de ese intervalo de redondeo, casi todos
por exceso, y por eso un patch lleno marcaba algo más del 100%.

**Doble clic en un módulo** para ver su coste, como hace el editor original, o
pulsa **F10** para etiquetarlos todos a la vez, que es la vista que quieres
cuando un patch se pasa de presupuesto y buscas qué recortar.

## 🔍 Valores al pasar el cursor, y las lecturas con teclas de función

Deja el cursor sobre un knob, un slider, un botón o un display y aparece su valor,
en las unidades del propio parámetro, de forma que un cutoff se lee "440 Hz" y no
"64". Arrastra un control y el valor sigue el movimiento en vivo, sin retardo, que
es justo el momento en el que quieres el número.

**F5** lee el patch entero de una vez. Ya existía, pero solo dibujaba los
parámetros asignados a un grupo de morph, y mostraba el rango de morph en bruto en
lugar de un valor, así que lo que debía ser una lectura del patch era en la
práctica un inspector de morphs. Un parámetro con morph muestra ahora el recorrido
que barre el morph, igual que el "46Hz-2.30kHz" del original.

**F7**, **F8** y **F9** completan el juego que documenta el editor original:
grupos de morph, asignaciones de knobs y asignaciones de MIDI CC, cada una
etiquetada sobre el parámetro al que pertenece. Todas son conmutadores, así que
puedes dejar una lectura abierta mientras trabajas en vez de mantener la tecla
pulsada.

## 🎛️ Los presets de módulo son ya una librería

Los presets eran una función del DrumSynth escondida tras un clic derecho en un
display de 57 píxeles de ancho, y ni siquiera ahí podías recuperar uno: solo
borrarlo.

Ahora, al seleccionar cualquier módulo, el Inspector añade una sección **Presets**
bajo sus asignaciones: clic en un nombre para recuperarlo, la x para borrarlo,
clic derecho para renombrar, y **Save current settings** para capturar el módulo
tal como está. La sección se pliega desde el chevron de su título, y la misma
lista está en el menú contextual del propio módulo. Recuperar un preset es un
único paso de deshacer, no uno por parámetro.

Por debajo, un preset es ya una instantánea con nombre de los parámetros de
cualquier módulo, y no una estructura del DrumSynth, así que los secuenciadores o
lo que sea pueden tener presets sin código nuevo. Viven en una carpeta **Presets**
de tu librería, junto a Patches, Snippets y Banks, como un paquete `.pchp` por
tipo de módulo. El formato es texto plano y está pensado para editarse a mano. Los
valores se identifican por el nombre del parámetro y no por su posición, así que
un preset que nombra dos parámetros ajusta esos dos y deja el resto del módulo
como estaba. Los presets guardados por versiones anteriores se migran en el primer
arranque.

## 📥 Carga un patch del sinte en el slot que tú digas

El doble clic en un patch del navegador del sintetizador sigue cargándolo en el
slot en el que estás. El clic derecho ofrece ahora **Load to Slot A..D**, de modo
que con varias subventanas abiertas puedes llevar un patch a una concreta sin
salir antes del slot en el que trabajas.

## 🍎 macOS vuelve a funcionar en High Sierra

El paquete de macOS declaraba Catalina como mínimo, lo que dejaba fuera a los Mac
Intel más antiguos sin ninguna razón técnica. Las builds de release apuntan ahora
a macOS 10.13, y el empaquetado verifica tanto la versión mínima del bundle como
la del propio binario Intel para que los metadatos no se desvíen.

## 🔠 Texto más grande en toda la aplicación

Los paneles, los navegadores, el Inspector, la cabecera, la barra de estado y
todos los diálogos tenían los tamaños de texto elegidos a mano, archivo por
archivo, y se habían quedado pequeños al lado del resto del escritorio. Ahora
comparten una sola escala, que los sube a la vez y mantiene sus pesos relativos.
Los cuerpos de los módulos en el canvas se quedan como estaban a propósito: su
texto vive dentro de una rejilla fija que viene del hardware.

## 🩹 Correcciones

- **Las flechas de los secuenciadores vuelven a avanzar en ambos sentidos (issue
  #34).** Los cuatro módulos de secuenciador dibujan sus flechas como un par
  izquierda/derecha, pero el clic se repartía arriba contra abajo, así que la
  dirección dependía de en qué mitad de la flecha dieras. Solo afectaba a los
  secuenciadores.
- **Clr limpia los pasos, no el secuenciador (issue #34).** Reiniciaba todos los
  parámetros a su mínimo, dejando el número de pasos en 1 junto con los ajustes de
  loop y transporte. Ahora vacía solo los valores de cada paso.
- **Los jacks y el bypass del Filter Bank vuelven a estar dentro del módulo (issue
  #35).** Su gráfico se dibujaba más alto que el módulo, así que la fila inferior
  de controles caía sobre el canvas de detrás. Un repaso de los 110 módulos
  confirmó que era el único afectado.
- **Ctrl+I funciona en la ventana principal (issue #38).** Ahora pliega la columna
  izquierda del Inspector, Ctrl+Shift+I pliega el navegador de patches de la
  derecha, y cada panel recuerda el ancho al que lo dejaste.
- **Reemplazar un patch en un slot ya no cierra el editor.** Crear un patch vacío
  o cargar un archivo en un slot que tenía un patch con asignaciones de knob o
  MIDI CC mataba la aplicación.
- **El medidor Load ya no muestra el coste del patch anterior** tras una edición
  que no viniera del canvas. Añadir módulos por el puente MCP marcaba 0.0%.
- **Los rótulos de macro se leen en los temas claros.** M1 a M4 sobre los diales
  de morph, y las cabeceras Macro 1..4 del Inspector, se pintaban con su propio
  color de macro, lo que hacía ilegible el M2 verde sobre Nord Classic. Ahora usan
  el color de texto del tema; los diales y las franjas de color siguen llevando el
  color.
- **"Press Enter to add modules" podía acumularse sobre sí mismo** en un canvas
  vacío. El aviso se centraba sobre el rectángulo que se estuviera repintando en
  vez de sobre el canvas.
- **Retirada la opción "Recycle Windows"** de Editor Options, que nunca hizo nada.

## 📖 El manual está al día

El manual se ha reescrito para el nuevo espacio de trabajo, en español y en
inglés:

**https://animatek.net/animatek-nme/manual/**

## 📦 Descargas

Descarga **Animatek NME 0.13.0** para Linux, Windows o macOS en los archivos
adjuntos. No es necesario instalar antes la versión 0.12.0.

Sumas de comprobación SHA-256 oficiales:

```text
Linux:   788af109944ad09097e2f3df0445da992a230b30bb8c3da3524484e0e02b9502
macOS:   4dee0e94d9e76444e4d2ba2f01748ceb6c5f627e571fad59a3e109eca1d3327c
Windows: bf766a904390829bd35006b9c73803537d2590309e765ba8eafb4add29d8043a
```

Sigue siendo beta. Haz una copia de seguridad de los patches que te importen antes
de usar esto con tu Nord Modular, y mantén el archivo `.var` de cada patch junto a
su `.pch` para conservar las variaciones y las exclusiones de mutación.

Limitaciones conocidas: las notas de MIDI-IN colgadas se limpian con el panic del
panel frontal; las builds de macOS no están firmadas, así que Gatekeeper necesita
el clic derecho → Abrir la primera vez; y F12 (valores actuales de los
controladores MIDI) del editor original todavía no está implementado.

Gracias por apoyar Animatek NME. 🙏
