# 5. Herramientas y ventanas flotantes

Cinco ventanas flotantes viven en el menú View (o en `Ctrl+5`–`Ctrl+9`), más dos
extras al final del capítulo. Son ventanas normales: llévalas a un segundo
monitor, redimensiona las que lo permitan, y el editor recuerda dónde estaban.

## Knob Floater (`Ctrl+5`)

Una vista interactiva de los 18 knobs de hardware más pedal, conmutador y
aftertouch. Cada knob muestra su LED de asignación y el módulo y parámetro que
controla; los knobs son totalmente interactivos (edición + sincronización +
deshacer, morphs incluidos). Clic derecho en un knob para reasignarlo a un hueco
libre.

## Keyboard Floater (`Ctrl+6`)

Un teclado virtual con navegación por octavas para tocar el sintetizador sin
teclado MIDI. Dos modos de interpretación:

- **DRONE**: mantiene las notas hasta que se sueltan.
- **REPEAT**: pulsa la nota mantenida (Rate 100–500 ms, Gate 20–400 ms).

Las notas se envían por el protocolo del editor, así que funcionan por la misma
conexión USB/DIN que todo lo demás.

## Patch Notes (`Ctrl+7`)

Un bloc de notas monoespaciado y redimensionable ligado al patch del slot
activo. Las notas se guardan en la sección `[Notes]` del archivo `.pch` (una
extensión de Nomad/nmedit que los editores originales de Clavia ignoran sin
más). No hay archivos auxiliares.

## Patch Mutator (`Ctrl+8`)

Un criador de sonidos interactivo al estilo del G2. Un sonido **Mother** y otro
**Father** flanquean una fila de **Children**; desde ahí puedes:

- **Mutate**: variación gaussiana alrededor de un sonido (las afinaciones de
  oscilador se ajustan a intervalos musicales),
- **Randomize**: valores aleatorios nuevos,
- **Interpolate**: mezcla entre Mother y Father,
- **Cross**: cruce genético (en modo secuencial o independiente).

Haz clic en un sonido para audicionarlo en el sintetizador. Los parámetros
bloqueados, los módulos excluidos (clic derecho en un módulo → excluir de la
mutación) y los módulos de salida no se tocan nunca. Una fila de almacenamiento
temporal guarda favoritos, y la fila de variaciones enlaza con las 8 variaciones
por slot. El control por teclado es rápido; mira los
[atajos](07-shortcuts.md#patch-mutator-con-la-ventana-enfocada).

## SysEx Monitor (`Ctrl+9`)

Un registro hexadecimal en vivo de TX/RX de todo el tráfico MIDI entre editor y
sintetizador, la herramienta a la que recurrir cuando algo no sincroniza y
quieres ver por qué (o para adjuntarlo a un informe de error). Coste cero cuando
está cerrado; funciona en builds de release sin consola.

## Subventanas de slot

No es un flotante, pero es la otra manera de tener más de una cosa en pantalla:
los cuatro slots son subventanas en mosaico dentro de la ventana principal, cada
una con su canvas, su selección y su historial de deshacer.
`Ctrl+Shift+1`–`Ctrl+Shift+4` las muestra y las oculta (`Cmd+Alt+1`–`Cmd+Alt+4`
en macOS). Mira
[Trabajar con el sintetizador](04-working-with-the-synth.md#los-cuatro-slots-en-pantalla).

## El puente MCP

Un canal local opcional, desactivado por defecto, que permite a un asistente de
IA editar el patch a través del propio sistema de deshacer del editor. Mira
[El puente MCP](09-mcp-bridge.md).
