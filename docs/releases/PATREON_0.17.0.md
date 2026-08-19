# Patreon post: Animatek NME 0.17.0

Two language versions are included below. Publish one or both.

**Screenshots to take before posting.** Marked in both versions with
`📸 CAPTURA` at the point in the post where the image goes. They are the same
six shots in each language, so take them once. Only the new features are
marked: the fixes go out as text.

| # | Shot | Where |
|---|------|-------|
| 1 | A cable being carried mid-re-route, hanging off the pointer | `Ctrl`-drag a connector on a patched module and stop with the cable following the mouse. A GIF would be better than a still if you can record one |
| 2 | A patch being dropped on a slot, with the target lit up | Drag a bank patch from the Synth tab over another slot's sub-window and shoot while it is highlighted |
| 3 | Four slot windows in A\|B over C\|D, and the ABCD button | Open four slots, shuffle them, then shoot right before and right after pressing ABCD |
| 4 | The synth's own display reading `ANME 0.17v` | A photo of the Nord, not a screenshot. Turn on "Show the editor on the synth display" and open any dialog |
| 5 | Editor Options with the new Cable Opacity slider | The Cable Style section, slider visible |
| 6 | A macOS menu showing `⌘N`, `⇧⌘S`, `⌥⌘1` | Mac only. The File menu open is enough |

---

## English

# 🔌 Animatek NME 0.17.0: the one where cables move

Two gestures the original editor had and this one did not: **a cable can be
lifted off a connector and dropped on another one**, and **a patch can be
dragged straight onto a slot**. Around them, four faults that only ever showed
on somebody else's machine, including a Mac menu bar with no keyboard shortcuts
on it and a patch that reloaded itself every three seconds.

## 🔌 Cables can be re-routed, not just cut

Hold `Ctrl` (`Cmd` or `Alt` also work) and drag a connector that already has a
cable: the cable comes off that end and follows the pointer from the end that
stays put, ready to drop on another connector. This is what moving a patch's
wiring onto a replacement module one cable at a time needs, and it is the
gesture the original editor had.

Nothing happens to the patch until you let go. The cable is only hidden from the
canvas while you carry it, so a re-route that lands nowhere legal costs nothing,
sends nothing to the synth, and leaves nothing on the undo stack. Where a
connector has several cables the one drawn on top comes off first, and repeating
the gesture takes the ones underneath. The move itself is one undo step.

📸 **CAPTURA 1** — Un cable a medio recolocar, colgando del puntero. Si puedes
grabar un GIF corto, aquí vale más que una foto fija.

## 🎯 Drag a patch onto a slot to load it there

From both browsers: a patch in the synth's banks from the **Synth** tab, and a
`.pch` from the **Disk** tab or the `Ctrl+B` window. Two places accept the drop:
the slot's own sub-window, which is the obvious gesture, and its row in the slot
bar down the left side, which is the one that still works when that slot's
window is closed. It opens on the way.

The target lights up while you are over it. Each drop ends in the load that
already existed, so nothing new goes to the synth. The `Ctrl+B` preset window
could not start a drag at all until now, snippets included.

📸 **CAPTURA 2** — Un patch soltándose sobre un slot, con el destino iluminado.

## 🔠 One button to put the slot windows back in order

**ABCD**, right of MUT in the header bar, with its four letters drawn in the
quadrants they land in so the button pictures the arrangement it produces. It
re-tiles the open sub-windows into A|B over C|D, or thirds, or side by side
depending on how many are open, and it puts the slots back into that order
whatever order you opened or shuffled them into. It also drops focus mode and
any layout you had dragged about, so one click is the way back to something
readable after loading four patches in whatever order they came.

**View > Slots > Reset Slot Order (ABCD)** is the same thing.

📸 **CAPTURA 3** — Cuatro slots ordenados A|B sobre C|D, y el botón ABCD en la
barra. Un antes y un después es lo que mejor lo cuenta.

## 🖥️ The editor can show itself on the synth's own display

Off by default. Turn on **Show the editor on the synth display** in Editor
Options and any dialog you open borrows the G1's display for `ANME 0.17v` while
it is up, giving the patch name back when it closes.

Nothing happens to the patch. Only the message that sets the name is sent, so
your patch is untouched, nothing is marked modified, and nothing lands on the
undo stack. On the synth it changes the edit buffer only, never the bank. It is
opt-in because an editor killed with a dialog open leaves the caption on the
display until the patch is reloaded.

📸 **CAPTURA 4** — La pantalla del propio Nord marcando `ANME 0.17v`. Una foto
del synth, no una captura de pantalla.

## 🤏 Pinch to zoom, and cable opacity where you can reach it

The canvas zooms around the pointer on a **two-finger pinch** on a trackpad, the
same zoom `Ctrl`/`Cmd`+wheel already did. Pinching out and back in lands on the
level you started from.

