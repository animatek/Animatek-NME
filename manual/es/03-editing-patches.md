# 3. Editar patches

## Añadir módulos

- **Quick Add**: pulsa `Enter` o haz doble clic en canvas vacío, escribe unas
  letras y elige de los resultados ordenados por relevancia. Busca en nombres,
  categorías y en una tabla de etiquetas escrita a mano (prueba `reverb`,
  `random`, `snare`…).
- **Navegador de módulos**: recorre la paleta completa por categorías y arrastra
  módulos al canvas, a la subventana sobre la que los sueltes.
- **Add Module**: clic derecho en canvas vacío para el menú completo por
  categorías.

Las áreas Poly y Common aceptan conjuntos de módulos distintos, igual que el
hardware. Los módulos consumen recursos de DSP en el sintetizador, y las tres
vías imprimen el coste del módulo junto a su nombre ("Audio In (2.2%)") para que
elijas con el presupuesto a la vista. Los medidores Load de la cabecera siguen el
total del patch; mira
[Voces y carga de DSP](02-interface.md#voces-y-carga-de-dsp).

## Selección y organización

- El clic selecciona; `Shift`-clic y la banda elástica amplían la selección;
  `Ctrl+A` selecciona la sección entera; `Escape` limpia.
- Arrastra para mover (la rejilla lo mantiene todo alineado); las flechas
  desplazan una celda.
- `Ctrl+X`/`Ctrl+C`/`Ctrl+V` cortan, copian y pegan; `Ctrl+D` duplica **con
  cables**.
- `Delete` borra la selección, cables incluidos. Todo es deshacible; cada slot
  tiene su propio historial (`Ctrl+Z` / `Ctrl+Shift+Z`).

## Renombrar módulos

Ponle a un módulo tu propio nombre desde su menú contextual, o desde el campo
**Name** arriba del Inspector. Renombrar es una edición normal y deshacible
(`Ctrl+Z` lo revierte). El nombre vive dentro del patch y llega al sintetizador
con la siguiente subida completa.

## Reemplazar un módulo

Para probar un FilterD donde tienes un FilterE, haz clic derecho en el módulo y
abre **Replace with**. Salen los módulos de su misma familia, cada uno con su
consumo de DSP y, si es el caso, cuántos cables perdería. El nuevo ocupa el
sitio del viejo en un solo paso deshacible (`Ctrl+Z` devuelve el anterior tal
como estaba).

Qué se conserva:

- **La posición**, y **el nombre** si le habías puesto uno.
- **Los cables** en conectores que también tiene el módulo nuevo, emparejados
  por nombre. Si los nombres no coinciden, un cable solo pasa cuando no hay
  nada que elegir: un FilterD cableado desde su salida `lp` conserva ese cable
  en la única `out` de un FilterE.
- **Los valores** de los parámetros con el mismo nombre y rango, como la
  resonancia.
- **Las asignaciones** de knobs, morph y CC MIDI sobre esos parámetros.

Lo demás se suelta en vez de adivinarse. El cable del `sync` de un OscA no acaba
en la entrada de FM de un OscB solo porque las dos sean entradas de audio. La
barra de estado dice cuántos cables y asignaciones se han perdido. Un módulo más
alto empuja hacia abajo los de su columna; si no cabe, el reemplazo se rechaza y
no cambia nada.

## Cables

- **Crear**: arrastra de un conector a otro compatible. Los destinos válidos se
  iluminan mientras arrastras; las salidas conectan a entradas.
- **Cables encadenados**: también puedes arrastrar de una *entrada* a otra
  *entrada*, encadenando una red igual que en el editor original, p. ej.
  Keyboard Note → OscA1 Pitch, y luego OscA1 Pitch → OscA2 Pitch. La regla del
  hardware se respeta: una red solo puede estar alimentada por **una** salida, y
  los destinos ilegales no se iluminan.
- **Borrar**: clic derecho en un conector para quitar sus cables.
- Los filtros de visibilidad, los estilos y la sacudida con `S` ayudan a
  desenredar patches grandes.

## Parámetros

- Knobs, sliders, botones y selectores se editan en vivo y se sincronizan con el
  sintetizador.
- **Deja el cursor** sobre cualquier control para leer su valor en las unidades
  del parámetro; arrástralo y la lectura sigue el movimiento en vivo.
- Clic derecho en un parámetro para asignarlo a un **grupo de morph**, a un
  **knob de hardware** o a un **controlador MIDI**, y para **bloquearlo** frente
  a la randomización.
- **Haz clic en una pantalla de frecuencia** de un oscilador, un LFO esclavo o
  un filtro y cambia las unidades en que lee, como en el editor original. Una
  frecuencia absoluta alterna entre hercios y la nota en la que cae; la de un
  oscilador esclavo recorre la relación de parciales, el intervalo en semitonos
  y la frecuencia a la que lo lleva realmente su maestro. Al pasar el ratón se
  muestran las unidades que la caja *no* está enseñando, así que un
  suboscilador se puede poner en `-12(Oct)` y comprobarlo en hercios sin tocar
  nada. La elección es de cada módulo y se guarda con el patch.

## Leer un patch: las teclas de superposición

Cinco teclas de función etiquetan el patch entero de una vez. Son conmutadores,
así que puedes dejar una lectura abierta mientras trabajas en vez de mantener la
tecla pulsada.

| Tecla | Qué etiqueta |
|-------|--------------|
| `F5` | El valor de cada parámetro. Un parámetro con morph muestra el recorrido que barre el morph, p. ej. "46Hz-2.30kHz" |
| `F7` | Pertenencia a grupos de morph |
| `F8` | Asignaciones de knobs de hardware |
| `F9` | Asignaciones de MIDI CC |
| `F10` | El coste de DSP de cada módulo |

Las cinco están también en **View > Overlays**, marcadas para que veas cuál está
abierta, con **None** para cerrarla.

## Presets de módulo

Selecciona cualquier módulo y el Inspector añade una sección **Presets** bajo sus
asignaciones: clic en un nombre para recuperarlo, la **x** para borrarlo, clic
derecho para renombrar, y **+ Save current settings** para capturar el módulo tal
como está. La sección se pliega desde el chevron de su título, y la misma lista
está en el menú contextual del propio módulo.

Recuperar un preset es un único paso de deshacer, no uno por parámetro. Un preset
no es más que una instantánea con nombre de los parámetros de un tipo de módulo,
así que cualquier módulo puede tenerlos: secuenciadores, filtros, el DrumSynth,
lo que sea. Viven en una carpeta **Presets** de tu librería, como un paquete
`.pchp` por tipo de módulo; mira
[Archivos y formatos](06-files-and-formats.md#pchp-presets-de-módulo).

### Escalas del Key Quantizer

El menú contextual del Key Quantizer tiene un submenú **Scale**. Arriba dice
qué escala forman ahora sus interruptores de nota; debajo están **Chromatic** y
una lista por tónica, de **C** a **B**. Elige **D > D Dorian** y los doce
interruptores quedan puestos para esa escala. Los interruptores no dicen cuál
es la tónica, así que salen marcadas todas las escalas con esas mismas notas:
las teclas blancas son a la vez Do mayor, La menor natural y Re dórico. Si las
notas no forman ninguna escala, pone **Custom notes**.

Además de las escalas mayores y menores, los modos, las pentatónicas, las de
blues y la de tonos enteros, hay frigia dominante (andaluza), doble armónica,
menor húngara, napolitanas menor y mayor, hirajoshi, in-sen y las dos
disminuidas.

Elegir una escala cambia los interruptores uno a uno, así que `Ctrl+Z` la
deshace interruptor a interruptor.

## Morphs

Los cuatro grupos de morph de la cabecera funcionan como los del hardware:
asigna parámetros a un grupo (clic derecho → morph), fija el rango de morph de
cada parámetro, y mueve el knob del grupo para desplazarlos todos. Los controles
asignados de cualquier tipo (knobs, selectores 4-1, conmutadores, botones de
incremento y sliders) muestran el color de su grupo en el canvas, y el Inspector
lista todas las asignaciones de un módulo. `F7` etiqueta la pertenencia a grupos
en todo el patch y `F5` muestra el recorrido que barre cada parámetro con morph.

## Randomize, initialize y bloqueos

- `Ctrl+R` randomiza parámetros (uniforme); `Ctrl+Shift+R` usa una dispersión
  gaussiana alrededor del centro del rango de cada parámetro. Si hay módulos
  seleccionados, solo randomiza esos.
- **Excluir módulos.** Haz clic derecho en un módulo y marca **Exclude from
  Random & Mutation**, o selecciona varios y hazlo desde el menú contextual de la
  selección. Los módulos excluidos llevan un marco rojo, y `Ctrl+R`,
  `Ctrl+Shift+R`, el Patch Mutator y `mutate_patch` del puente MCP no los
  tocan. La barra de estado dice cuántos se han saltado.
- Tampoco se tocan los parámetros bloqueados, ni los interruptores on/off, mutes,
  niveles o bypass, para que randomizar no deje el patch en silencio.
- Initialize devuelve el patch a un estado limpio.

Para diseño de sonido evolutivo con cruce e interpolación, mira el
[Patch Mutator](05-tools-and-floaters.md#patch-mutator-ctrl8).

## Snapshots y variaciones

Los 8 botones de la cabecera guardan **variaciones del patch**: snapshots
completos de parámetros que puedes audicionar y alternar. Persisten en un
archivo `.var` junto al patch (el `.pch` en sí se mantiene 100% estándar). Las
ediciones en vivo se escriben sobre la variación activa.
