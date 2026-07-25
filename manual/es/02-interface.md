# 2. La interfaz

## La ventana principal

De arriba abajo:

- **Barra de menús**: File, Edit, View, Device y Help. Device tiene las
  herramientas que hablan con el sintetizador (transferencia de bancos,
  controller snapshot); View tiene las ventanas flotantes.
- **Cabecera**: el nombre del patch actual (clic para renombrar), el contador
  **Voices** con sus flechas, los medidores **Load**, los cuatro **knobs de
  morph** y los **botones de variación** (8 por slot).
- **Inspector** (columna izquierda): todo lo del módulo seleccionado que no está
  en su cara visible: su nombre (editable, y deshacible), su sección, y sus
  **asignaciones** (grupos de morph, knobs de hardware y MIDI CCs). Junto al
  título de asignaciones hay un **mapa de knobs de hardware**: un diagrama de
  cuatro paneles y 18 LEDs que replica la disposición física, con los knobs
  asignados en verde vivo y los libres en el color apagado de la lente del
  hardware. Sin nada seleccionado, lista las asignaciones del patch entero.
- **Barra de slots** (bajo el Inspector): los cuatro slots de hardware A–D con
  LEDs al estilo del aparato: un LED *parpadeando* es el slot seleccionado, los
  LEDs *fijos* son los slots activos. El clic normal selecciona un slot;
  `Ctrl+click` activa o desactiva un slot sin seleccionarlo, igual que
  `Shift+botón de slot` en el panel frontal; **el clic derecho saca el slot a su
  propia ventana** (mira
  [Trabajar con el sintetizador](04-working-with-the-synth.md#ventanas-de-slot)).
  Un slot con la insignia **LOCAL** tiene un patch que el sintetizador no conoce.
- **Canvas de patch**: el corazón del editor, dividido en **área Poly** (una
  instancia por voz) y **área Common** (una por patch: teclado, secuenciadores,
  efectos, salidas). Un separador arrastrable las divide. El arrastre con el
  botón central hace pan, `Ctrl++`/`Ctrl+-` hace zoom, y `Z` hace zoom a la
  selección.
- **Navegador de presets** (derecha, `Ctrl+B`): dos mundos en un mismo panel, la
  memoria interna del sintetizador (9 bancos) y tu librería en disco, con
  búsqueda y filtros de patch, snippet y banco.
- **Barra de estado**: estado de conexión, información del sintetizador y
  actividad.

## Voces y carga de DSP

El campo **Voices** de la cabecera fija la polifonía del patch; las flechas la
cambian y el patch se vuelve a subir para que el sintetizador siga (el G1 guarda
el número de voces dentro de la cabecera del patch, así que esa resubida *es* el
cambio). Mantener las flechas pulsadas es seguro; las pulsaciones rápidas se
agrupan en una sola subida con el valor final. El mismo ajuste está también en
Patch Settings (`Ctrl+P`).

Al lado, dos barras **Load** muestran el coste de DSP del patch con un decimal,
como hacía el editor original: `PVA:` para el área poly/voz y `E:` para el área
común (efectos). La cifra es una estimación propia del editor a partir del coste
en ciclos de cada módulo; el sintetizador no informa de su carga.

## Módulos en el canvas

Cada módulo está dibujado fiel al píxel respecto al editor original: knobs,
botones, selectores, displays, conectores y luces. Los datos en vivo del
sintetizador animan los **medidores VU y los LEDs** en tiempo real mientras hay
conexión.

- Clic y arrastre en un knob para cambiarlo (el sintetizador sigue al instante).
- Clic derecho en un knob para las opciones del parámetro (asignación de morph,
  MIDI CC, knob de hardware, bloqueos).
- Arrastra un módulo por su título o cuerpo para moverlo por la rejilla;
  multi-selección con banda elástica o `Shift`-clic.
- `F1` muestra la ayuda del módulo bajo el cursor, directamente de la
  documentación original del Nord Modular.

## Colores de cable

Los cables toman el color de la señal que llevan:

| Color | Señal |
|-------|-------|
| Rojo | Audio |
| Azul | Control |
| Amarillo | Lógica / gate |
| Gris | Master/slave (grupos de sync de osciladores) |
| Verde / morado | Cables recoloreados por el usuario |
| Blanco | Desconocido |

El menú View y las herramientas de la cabecera permiten ocultar colores de cable
de forma selectiva, cambiar el estilo de cable (curvo/recto, grueso/fino) y su
opacidad, y `S` "sacude" los cables para que los tramos solapados se
redistribuyan.

## Temas

`Ctrl+T` rota entre los 13 temas de color, o elige uno en **View → Theme** (la
marca de verificación sigue siempre al tema realmente en uso). **Nord** es el
tema por defecto; **Nord Classic** es un tema claro de grises cálidos que remite
al editor original de Clavia: cuerpos de módulo en gris plano, etiquetas negras,
lecturas de valor en índigo tipo LCD y los colores de cable clásicos, todo
muestreado del original. El tema con los colores propios de Nomad se llama
**Nomad**.

Los temas se aplican a toda la aplicación, no solo al canvas: barra de menús,
cabecera, barra de estado, lista de slots, Inspector y todos los diálogos siguen
la paleta, así que el texto sigue siendo legible tanto en temas claros como
oscuros. Un grano procedural muy sutil sobre el canvas le da textura de papel en
lugar de un relleno plano, más visible en Nord Classic.

`Ctrl+W` alterna un estilo de módulo en wireframe que funciona con todos los
temas. Ambos ajustes persisten entre sesiones, igual que el tamaño y posición de
ventana y la disposición de los flotantes.