**Cable opacity** has moved out of the View menu and into **File > Editor
Options**, next to the cable style. It was a slider inside a menu, which a
native Mac menu cannot host at all, so on macOS it was a blank line and the
setting was unreachable. It is a percentage slider now: the canvas follows it
while you drag so you can see what you are choosing, and Cancel puts the old
value back. It also survives a restart, which it never did on any platform.

📸 **CAPTURA 5** — Editor Options con el slider de opacidad nuevo.

## 🍎 The macOS menus have their keyboard shortcuts back

If you are on a Mac and 0.16.0 left your menus bare, this is why. The Mac menu
bar is the system's own, and it only prints a shortcut that comes from a JUCE
command manager: the field the Windows and Linux menus use for the hint is
thrown away there. Moving the hints into that field, so the Mac would stop
printing a literal tab character, took every shortcut off the Mac menus.

They are written into the item's own text now, in the symbols a Mac reads:
**New Patch ⌘N**, **Save As... ⇧⌘S**, **Slot A ⌥⌘1**. They also stop saying
"Ctrl" for what is Cmd on your machine. Nothing changes on Windows or Linux.

📸 **CAPTURA 6** — Un menú de macOS con los atajos visibles. Solo para Mac.

## 🔁 The patch stops reloading itself

The canvas jumped back to its top-left corner on its own and the status bar
flashed "loading patch 1/13", with nobody touching anything.

The G1 does not only answer the editor's hello, it announces itself: in the
SysEx dumps behind this report one arrives roughly every three seconds for as
long as the synth is on. The editor read each announcement as a connection that
had just come up and re-ran its whole opening sequence, so the patch was pulled
down again on top of the one being worked in. An announcement from a synth the
editor is already talking to now changes nothing.

Thanks to Nocticore for the dumps: this one was not findable without them.

## 🩹 Also fixed

- **Uni/Bip switches say which polarity they are in.** Constant, LevMult, LevAdd
  and the Control Sequencer all have one, and the inherited panel data labelled
  both of its states "Uni": the only clue was the bevel.
- **A patch opened from a bank backup no longer keeps the position number in its
  name.** Loading `35 - BELLS++` stored it on the synth as `35 - BELLS++`
  instead of `BELLS++`.
- **The empty-canvas hint is readable on every theme.** "Press Enter to add
  modules" was black on near-black on the themes with light module faces.
- **The status bar follows the theme**, including when the theme changes while
  it is on screen, which it never did.

## Known limitations

- Stuck MIDI-IN notes are cleared with the synth's front-panel panic.
- macOS builds are unsigned: Gatekeeper needs **Open Anyway** in
  System Settings > Privacy & Security the first time.

## 📦 Downloads

*(links)*

---

## Español

# 🔌 Animatek NME 0.17.0: la de los cables que se mueven

Dos gestos que el editor original tenía y este no: **un cable se puede levantar
de un conector y soltarlo en otro**, y **un patch se puede arrastrar
directamente sobre un slot**. Alrededor, cuatro fallos que solo se veían en
máquinas ajenas, entre ellos una barra de menús de Mac sin un solo atajo y un
patch que se recargaba solo cada tres segundos.

## 🔌 Los cables se recolocan, no solo se cortan

Mantén `Ctrl` (`Cmd` o `Alt` también valen) y arrastra un conector que ya tenga
cable: el cable se suelta de ese extremo y sigue al puntero desde el que se
queda, listo para dejarlo en otro conector. Es lo que hace falta para llevar el
cableado de un patch a un módulo de repuesto cable a cable, y es el gesto que
tenía el editor original.

No le pasa nada al patch hasta que sueltas. El cable solo se esconde del canvas
mientras lo llevas, así que una recolocación que no acabe en ningún sitio válido
no cuesta nada, no manda nada al synth y no deja nada en el historial de
deshacer. Si el conector tiene varios cables sale primero el que está dibujado
encima, y repitiendo el gesto salen los de debajo. El movimiento en sí es un
único paso de deshacer.

📸 **CAPTURA 1** (la misma que arriba)

## 🎯 Arrastra un patch sobre un slot para cargarlo ahí

Desde los dos navegadores: un patch de los bancos del synth desde la pestaña
**Synth**, y un `.pch` desde la pestaña **Disk** o desde la ventana de `Ctrl+B`.
Hay dos sitios que aceptan la soltada: la propia subventana del slot, que es el
gesto evidente, y su fila en la barra de slots de la izquierda, que es la que
sigue funcionando cuando esa ventana está cerrada. Se abre por el camino.

El destino se ilumina mientras estás encima. Cada soltada acaba en la carga que
ya existía, así que no sale nada nuevo hacia el synth. La ventana de `Ctrl+B` no
podía iniciar un arrastre en absoluto hasta ahora, snippets incluidos.

📸 **CAPTURA 2** (la misma que arriba)

## 🔠 Un botón para volver a poner los slots en orden

