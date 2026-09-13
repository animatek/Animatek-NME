# 4. Trabajar con el sintetizador

## Slots

El G1 hace sonar hasta cuatro patches a la vez en los slots A–D. El editor
reproduce fielmente el sistema de slots de dos niveles del hardware:

- **Slot seleccionado** (LED parpadeando): el que estás editando y tocando desde
  el teclado. Clic normal en la barra de slots para seleccionarlo; el editor
  carga el patch de ese slot. `Ctrl+1`–`Ctrl+4` cambian desde el teclado.
- **Slots activos** (LED fijo): los slots que suenan. Puede haber varios a la
  vez. `Ctrl+click` en un slot lo activa o desactiva sin seleccionarlo, el mismo
  gesto que `Shift+botón de slot` en el panel frontal.

Cada slot guarda su propio patch, su historial de deshacer y su estado de
sincronización; los slots de fondo nunca contaminan aquel en el que trabajas.
Una transferencia en un slot ya no bloquea a los demás: puedes seguir editando
el slot A mientras el B sube o baja datos.

En cuanto el editor sabe qué slots están ocupados, descarga sus patches en
segundo plano, de uno en uno, para que cambiar a un slot por primera vez sea
instantáneo en vez de disparar una descarga completa. Volver a un slot del que
el editor ya tiene una copia al día tampoco lo vuelve a descargar; un cambio
real en el sintetizador (program change, carga de banco, reconexión) siempre lo
hace.

## Los cuatro slots en pantalla

Los slots son **subventanas dentro de la ventana principal**, distribuidas en
mosaico como colocaban los patches el editor original de Clavia y Nomad. Así se
trabaja con dos o más patches en paralelo, y nunca se pierde nada detrás de otra
aplicación.

Los slots abiertos **se reparten solos** el espacio: uno ocupa toda el área de
trabajo, dos la parten por la mitad, tres van en tercios, cuatro en 2x2, y la
distribución se rehace al abrir y cerrar. No hay nada que colocar salvo que
quieras: arrastra o redimensiona una subventana y las ventanas se quedan donde
las pongas a partir de ese momento, con **View > Slots > Tile Slots** para
volver al mosaico.

- `Ctrl+Shift+1`–`Ctrl+Shift+4` (`Cmd+Alt+1`–`Cmd+Alt+4` en macOS) muestra u oculta
  la subventana de un slot, igual
  que el clic derecho en su fila de la barra de slots.
- `Ctrl+1`–`Ctrl+4` cambia a un slot, abriéndolo si estaba cerrado.
- `F11`, o el botón de maximizar de una subventana, agranda el slot con foco a
  toda el área de trabajo y lo devuelve, para mirarlo de cerca.
- `Ctrl+Shift+` una flecha mueve el slot con foco a la casilla vecina, así el
  patch en el que trabajas va donde tú quieres. **View > Slots > Rotate Slots**
  los desplaza todos a la vez.

Las subventanas se deslizan hasta su nuevo sitio en vez de saltar. Desactívalo
con **Animate Slot Tiling** en Editor Options (`Ctrl+,`) si lo prefieres
instantáneo.

Cada slot conserva su propio canvas, su selección y su historial de deshacer, y
las ediciones caen en el slot correcto aunque no tenga el foco del hardware.
Todas las subventanas siguen al sintetizador en vivo: girar un knob físico del
panel frontal, o el movimiento de una luz o un medidor, anima el slot que toca.
`Ctrl+R` / `Ctrl+Shift+R` randomizan (uniforme / gaussiano) y `Ctrl+S` /
`Ctrl+Shift+S` guardan y guardan como, actuando sobre el slot **con foco** y
respetando su propia selección de módulos.

El Inspector, la cabecera, los navegadores y la barra de estado son compartidos y
siguen a la subventana con foco, y un canvas de fondo conserva su selección, así
que el Inspector retoma donde ese slot lo dejó en vez de quedarse en blanco. La
subventana con foco lleva un borde del color de texto del tema.

Qué slots tenías abiertos, cuál tenía el foco y cómo estaban colocados vuelven al
reabrir el editor. Al conectar con el Nord, el área de trabajo se alinea una vez
con los slots que el sintetizador tiene activados; a partir de ahí las ventanas
son solo tuyas, y pulsar los botones de slot del panel frontal mueve el foco sin
cerrar ninguna.

## Sincronización editor ↔ sintetizador

Mientras hay conexión, cada edición (parámetros, cables, módulos, morphs,
asignaciones de knob y CC, nombre del patch) se envía al sintetizador según la
haces, y los cambios hechos en el panel frontal vuelven al editor. No hay ningún
botón de "enviar" que recordar.

