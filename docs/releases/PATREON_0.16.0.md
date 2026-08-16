# Patreon post: Animatek NME 0.16.0

Two language versions are included below. Publish one or both.

**Screenshots to take before posting.** Marked in both versions with
`📸 CAPTURA` at the point in the post where the image goes. They are the same
seven shots in each language, so take them once. Only the new features are
marked: the fixes go out as text.

| # | Shot | Where |
|---|------|-------|
| 1 | The Inspector's Parameters list, with a value being typed in | Select a module with a lot of controls (OscA, DrumSynth) and double-click a value |
| 2 | The nudge arrows under a hovered knob | Rest the pointer on a knob; take it with the arrows lit |
| 3 | A patch with two or three text notes labelling sections of it | Any patch of yours worth showing |
| 4 | The store button next to the patch name, showing a bank location | Load a patch from a bank; the button shows where it came from |
| 5 | The Inspector with its sections folded and unfolded | Two shots side by side, or one with a couple folded |
| 6 | The DSP cost overlay on a busy patch (`F3`) | A patch near its budget reads best |
| 7 | Editor Options, showing the "ask which slot" setting | Optional; only if the post feels short on pictures |

---

## English

# 🎛️ Animatek NME 0.16.0: the one that went inwards

Most of what changed here is not something you can point at on screen, which is
an odd thing to say about a release. **Big patches draw far faster**, a whole
family of crashes cannot come back, and the project has tests and a build robot
for the first time. Nine issues are closed, and five more faults were found by a
test pass written specifically to go hunting for them.

There is plenty you *can* point at too, so let us start there.

## 🎛️ The Inspector lists every parameter, and lets you type

A knob is a few pixels holding 128 steps: fine for sweeping, useless for saying
what a value actually **is**. Selecting a module now fills a **Parameters**
section at the top of the Inspector with every control it has, each as its name
and the figure it reads in its own units.

Drag a value to walk it, or **double-click it and type one in**. Typing accepts
the reading as it is written, so `440Hz`, `C#3` and `-12(Oct)` all land where you
mean them to, and a bare number picks the nearest step. Whatever the module wears
as a **button** is a button here too, carrying the same lettering its face does:
a two-state switch flips and lights up, and a selector like the DrumSynth's
LP/BP/HP walks round its options.

Every edit goes to the synth as it happens and undoes in one step.

📸 **CAPTURA 1** — La lista de parámetros del Inspector, con un valor
escribiéndose. Elige un módulo con muchos controles (OscA, DrumSynth) y haz
doble clic en un valor para que se vea el campo de texto abierto.

## ↕️ Nudge arrows, and `+` / `-`

Landing on an exact frequency or MIDI note by dragging a knob is guesswork.
Resting the pointer on a knob or a slider now pops the same two little buttons
the original editor shows: the left one steps down, the right one up, and holding
either repeats. The value reads out while you step it.

`+` and `-` do the same to whatever the pointer is over, so a NoteDetect can be
walked to exactly the note you want without touching the mouse. Held down, the
key repeats, and the whole run is a single undo step. The four morph dials in the
header bar have them too.

📸 **CAPTURA 2** — Las flechitas bajo un knob con el puntero encima, con una de
ellas iluminada y el valor leyéndose al lado.

## 📝 Comments on the patch

A text note you can drop into any empty space, from the module bar's **ANME**
group. It takes its rectangle of the grid the way a module does, so modules make
room for it and it makes room for them, and you can pull its bottom corners to
resize it.

Notes are part of a selection like anything else: they copy, paste, move and
delete together with the modules around them, so a labelled section of a patch
travels as one piece. They never reach the synth, and they survive a trip
through it: load the patch back from the Nord and the notes come back with it.

📸 **CAPTURA 3** — Un patch tuyo con dos o tres notas rotulando secciones. Esta
es la que mejor se entiende de un vistazo, así que merece un patch bonito.

## 💾 Where a patch came from, and one click to put it back

A **store button** sits next to the patch name showing the bank location the
patch on screen came from, so putting it back where it belongs is one click and
no dialog. When the editor does not know the location — a new patch, or one
opened from a file — it falls back to asking.

The Store Patch to Bank dialog also opens on the patch's own location instead of
bank 1, and Save Patch As opens with the filename already typed in.