**ABCD**, a la derecha de MUT en la barra superior, con sus cuatro letras
dibujadas en los cuadrantes donde van a caer, de modo que el botón es un dibujo
de lo que hace. Recoloca las subventanas abiertas en A|B sobre C|D, o en tercios,
o una al lado de otra según cuántas haya, y devuelve los slots a ese orden sin
importar en qué orden los abrieras o los movieras. También quita el modo foco y
cualquier disposición que hubieras arrastrado a mano, así que un clic es la vuelta
a algo legible después de cargar cuatro patches como vinieron.

**View > Slots > Reset Slot Order (ABCD)** hace lo mismo.

📸 **CAPTURA 3** (la misma que arriba)

## 🖥️ El editor puede salir en la pantalla del propio synth

Desactivado por defecto. Activa **Show the editor on the synth display** en
Editor Options y cualquier diálogo que abras toma prestada la pantalla del G1
para poner `ANME 0.17v` mientras está abierto, devolviendo el nombre del patch al
cerrarse.

Al patch no le pasa nada. Solo se manda el mensaje que fija el nombre, así que tu
patch queda intacto, no se marca como modificado y no cae nada en el historial de
deshacer. En el synth cambia únicamente el buffer de edición, nunca el banco. Es
opcional porque un editor que muera con un diálogo abierto deja el rótulo en la
pantalla hasta que se recargue el patch.

📸 **CAPTURA 4** (la misma que arriba)

## 🤏 Pellizco para hacer zoom, y la opacidad de cables donde se puede tocar

El canvas hace zoom alrededor del puntero con un **pellizco de dos dedos** en el
trackpad, el mismo zoom que ya hacía `Ctrl`/`Cmd`+rueda. Abrir el pellizco y
volver a cerrarlo te deja en el nivel del que saliste.

La **opacidad de los cables** se ha ido del menú View a **File > Editor
Options**, junto al estilo de cable. Era un slider dentro de un menú, y un menú
nativo de Mac no puede alojar uno: allí salía una línea en blanco y el ajuste era
inalcanzable. Ahora es un slider en porcentaje, el canvas lo sigue mientras
arrastras para que veas lo que estás eligiendo, y Cancel devuelve el valor
anterior. Además sobrevive a un reinicio, cosa que no hacía en ninguna
plataforma.

📸 **CAPTURA 5** (la misma que arriba)

## 🍎 Los menús de macOS recuperan sus atajos

Si estás en un Mac y la 0.16.0 te dejó los menús pelados, esta es la razón. La
barra de menús del Mac es la del sistema, y solo imprime un atajo que venga de un
command manager de JUCE: el campo que usan los menús de Windows y Linux para la
pista se descarta allí. Al mover las pistas a ese campo, para que el Mac dejara
de imprimir un tabulador literal, desaparecieron todos los atajos de los menús de
Mac.

Ahora van escritos en el texto del propio elemento, con los símbolos que un Mac
lee: **New Patch ⌘N**, **Save As... ⇧⌘S**, **Slot A ⌥⌘1**. Y dejan de decir
"Ctrl" para lo que en tu máquina es Cmd. En Windows y Linux no cambia nada.

📸 **CAPTURA 6** (la misma que arriba)

## 🔁 El patch deja de recargarse solo

El canvas volvía solo a su esquina superior izquierda y la barra de estado
parpadeaba con "loading patch 1/13", sin que nadie tocara nada.

El G1 no solo responde al saludo del editor, se anuncia él solo: en los volcados
de SysEx de este reporte llega uno cada tres segundos mientras el synth esté
encendido. El editor leía cada anuncio como una conexión recién establecida y
repetía toda su secuencia de arranque, así que el patch se descargaba otra vez
encima del que estabas trabajando. Un anuncio de un synth con el que el editor ya
está hablando ya no cambia nada.

Gracias a Nocticore por los volcados: este no había forma de encontrarlo sin
ellos.

## 🩹 También corregido

- **Los interruptores Uni/Bip dicen en qué polaridad están.** Constant, LevMult,
  LevAdd y el Control Sequencer tienen uno, y los datos de panel heredados
  etiquetaban "Uni" sus dos estados: la única pista era el bisel.
- **Un patch abierto desde una copia de banco ya no se queda con el número de
  posición en el nombre.** Cargar `35 - BELLS++` lo guardaba en el synth como
  `35 - BELLS++` en vez de como `BELLS++`.
- **El aviso del canvas vacío se lee en todos los temas.** "Press Enter to add
  modules" salía negro sobre casi negro en los temas de módulos claros.
- **La barra de estado sigue al tema**, también cuando el tema cambia con ella en
  pantalla, cosa que nunca hacía.

## Limitaciones conocidas

- Las notas MIDI-IN atascadas se limpian con el pánico del panel frontal del
  synth.
- Las builds de macOS no están firmadas: Gatekeeper necesita **Open Anyway** en
  Ajustes del Sistema > Privacidad y Seguridad la primera vez.

## 📦 Descargas

*(enlaces)*
