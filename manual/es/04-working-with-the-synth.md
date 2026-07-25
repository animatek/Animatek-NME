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

## Ventanas de slot

**Clic derecho en una fila de slot** para abrir el patch de ese slot en su
propia ventana. Así se trabaja con dos o más patches en paralelo; la ventana
principal mantiene sus slots A–D funcionando exactamente igual que antes.

Dentro de una ventana de slot todo es independiente: canvas, módulos, cables,
parámetros, asignación de morph/knob/MIDI-CC, renombrado de módulos y su propio
historial de deshacer. Las ediciones caen en el slot correcto aunque no tenga el
foco del hardware.

- La ventana sigue al sintetizador en vivo: girar un knob físico del panel
  frontal, o el movimiento de una luz o un medidor, anima también la ventana de
  ese slot.
- `Ctrl+R` / `Ctrl+Shift+R` randomizan (uniforme / gaussiano) y `Ctrl+S` /
  `Ctrl+Shift+S` guardan y guardan como, actuando sobre el slot **de esa
  ventana** y respetando su propia selección de módulos.
- `Ctrl+I`, o la tira de flecha fina en el borde izquierdo del canvas, oculta el
  Inspector para darle al canvas todo el ancho de la ventana.
- Cuando el foco del panel frontal pasa a un slot que tiene ventana abierta, esa
  ventana pasa al frente y su título gana **"- Focused"**, replicando la barra
  de título resaltada del Nomad original.

La barra de ajustes superior (macros, medidores de CPU y voces) se queda solo en
la ventana principal, igual que en el editor original.

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
- **cargar** un patch en un slot,
- **almacenar** el patch actual en una posición de banco,
- **copiar, mover y borrar** patches dentro de la memoria del sintetizador.

Los archivos Nord Modular 2.10 antiguos se marcan como **PCH2** en el navegador
de disco y se cargan de forma transparente.

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
