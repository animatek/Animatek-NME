# 3. Editar patches

## Añadir módulos

- **Quick Add**: pulsa `Enter` o haz doble clic en canvas vacío, escribe unas
  letras y elige de los resultados ordenados por relevancia. Busca en nombres,
  categorías y en una tabla de etiquetas escrita a mano (prueba `reverb`,
  `random`, `snare`…).
- **Navegador de módulos**: recorre la paleta completa por categorías y arrastra
  módulos al canvas.

Las áreas Poly y Common aceptan conjuntos de módulos distintos, igual que el
hardware. Los módulos consumen recursos de DSP en el sintetizador; la barra de
estado sigue la carga.

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
(`Ctrl+Z` lo revierte) y funciona igual en una ventana de slot. El nombre vive
dentro del patch y llega al sintetizador con la siguiente subida completa.

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
- Clic derecho en un parámetro para asignarlo a un **grupo de morph**, a un
  **knob de hardware** o a un **controlador MIDI**, y para **bloquearlo** frente
  a la randomización.
- El módulo **DrumSynth** tiene un selector de presets local (esquina inferior
  derecha): clic derecho para guardar o gestionar tus propios presets de
  percusión.

## Morphs

Los cuatro grupos de morph de la cabecera funcionan como los del hardware:
asigna parámetros a un grupo (clic derecho → morph), fija el rango de morph de
cada parámetro, y mueve el knob del grupo para desplazarlos todos. Los controles
asignados de cualquier tipo (knobs, selectores 4-1, conmutadores, botones de
incremento y sliders) muestran el color de su grupo en el canvas, y el Inspector
lista todas las asignaciones de un módulo. Las superposiciones también los
visualizan: `F5` muestra los valores de morph, `F7` la pertenencia a grupos.

## Randomize, initialize y bloqueos

- `Ctrl+R` randomiza parámetros (uniforme); `Ctrl+Shift+R` usa una dispersión
  gaussiana alrededor de los valores actuales.
- Los parámetros bloqueados y los módulos excluidos no se tocan nunca.
- Initialize devuelve el patch a un estado limpio.

Para diseño de sonido evolutivo con cruce e interpolación, mira el
[Patch Mutator](05-tools-and-floaters.md#patch-mutator-ctrl8).

## Snapshots y variaciones

Los 8 botones de la cabecera guardan **variaciones del patch**: snapshots
completos de parámetros que puedes audicionar y alternar. Persisten en un
archivo `.var` junto al patch (el `.pch` en sí se mantiene 100% estándar). Las
ediciones en vivo se escriben sobre la variación activa.
