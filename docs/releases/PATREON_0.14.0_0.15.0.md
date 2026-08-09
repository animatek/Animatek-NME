# Patreon post: Animatek NME 0.14.0 + 0.15.0

Two language versions are included below. Publish one or both.

## English

# 🎛️ Animatek NME 0.15.0: put it where you want it, and a synth that stops going quiet

Version 0.14.0 never went out, so this release carries everything from **0.14.0
and 0.15.0** together.

You only need to download **0.15.0**. It already includes every improvement from
0.14.0. If you are coming from 0.13.0, this post covers everything that has
changed since then.

## 🎛️ The module bar is back

The original editors put the module palette in a bar grouped by category, right
above the patch, and plenty of people reach for a module there rather than
through a tree. It is back, laid out the way Clavia's is: category tabs across
the top — **In/Out, Osc, LFO, Env, Filter, Mixer, Audio, Ctrl, Logic, Seq** — and
the modules of the chosen category underneath.

**Drag** one onto a patch area, or **click** it and the module hangs off the
pointer until you click where you want it, which lets a single click choose the
area and the slot as well as the spot.

The bar remembers the tab you left it on, and **View > Module Icon Bar** turns it
off if you would rather have the pixels for the canvas.

The original shows pictograms rather than names, and so will this. Not yet,
though: the only ready-made set is Nomad's, and those are coloured discs that
ignore the theme entirely. Our own artwork is coming, and until it does the bar
shows names.

## 🫥 Paste and Add Module hand you the modules

Neither one places anything any more. They show the modules as outlines that
follow the cursor, and you click where you want them. `Escape` or a right-click
throws them away without adding anything.

Because the click chooses the spot, it chooses the **area** as well, and that
quietly closes a lot at once:

- Copies land where you are working instead of somewhere off in the canvas.
- Poly to Common and back works, in either direction, for the first time.
- Pasting inside the Common area works at all.
- The clipboard belongs to the whole editor, so a copy made in one slot can be
  pasted into another slot's window.

Adding from the keyboard is still the quickest route: **Enter**, a few letters,
**Enter** to pick the module, **Enter** again to drop it where the pointer
already is. Reach for the mouse only when the module belongs somewhere else.

**Cut, Copy, Paste and Duplicate are on the Edit menu** now, where anyone would
look for them, and **Paste and Duplicate can be undone**, which they never could.

## 🪜 Nothing gets buried

Drop a module on top of another and the modules below move down their column to
make room, cascading into whatever they run into. Before, the older module was
simply covered and there was nothing on screen to say it was still in the patch.
Undo puts the whole column back.

## 📐 Frequency displays that change units

Click the frequency box on an oscillator, a slave LFO or a filter and it changes
the units it reads in, the way the original does. An absolute frequency
alternates between hertz and the note it lands on. A **slave oscillator** goes
round three: the partial ratio, the interval in semitones, and the frequency its
master actually puts it at.

**Hovering shows the units the box is not displaying**, so a suboscillator can be
set to `-12(Oct)` and checked in hertz without touching anything. The choice
belongs to each module and is saved with the patch, in the same place the
original keeps it.

## 🥁 The Drum Synthesizer's factory presets

All 29 of Clavia's own, from Kick 1 to Perc 6, in a **Factory** group that stays
folded away so the presets you saved yourself are still the ones you reach first.
They cannot be renamed or deleted and never touch your own preset files.

The module also had its **filter types labelled the wrong way round**: picking LP
sent the synth the value it reads as HP. Clavia's own presets settle it, and every
patch built or loaded now reads correctly. A new Drum Synthesizer also comes up
with the original's own default settings rather than a middle value in every knob.

## 🎚️ Knobs behave the way the original's do

**Vertical movement is the default** now, which is how the original behaves and
what most people reach for first. Whatever you have chosen under **Ctrl+, > Knob
Control** is left alone.

**Circular control reads the dial** rather than counting turns. Grab a knob at the
point marked 100 and it goes to 100, instead of turning up by 100 from wherever it
was. The response no longer speeds up near the point you grabbed and crawls far
from it.

**Turning a knob is no longer stopped by the edge of the desktop.** The pointer is
hidden while you turn, as in the original, and comes back on the knob when you let
go, so a sweep covers the whole range wherever the knob sits on screen. The morph
knobs suffered worst, sitting a few pixels from the top of the screen; they also
**follow the knob-control setting** now, which they never did.

## 🩹 The three transfer faults

These are the ones worth reading if you work with big patches.