📸 **CAPTURA 4** — El botón de guardar junto al nombre del patch, mostrando una
ubicación de banco. Carga un patch de un banco para que salga la posición.

## 🗂️ The Inspector folds away

Parameters, Morphs, Knobs and MIDI CC each grew the chevron the Presets section
already had, so a module with thirty parameters and no assignments does not push
its presets off the bottom of the column. The choice is remembered between runs
and shared by every Inspector on screen.

Knobs assigned to the morph groups are listed there now as well, and the Knob
Floater keeps up with the morph dials.

📸 **CAPTURA 5** — El Inspector con algunas secciones plegadas y otras abiertas,
para que se vea el mecanismo.

## ⚡ Big patches draw far faster

Every time the canvas painted a module, it worked out — from scratch, and twice
over — where every module in the patch keeps its LEDs and meters. On a
hundred-module patch that came to roughly **18 milliseconds of pure bookkeeping
on every single redraw**, and while the meters are moving the synth asks for
redraws several times a second.

It is worked out once now and reused until the patch actually changes:
**about 14 microseconds**. Cables no longer rebuild a lookup of every connector
in the patch on each redraw either, and resting the pointer on a knob stopped
redrawing the whole canvas on every mouse movement.

The difference is widest exactly where it hurt most: a large patch with the
meters running.

📸 **CAPTURA 6** — El overlay de coste DSP (`F3`) sobre un patch cargado. No hay
forma de fotografiar la velocidad, pero esta imagen dice "patch grande" mejor que
un párrafo.

## 🧠 And the crash family is closed off

Selecting a module, hovering one or reading its DSP cost all left the editor
holding the module itself in memory, and deleting it left those pointing at
nothing. That was the macOS crash in 0.15.0, which 0.15.1 kept at bay by sweeping
them all before each redraw.

Modules are now identified by **where they live in the patch** rather than by
where they sit in memory, so the question cannot be got wrong. The whole family
is closed off rather than patched over — and deleting a module and undoing it now
gives you the selection back, where before you had to find it and re-pick it.

## ⌨️ Shortcuts that stop colliding

The DSP cost overlay moved to `F3` and focus mode to `F4`: F10 belongs to the
menu bar on Windows and some Linux desktops, and macOS keeps F11 for Show
Desktop. The old keys still work. On macOS, wireframe moved to `Cmd+Shift+W`,
because the naked `Cmd+W` is the system's own close-window and was firing both
things at once. And menu shortcuts now sit right-aligned in their own column
instead of inside the item text, which the macOS menu bar was printing literally,
tab character and all.

## 🩹 Also fixed

- **Pressing A, B, C or D on the synth brings that slot up on screen.** The
  buttons looked dead: the editor followed the front panel only into a window
  that already happened to be open.
- **Nothing can be dragged off the canvas** any more, in any direction.
- **A module can no longer be buried under another** at the bottom of a column.
- **Nudging a selection into an edge stops it as a block** instead of piling the
  modules onto the same row.
- **Select All takes the text notes too.**
- **Closing the editor with the slot chooser open no longer leaks it.**
- **Error messages leave when their moment does**, and any status message can be
  dismissed with a click.
- **Module help** drops its `$Contents` placeholder text and follows the theme
  instead of staying near-white.
- **A sequencer's Clr button** parks every step at its default: a CtrlSeq fader
  returns to centre, not to zero.
- **The Inspector's section headings** share one readable colour instead of three
  that were chosen against a dark panel and came out too faint on Nord Classic.
- **Renaming a module reaches the synth**, so storing to a bank right after a
  rename no longer saves the old name.

## 🔬 For the curious

The canvas was one file of 9,330 lines and is now six, one per job. The upload
packetizer moved into a module of its own so the 166-byte packet rule and the
packet that frees a stuck synth are held there by tests rather than by memory.
And the whole project is built and tested by GitHub on every push, once normally
and once under the sanitizers — which earned its keep on the first day by
catching undefined behaviour in code written that same afternoon.

## Known limitations

- Stuck MIDI-IN notes are cleared with the synth's front-panel panic.
- macOS builds are unsigned: Gatekeeper needs **Open Anyway** in
  System Settings > Privacy & Security the first time.

## 📦 Downloads

