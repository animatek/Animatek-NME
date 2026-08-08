# 7. Atajos de teclado

También disponibles dentro del editor en **Help → Keyboard Shortcuts...**

En macOS, `Ctrl` es `Cmd`.

## Archivo

| Atajo | Acción |
|-------|--------|
| `Ctrl+N` | Patch nuevo |
| `Ctrl+O` | Abrir patch |
| `Ctrl+S` | Guardar |
| `Ctrl+Shift+S` | Guardar como |
| `Ctrl+B` | Navegador de presets |
| `Ctrl+P` | Patch settings |
| `Ctrl+G` | Synth settings |
| `Ctrl+,` | Editor options |
| `Ctrl+Q` | Salir |

## Edición

| Atajo | Acción |
|-------|--------|
| `Ctrl+Z` | Deshacer |
| `Ctrl+Shift+Z`, `Ctrl+Y` | Rehacer |
| `Ctrl+A` | Seleccionar todos los módulos de la sección |
| `Ctrl+X` / `Ctrl+C` / `Ctrl+V` | Cortar / copiar / pegar módulos |
| `Ctrl+D` | Duplicar la selección con cables |
| `Delete`, `Backspace` | Borrar la selección |
| `Escape` | Limpiar la selección |
| Flechas | Desplazar los módulos seleccionados una celda de rejilla |
| `Ctrl+R` | Randomizar parámetros |
| `Ctrl+Shift+R` | Randomizar parámetros (gaussiano) |

## Canvas

| Atajo | Acción |
|-------|--------|
| `Enter`, doble clic | Quick Add en la posición del ratón |
| `F1` | Ayuda del módulo bajo el cursor o seleccionado |
| `F5` | Superposición de valores de parámetro de todo el patch (los que tienen morph muestran su recorrido) |
| cursor encima | Deja el cursor sobre un control para leer su valor |
| `F7` | Superposición de grupos de morph |
| `F8` | Superposición de asignaciones de knobs |
| `F9` | Superposición de asignaciones de MIDI CC |
| `F10` | Superposición del coste de DSP de cada módulo |
| doble clic | Doble clic en un módulo para leer su coste de DSP |
| `Z` | Zoom a la selección (o reset si no hay nada seleccionado) |
| `Shift+Z` | Reset del zoom al 100% |
| `Ctrl++` / `Ctrl+-` | Acercar / alejar |
| `Ctrl+T` | Siguiente tema de color |
| `Ctrl+W` | Módulos en wireframe |
| `Ctrl+I` | Mostrar u ocultar el Inspector (columna izquierda) |
| `Ctrl+Shift+I` | Mostrar u ocultar el navegador de patches (columna derecha) |
| `S` | Sacudir cables |
| Arrastre con botón central | Desplazar el canvas |

## Slots

| Atajo | Acción |
|-------|--------|
| `Ctrl+1`..`Ctrl+4` | Cambiar al slot A..D (abre su subventana si estaba cerrada) |
| `Ctrl+Shift+1`..`Ctrl+Shift+4` | Mostrar u ocultar la subventana del slot A..D (**en macOS: `Cmd+Alt+1`..`Cmd+Alt+4`**, porque macOS se reserva `Cmd+Shift+3` y `Cmd+Shift+4` para sus capturas de pantalla) |
| `F11` | Modo foco: agranda el slot con foco a toda el área, y lo devuelve |
| Botón de maximizar | Lo mismo, en la barra de título de esa subventana |
| `Ctrl+Shift+` flechas | Mueve el slot con foco a la casilla vecina, intercambiándolo con lo que hubiera. Arriba y abajo solo existen en el 2x2 de cuatro slots; en un borde no pasa nada |
| Clic derecho en una fila de slot | Mostrar u ocultar la subventana de ese slot |
| Clic derecho en un patch del navegador del sinte | **Load to Slot A..D**: traerlo a un slot concreto |
| `Ctrl+click` en una fila de slot | Activar o desactivar el slot sin seleccionarlo |

Los slots abiertos se reparten el espacio solos, como hace un gestor de ventanas
en mosaico: uno ocupa toda el área de trabajo, dos la parten por la mitad, tres
van en tercios, cuatro en 2x2. La distribución se rehace al abrir o cerrar
cualquiera. Arrastrar o redimensionar una subventana deja las ventanas donde las
pongas a partir de entonces; **View > Slots > Tile Slots** las devuelve al
mosaico.

## Ventanas flotantes

| Atajo | Acción |
|-------|--------|
| `Ctrl+5` | Knob Floater |
| `Ctrl+6` | Keyboard Floater |
| `Ctrl+7` | Patch Notes |
| `Ctrl+8` | Patch Mutator |
| `Ctrl+9` | SysEx Monitor |

## Subventana (la que tiene el foco)

Todos estos actúan sobre el slot y la selección propios de esa subventana.

| Atajo | Acción |
|-------|--------|
| `Ctrl+R` / `Ctrl+Shift+R` | Randomizar parámetros (uniforme / gaussiano) |
| `Ctrl+S` / `Ctrl+Shift+S` | Guardar / guardar como |

## Patch Mutator (con la ventana enfocada)

| Atajo | Acción |
|-------|--------|
| `1`-`8` | Enfocar Mother / Children / Father |
| `O` / `T` | Copiar el sonido enfocado a Mother / Father |
| `E` / `U` | Mutar desde el enfocado / desde Mother |
| `N` | Randomizar |
| `I` / `X` | Interpolar / cruzar (Mother + Father) |
| `S` | Guardar el sonido enfocado en el almacenamiento temporal |
| Shift+arrastre | Interpolar dos sonidos |
| Ctrl+arrastre | Cruzar dos sonidos |