**A patch of around a hundred modules can be uploaded again.** It always failed:
the synth rejected it with a checksum error partway through and the transfer died,
so a patch like `SY-1 RndBlips1` could be downloaded but never sent back. The
bytes were right; the shape was wrong. Each of the sixteen sections was going out
as one packet, which works only while every section stays small, and the
module-name section grows with the module count. The synth is built to receive one
continuous stream cut into small packets, and that is what it gets now.

**A failed upload no longer leaves the synth deaf.** The synth waits for a packet
marked as the last one before it leaves bulk-receive mode, and an upload that
simply stopped never sent one. Parked there it answered nothing at all: no
acknowledgements, no reply to the editor's handshake, not even its own idle
stream. It looked like dead hardware that needed a power cycle. It never did, and
now every way out of an upload closes the transfer properly.

**Loading a patch from the synth's own bank refreshes the editor.** A synth still
writing a large patch was not answering the request that follows, and the editor
gave up without a word and sat on the previous patch. It asks again now, and says
so if the synth really is not answering.

## 🩹 Other fixes

- **Buttons answer as readily as knobs.** Every press of `KBT`, a mute or a
  waveform selector was putting the same message on the wire twice and rebuilding
  the morph list and the DSP figures behind it, which walk the whole patch. A knob
  pays that once per drag; a button was paying on every click.
- **Zooming the Note Sequencer's piano roll no longer retunes its first step.**
  The zoom is a display setting, but it was being sent to the synth as an ordinary
  parameter, and it shares an index with the sequence's first note.
- **The overlay readouts are on the View menu** as well as on `F5` and `F7`-`F10`,
  ticked to show which one is open.
- **The slave LFOs no longer carry arrow buttons they never had.** They ran through
  LFOSlvA's Mute button and over the bottom edge of LFOSlvC and LFOSlvE. The ratio
  is set with the knob beside the display, as it always was, so nothing loses a
  control.
- **On macOS, showing and hiding a slot moved to `Cmd+Alt+1`-`Cmd+Alt+4`.** macOS
  keeps `Cmd+Shift+3` and `Cmd+Shift+4` for its own screen capture and never
  passes them on, so two of the four slots could not be toggled from the keyboard
  at all. Windows and Linux are unchanged.
- **Accented and typographic characters no longer come out as garbage.** The slot
  chooser showed `Local (editor only â€ don't upload)` on macOS, and every
  quotation mark in the module help text was mangled the same way.
- **The editor no longer corrupts its own memory as it closes.** Both browser
  panels let their tree view outlive the item it was displaying, so every session
  ended by writing into memory that had just been freed.
- **Saving a selection as a snippet no longer overwrites the clipboard.**
- **A long-lived patch can no longer run out of module slots when inserting a
  snippet.** Enough add-and-delete cycles pushed the module number past the 127 the
  format allows, and the insert failed.

## 📖 The manual is up to date

In English and Spanish: **https://animatek.net/animatek-nme/manual/**

## 📦 Downloads

Download **Animatek NME 0.15.0** below for Linux, Windows or macOS. There is no
need to install 0.14.0 first.

Official SHA-256 checksums:

```text
Linux:   bbe6fad00795c949e99e90803cda9003ba1c549e388d9c16582fbc7adc86363e
macOS:   97df8c108a50af3702f5379ba6e43787df706323705db77ae25d0bd3c2ba50d4
Windows: 6627633eef56b56fb6fe0487a11d34d6185b860a2d0258f3ca816e61c3c8d378
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

# 🎛️ Animatek NME 0.15.0: ponlo donde tú quieras, y un sinte que deja de quedarse mudo

La versión 0.14.0 nunca llegó a salir, así que esta entrega reúne todo lo de
**0.14.0 y 0.15.0**.

Solo necesitas descargar **0.15.0**, que ya incluye todas las mejoras de 0.14.0.
Si vienes de 0.13.0, este post cubre todos los cambios desde entonces.

## 🎛️ Vuelve la barra de módulos

Los editores originales ponen la paleta de módulos en una barra agrupada por
categoría, justo encima del patch, y mucha gente va a buscar un módulo ahí antes
que en un árbol. Vuelve, colocada como la de Clavia: pestañas de categoría arriba
(**In/Out, Osc, LFO, Env, Filter, Mixer, Audio, Ctrl, Logic, Seq**) y debajo los
módulos de la categoría elegida.

**Arrastra** uno al área de patch, o **haz clic** y el módulo se queda colgando
del puntero hasta que pulses donde lo quieras, con lo que un solo clic elige el
área y el slot además del sitio.

La barra recuerda la pestaña en la que la dejaste, y **View > Module Icon Bar** la
quita si prefieres esos píxeles para el canvas.

El original muestra pictogramas en vez de nombres, y este acabará haciéndolo. Aún
no: el único juego ya hecho es el de Nomad, y son discos de colores que ignoran
por completo el tema. Los nuestros están en camino, y hasta entonces la barra
muestra nombres.

## 🫥 Pegar y Añadir módulo te entregan los módulos

Ninguno de los dos coloca ya nada. Muestran los módulos como siluetas que siguen
al cursor, y tú haces clic donde los quieres. `Escape` o un clic derecho los tira
sin añadir nada.

Como el clic elige el sitio, elige también el **área**, y eso cierra de golpe
unas cuantas cosas:

- Las copias caen donde estás trabajando y no en cualquier rincón del canvas.
- De Poly a Common y al revés funciona, en los dos sentidos, por primera vez.
- Pegar dentro del área Common funciona, sin más.
- El portapapeles es de todo el editor, así que una copia hecha en un slot se
  puede pegar en la ventana de otro.

Añadir desde el teclado sigue siendo lo más rápido: **Enter**, unas letras,
**Enter** para elegir el módulo y **Enter** otra vez para soltarlo donde ya está
el puntero. El ratón solo hace falta si el módulo va a otro sitio.

**Cortar, Copiar, Pegar y Duplicar están ya en el menú Edit**, donde cualquiera
los buscaría, y **Pegar y Duplicar se pueden deshacer**, cosa que nunca se pudo.

## 🪜 Ya no se entierra nada

Suelta un módulo encima de otro y los de abajo bajan por su columna para hacer
sitio, empujando a su vez a los que se encuentren. Antes el módulo antiguo se
quedaba tapado y nada en pantalla decía que seguía en el patch. Deshacer devuelve
la columna entera.

## 📐 Pantallas de frecuencia que cambian de unidades

Haz clic en la caja de frecuencia de un oscilador, un LFO esclavo o un filtro y
cambia las unidades en que lee, como en el original. Una frecuencia absoluta
alterna entre hercios y la nota en la que cae. La de un **oscilador esclavo**
recorre tres: la relación de parciales, el intervalo en semitonos y la frecuencia
a la que lo lleva realmente su maestro.

**Al pasar el ratón se muestran las unidades que la caja no está enseñando**, así
que un suboscilador se puede poner en `-12(Oct)` y comprobarlo en hercios sin
tocar nada. La elección es de cada módulo y se guarda con el patch, en el mismo
sitio donde la guarda el original.

## 🥁 Los presets de fábrica del Drum Synthesizer

Los 29 de Clavia, de Kick 1 a Perc 6, en un grupo **Factory** que se queda plegado
para que los presets que guardaste tú sigan siendo los primeros a mano. No se
pueden renombrar ni borrar, y no tocan tus archivos de presets.

El módulo tenía además **los tipos de filtro etiquetados al revés**: elegir LP
mandaba al sinte el valor que él lee como HP. Los presets de la propia Clavia lo
zanjan, y cualquier patch que construyas o cargues se lee ya correctamente. Un
Drum Synthesizer nuevo también aparece con los ajustes por defecto del original y
no con un valor medio en cada knob.

## 🎚️ Los knobs se comportan como los del original

**El movimiento vertical es ya el predeterminado**, que es como se comporta el
original y lo que casi todo el mundo intenta primero. Lo que tengas elegido en
**Ctrl+, > Knob Control** se respeta.

**El control circular lee el dial** en vez de contar vueltas. Agarra un knob en el
punto marcado 100 y va a 100, en lugar de subir 100 desde donde estuviera. La
respuesta ya no se acelera cerca del punto que agarraste ni se arrastra lejos de
él.

**Girar un knob ya no se detiene en el borde del escritorio.** El puntero se
oculta mientras giras, como en el original, y vuelve sobre el knob al soltar, así
que un barrido cubre todo el recorrido esté donde esté el knob. Los que peor lo
llevaban eran los de morph, que están a unos píxeles del borde superior; además
ahora **siguen el ajuste de control de knobs**, cosa que nunca hicieron.

## 🩹 Los tres fallos de transferencia

Estos son los que interesan si trabajas con patches grandes.

**Un patch de un centenar de módulos se puede volver a subir.** Fallaba siempre:
el sinte lo rechazaba con un error de checksum a mitad y la transferencia moría,
así que un patch como `SY-1 RndBlips1` se podía descargar pero nunca devolver. Los
bytes eran correctos; la forma no. Cada una de las dieciséis secciones salía como
un solo paquete, algo que funciona solo mientras todas las secciones sean
pequeñas, y la de nombres de módulo crece con el número de módulos. El sinte está
hecho para recibir un flujo continuo cortado en paquetes pequeños, y eso es lo que
recibe ahora.

**Una subida fallida ya no deja el sinte sordo.** El sinte espera un paquete
marcado como el último antes de salir del modo de recepción, y una subida que
simplemente se paraba nunca lo mandaba. Ahí aparcado no contestaba absolutamente
nada: ni confirmaciones, ni respuesta al saludo del editor, ni siquiera su propio
flujo de reposo. Parecía hardware muerto que necesitaba apagarse y encenderse.
Nunca lo necesitó, y ahora toda salida de una subida cierra la transferencia como
es debido.

**Cargar un patch desde el banco del propio sinte refresca el editor.** Un sinte
que todavía estaba escribiendo un patch grande no contestaba a la petición que
viene después, y el editor se rendía sin decir nada y se quedaba con el patch
anterior. Ahora vuelve a preguntar, y avisa si el sinte de verdad no contesta.

## 🩹 Otras correcciones

- **Los botones responden igual de rápido que los knobs.** Cada pulsación de
  `KBT`, un mute o un selector de onda ponía el mismo mensaje dos veces en la
  línea y reconstruía detrás la lista de morphs y las cifras de DSP, que recorren
  el patch entero. Un knob paga eso una vez por arrastre; un botón lo pagaba en
  cada clic.
- **Hacer zoom en el piano roll del Note Sequencer ya no desafina su primer
  paso.** El zoom es un ajuste de pantalla, pero se mandaba al sinte como un
  parámetro normal, y comparte índice con la primera nota de la secuencia.
- **Las lecturas de superposición están en el menú View** además de en `F5` y
  `F7`-`F10`, con una marca para ver cuál está abierta.
- **Los LFO esclavos ya no llevan flechas que nunca tuvieron.** Pasaban por encima
  del botón Mute del LFOSlvA y del borde inferior del LFOSlvC y el LFOSlvE. La
  relación se pone con el knob que hay junto al display, como siempre, así que no
  se pierde ningún control.
- **En macOS, mostrar y ocultar un slot pasa a `Cmd+Alt+1`-`Cmd+Alt+4`.** macOS se
  reserva `Cmd+Shift+3` y `Cmd+Shift+4` para sus capturas de pantalla y no los
  cede, así que dos de los cuatro slots no se podían conmutar desde el teclado.
  En Windows y Linux no cambia nada.
- **Los caracteres acentuados y tipográficos ya no salen como basura.** El
  selector de slot mostraba `Local (editor only â€ don't upload)` en macOS, y
  todas las comillas del texto de ayuda de los módulos salían igual de rotas.
- **El editor ya no corrompe su propia memoria al cerrarse.** Los dos paneles de
  navegación dejaban que su árbol sobreviviera al elemento que estaba mostrando,
  así que cada sesión terminaba escribiendo en memoria recién liberada.
- **Guardar una selección como snippet ya no pisa el portapapeles.**
- **Un patch de larga vida ya no se queda sin huecos de módulo al insertar un
  snippet.** Bastantes ciclos de añadir y borrar empujaban el número de módulo más
  allá de los 127 que permite el formato, y la inserción fallaba.

## 📖 El manual está al día

En español y en inglés: **https://animatek.net/animatek-nme/manual/**

## 📦 Descargas

Descarga **Animatek NME 0.15.0** para Linux, Windows o macOS en los archivos
adjuntos. No es necesario instalar antes la 0.14.0.

Sumas de comprobación SHA-256 oficiales:

```text
Linux:   bbe6fad00795c949e99e90803cda9003ba1c549e388d9c16582fbc7adc86363e
macOS:   97df8c108a50af3702f5379ba6e43787df706323705db77ae25d0bd3c2ba50d4
Windows: 6627633eef56b56fb6fe0487a11d34d6185b860a2d0258f3ca816e61c3c8d378
```

Sigue siendo beta. Haz una copia de seguridad de los patches que te importen antes
de usar esto con tu Nord Modular, y mantén el archivo `.var` de cada patch junto a
su `.pch` para conservar las variaciones y las exclusiones de mutación.

Limitaciones conocidas: las notas de MIDI-IN colgadas se limpian con el panic del
panel frontal; las builds de macOS no están firmadas, así que Gatekeeper necesita
el clic derecho → Abrir la primera vez; y F12 (valores actuales de los
controladores MIDI) del editor original todavía no está implementado.

Gracias por apoyar Animatek NME. 🙏