*(links)*

---

## Español

# 🎛️ Animatek NME 0.16.0: la que fue hacia dentro

Casi todo lo que cambia aquí no es algo que se pueda señalar en pantalla, lo cual
es raro de decir en una release. **Los patches grandes se dibujan mucho más
rápido**, toda una familia de cuelgues ya no puede volver, y el proyecto tiene
por primera vez tests y un robot que los ejecuta. Se cierran nueve issues, y una
ronda de pruebas escrita a propósito para ir a buscar fallos destapó otros cinco.

También hay bastante que **sí** se ve, así que empecemos por ahí.

## 🎛️ El Inspector lista todos los parámetros, y te deja escribirlos

Un knob son cuatro píxeles conteniendo 128 pasos: perfecto para barrer, inútil
para decir cuánto vale algo. Al seleccionar un módulo, el Inspector se llena con
una sección **Parameters** con todos sus controles, cada uno con su nombre y la
cifra que marca en sus propias unidades.

Arrastra un valor para recorrerlo, o **haz doble clic y escríbelo**. Acepta la
lectura tal como se escribe, así que `440Hz`, `C#3` y `-12(Oct)` caen donde
quieres, y un número a secas elige el paso más cercano. Lo que el módulo lleva
como **botón** aquí también es un botón, con las mismas letras que en su cara: un
interruptor de dos estados se enciende, y un selector como el LP/BP/HP del
DrumSynth va rotando sus opciones.

Cada edición sale hacia el synth al momento y se deshace en un solo paso.

📸 **CAPTURA 1** (la misma que arriba)

## ↕️ Flechitas de ajuste, y `+` / `-`

Acertar una frecuencia exacta arrastrando un knob es adivinar. Al posar el
puntero sobre un knob o un fader salen los mismos dos botoncitos que muestra el
editor original: el izquierdo baja un paso, el derecho sube, y manteniéndolos se
repiten. El valor se lee mientras lo mueves.

`+` y `-` hacen lo mismo con lo que tengas bajo el puntero, así que un NoteDetect
se puede llevar a la nota exacta sin tocar el ratón. Mantenida, la tecla repite, y
toda la tirada es un único paso de deshacer. Los cuatro morphs de la barra
superior también las tienen.

📸 **CAPTURA 2** (la misma que arriba)

## 📝 Comentarios en el patch

Una nota de texto que puedes dejar en cualquier hueco, desde el grupo **ANME** de
la barra de módulos. Ocupa su rectángulo de la rejilla igual que un módulo, así
que los módulos le hacen sitio y ella se lo hace a ellos, y puedes tirar de sus
esquinas inferiores para agrandarla.

Las notas forman parte de la selección como todo lo demás: se copian, se pegan,
se mueven y se borran junto a los módulos que las rodean, así que una sección
rotulada del patch viaja de una pieza. Nunca llegan al synth, y sobreviven al
viaje: carga el patch de vuelta desde el Nord y las notas vuelven con él.

📸 **CAPTURA 3** (la misma que arriba)

## 💾 De dónde vino el patch, y un clic para devolverlo

Junto al nombre del patch hay ahora un **botón de guardar** que muestra la
posición del banco de la que vino el patch que tienes en pantalla, así que
devolverlo a su sitio es un clic y sin diálogo. Cuando el editor no conoce la
ubicación (un patch nuevo, o uno abierto de un fichero) vuelve a preguntar.

El diálogo de Store Patch to Bank también se abre en la ubicación del propio
patch en vez de en el banco 1, y Save Patch As se abre con el nombre del fichero
ya escrito.

📸 **CAPTURA 4** (la misma que arriba)

## 🗂️ El Inspector se pliega

Parameters, Morphs, Knobs y MIDI CC tienen ya el mismo chevron que tenía Presets,
así que un módulo con treinta parámetros y ninguna asignación no empuja sus
presets fuera de la columna. La elección se recuerda entre sesiones y la comparten
todos los Inspectores abiertos.

Los knobs asignados a los grupos de morph también aparecen ahí, y el Knob Floater
sigue a los morphs.

📸 **CAPTURA 5** (la misma que arriba)

## ⚡ Los patches grandes se dibujan mucho más rápido