Seleccionar un slot descarga su patch del sintetizador.

## Abrir un patch: elegir dónde va

Abrir un `.pch` (File → Open, o cualquiera de los dos navegadores) pregunta
**dónde ponerlo**. El selector lista los slots A–D con el patch que hay en cada
uno, propone el slot activo por defecto, y añade una opción **Local**:

- Elige **A–D** y el patch se carga en ese slot y sube al sintetizador,
  reemplazando lo que hubiera.
- Elige **Local** y el patch se carga solo en el editor; no se envía nada al
  sintetizador. Sirve para curiosear patches sin tocar lo que el rack está
  tocando.

Un slot cuyo patch en el editor no se sabe que coincida con el del sintetizador
(cargado como Local, o cargado o construido sin conexión) lleva una insignia
**LOCAL** en la barra de slots. La insignia desaparece en cuanto ese patch se
sube al sintetizador, o se descarga de él.

## El navegador de patches del sintetizador

El navegador de la derecha (`Ctrl+B`) lista los 9 bancos internos del
sintetizador. Desde ahí puedes:

- buscar y ocultar posiciones vacías,
- **cargar** un patch en un slot: el doble clic lo pone en el slot en el que
  estás, y el clic derecho ofrece **Load to Slot A..D**, de modo que con varias
  subventanas abiertas puedes llevar un patch a una concreta sin salir antes del
  slot en el que trabajas,
- **almacenar** el patch actual en una posición de banco,
- **copiar, mover y borrar** patches dentro de la memoria del sintetizador.

Los archivos Nord Modular 2.10 antiguos se marcan como **PCH2** en el navegador
de disco y se cargan de forma transparente.

## Sets de slots

Los patches por capas, uno en cada slot, solo suenan como deben juntos, y el G1
no tiene manera de guardar el grupo: una posición de banco guarda un patch. Un
**set de slots** guarda ese grupo en el editor.

- **File > Save Slot Set...** pide un nombre y, para cada slot, si el set
  **incluye su patch** (*Include patch*), **apaga el slot** (*Switch slot off*) o
  **lo deja como está** (*Leave as is*). Un set de dos o tres patches es un set
  con el resto de slots como están. Las notas, opcionales, se enseñan cada vez
  que se carga el set.
- El set se escribe en `Sets/<nombre>/` dentro de la librería: una copia de cada
  patch incluido (con su `.var`), nombrada con la letra de su slot, y un
  manifiesto `<nombre>.nmset`. Las copias son `.pch` normales, así que editar
  después un patch no cambia el set: para actualizarlo, se vuelve a guardar.
- **File > Open Slot Set...**, o doble clic en una entrada **SET** de la pestaña
  Disk, enseña primero qué va a cada slot y qué sustituye. Después los patches
  van al sintetizador de uno en uno, cada uno esperando a que se confirme la
  subida anterior; los slots se encienden o apagan como se guardaron, y el foco
  pasa al slot que estaba activo al guardar. El sintetizador nunca apaga el slot
  enfocado, así que el foco siempre cae en un slot que el set deja encendido.
- Sin conexión, los patches se cargan solo en el editor (**LOCAL**) y no se
  enciende ni apaga ningún slot.
- Si falla una subida, la carga sigue con el siguiente slot, ese slot queda
  LOCAL y al final un mensaje explica qué ha fallado.

El manifiesto apunta también la posición de banco de cada patch cuando el editor
la conocía (`414`), solo como referencia: un set carga siempre sus propias
copias, nunca el banco.

## Transferencia de bancos (menú Device)

- **Save Bank to Disk**: vuelca un banco entero del sintetizador a una carpeta;
  la posición se conserva en los nombres de archivo `NN - Nombre.pch`.
- **Send Bank to Synth**: sube una carpeta de patches a un banco, con aviso de
  sobrescritura; una transferencia fallida se detiene limpiamente.
- **Backup All Banks to Library**: replica los 9 bancos en las carpetas
  `Banks/Bank1`–`Bank9` de tu librería en una sola acción.

Todas las transferencias muestran progreso y se pueden cancelar.

## Controller snapshot (menú Device)

**Send Controller Snapshot** le pide al *sintetizador* que emita los valores
actuales de las asignaciones MIDI CC del patch como mensajes CC por su MIDI OUT,
la misma función que CTRL SNAP SHOT en el panel frontal, útil para preparar una
grabación en un secuenciador. No cambia ningún estado del sintetizador.

## Velocidad de envío

Editor Options (`Ctrl+,`) incluye un ajuste de **send speed** que regula los
envíos masivos de parámetros (Mutator, Randomize) para que los patches grandes
no desborden al sintetizador. Las ediciones normales de knob se envían siempre
de inmediato.