Cada vez que el canvas pintaba un módulo calculaba, desde cero y por duplicado,
dónde guarda cada módulo del patch sus LEDs y sus medidores. En un patch de cien
módulos eso salían unos **18 milisegundos de puro papeleo en cada redibujado**, y
con los medidores en marcha el synth pide redibujados varias veces por segundo.

Ahora se calcula una vez y se reutiliza hasta que el patch cambia de verdad:
**unos 14 microsegundos**. Los cables tampoco reconstruyen ya una tabla de todos
los conectores en cada redibujado, y posar el puntero sobre un knob dejó de
repintar el canvas entero en cada movimiento del ratón.

La diferencia es mayor justo donde más dolía: un patch grande con los medidores
moviéndose.

📸 **CAPTURA 6** (la misma que arriba)

## 🧠 Y la familia de cuelgues queda cerrada

Seleccionar un módulo, pasarle el ratón por encima o leer su coste DSP dejaban al
editor guardando el módulo en memoria, y borrarlo dejaba todo eso apuntando a la
nada. Ese era el cuelgue de macOS en 0.15.0, que 0.15.1 mantenía a raya barriendo
todos esos punteros antes de cada redibujado.

Ahora un módulo se identifica por **dónde vive en el patch**, no por dónde está en
memoria, así que la pregunta no se puede responder mal. La familia entera queda
cerrada en vez de parcheada, y de paso borrar un módulo y deshacer te devuelve la
selección, cuando antes tenías que buscarlo y volver a marcarlo.

## ⌨️ Atajos que dejan de chocar

El overlay de coste DSP pasa a `F3` y el modo foco a `F4`: F10 es de la barra de
menús en Windows y en algunos escritorios de Linux, y macOS reserva F11 para
Mostrar Escritorio. Las teclas antiguas siguen valiendo. En macOS el wireframe
pasa a `Cmd+Shift+W`, porque el `Cmd+W` a secas es el cerrar ventana del sistema y
estaba disparando las dos cosas. Y los atajos del menú van ahora alineados a la
derecha en su propia columna en vez de dentro del texto, que el menú de macOS
imprimía tal cual, con el tabulador incluido.

## 🩹 También corregido

- **Pulsar A, B, C o D en el synth trae ese slot a la pantalla.** Los botones
  parecían muertos: el editor solo seguía al panel hacia una ventana que ya
  estuviera abierta.
- **Ya no se puede arrastrar nada fuera del canvas**, en ninguna dirección.
- **Un módulo ya no puede quedar enterrado bajo otro** al fondo de una columna.
- **Empujar una selección contra un borde la para como bloque** en vez de apilar
  los módulos en la misma fila.
- **Seleccionar todo coge también las notas de texto.**
- **Cerrar el editor con el selector de slot abierto ya no lo deja colgado.**
- **Los mensajes de error se van cuando toca**, y cualquier mensaje de estado se
  quita con un clic.
- **La ayuda de módulo** deja de mostrar el texto de relleno `$Contents` y sigue
  el tema en vez de quedarse casi blanca.
- **El botón Clr de un secuenciador** deja cada paso en su valor por defecto: un
  fader del CtrlSeq vuelve al centro, no a cero.
- **Las cabeceras del Inspector** comparten un color legible en vez de tres
  elegidos contra un panel oscuro que quedaban demasiado tenues en Nord Classic.
- **Renombrar un módulo llega al synth**, así que guardar en un banco justo
  después de renombrar ya no guarda el nombre viejo.

## 🔬 Para los curiosos

El canvas era un fichero de 9.330 líneas y ahora son seis, uno por cometido. El
empaquetador de subida se mudó a su propio módulo para que la regla de los 166
bytes y el paquete que descuelga un synth atascado estén sujetos por tests y no
por memoria. Y GitHub compila y prueba el proyecto entero en cada push, una vez
normal y otra bajo los sanitizers, que se ganaron el sueldo el primer día cazando
comportamiento indefinido en código escrito esa misma tarde.

## Limitaciones conocidas

- Las notas MIDI-IN atascadas se limpian con el pánico del panel frontal del
  synth.
- Las builds de macOS no están firmadas: Gatekeeper necesita **Open Anyway** en
  Ajustes del Sistema > Privacidad y Seguridad la primera vez.

## 📦 Descargas

*(enlaces)*
