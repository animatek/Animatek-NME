# 2. La interfaz

## La ventana principal

De arriba abajo:

- **Barra de menús**: File, Edit, View, Device, Help y About. Device tiene las
  herramientas que hablan con el sintetizador (transferencia de bancos,
  controller snapshot); View tiene las ventanas flotantes. Cuando sale una
  versión nueva de NME, **About** lleva un punto rojo y su primera opción,
  *Version X.Y.Z is out*, abre Patreon, que es donde están las descargas. NME
  lo mira como mucho una vez al día al arrancar y nunca descarga nada por su
  cuenta; en Editor Options se puede desactivar.
- **Cabecera**: el nombre del patch actual (clic para renombrar), el contador
  **Voices** con sus flechas, los medidores **Load**, los cuatro **knobs de
  morph** y los **botones de variación** (8 por slot).
- **Inspector** (columna izquierda): todo lo del módulo seleccionado que no está
  en su cara visible: su nombre (editable, y deshacible), su sección, sus
  **asignaciones** (grupos de morph, knobs de hardware y MIDI CCs), su **coste
  de DSP** y sus **presets**. Junto al título de asignaciones hay un **mapa de
  knobs de hardware**: un diagrama de cuatro paneles y 18 LEDs que replica la
  disposición física, con los knobs asignados en verde vivo y los libres en el
  color apagado de la lente del hardware. Sin nada seleccionado, lista las
  asignaciones del patch entero. `Ctrl+I` pliega la columna entera.
- **Barra de slots** (bajo el Inspector): los cuatro slots de hardware A–D con
  LEDs al estilo del aparato: un LED *parpadeando* es el slot seleccionado, los
  LEDs *fijos* son los slots activos. El clic normal selecciona un slot;
  `Ctrl+click` activa o desactiva un slot sin seleccionarlo, igual que
  `Shift+botón de slot` en el panel frontal; **el clic derecho muestra u oculta
  la subventana de ese slot** (mira
  [Trabajar con el sintetizador](04-working-with-the-synth.md#los-cuatro-slots-en-pantalla)).
  Un slot con la insignia **LOCAL** tiene un patch que el sintetizador no conoce.
- **Barra de módulos** (bajo la cabecera, a todo lo ancho): la paleta de módulos
  tal como la presenta el editor original. Las pestañas eligen categoría (In/Out,
  Osc, LFO, Env, Filter, Mixer, Audio, Ctrl, Logic, Seq) y debajo aparecen los
  módulos de esa categoría, cada uno en una cajita fina con su nombre.
  **Arrastra una cajita** al área de patch, o **haz clic** y el módulo sigue al
  puntero hasta que pulses donde lo quieras, lo que elige también el área y el
  slot. La barra recuerda la pestaña en la que la dejaste, y
  **View > Module Icon Bar** la quita si prefieres esos píxeles para otra cosa.
- **Área de trabajo**: los cuatro slots viven aquí como **subventanas**,
  distribuidas en mosaico dentro de la ventana principal. Cada una contiene un
  **canvas de patch**, dividido en **área Poly** (una instancia por voz) y **área
  Common** (una por patch: teclado, secuenciadores, efectos, salidas). Un
  separador arrastrable las divide. El arrastre con el botón central hace pan,
  `Ctrl++`/`Ctrl+-` hace zoom, y `Z` hace zoom a la selección.
- **Navegador de presets** (derecha, `Ctrl+B`): dos mundos en un mismo panel, la
  memoria interna del sintetizador (9 bancos) y tu librería en disco, con
  búsqueda y filtros de patch, snippet y banco. `Ctrl+Shift+I` lo pliega.
- **Barra de estado**: estado de conexión, información del sintetizador y
  actividad.

El Inspector, la cabecera, los navegadores y la barra de estado son compartidos:
siguen a la subventana que tenga el foco, y esa subventana lleva un borde del
color de texto del tema para que siempre veas qué slot estás editando.

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

El coste individual de cada módulo aparece allí donde eliges o inspeccionas un
módulo: en el menú contextual **Add Module** ("Audio In (2.2%)"), en el navegador
de módulos, en cada fila de Quick Add y en el Inspector. **Doble clic en un
módulo** para ver su coste, como hace el editor original, o pulsa `F10` para
etiquetarlos todos a la vez, que es la vista que quieres cuando un patch se pasa
de presupuesto y buscas qué recortar. Las cifras están redondeadas a las dos
cifras significativas que imprime el editor original de Clavia, así que un patch
optimizado contra el editor de hardware muestra aquí los mismos números.

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
- **Deja el cursor sobre cualquier control** para leer su valor en las unidades
  del propio parámetro, de forma que un cutoff se lee "440 Hz" y no "64".
  Arrástralo y el valor sigue el movimiento en vivo.
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

En una pantalla 4K el texto puede quedar demasiado pequeño. **UI size** en Editor
Options (del 100 % al 300 %) escala toda la interfaz: ventanas, menús, diálogos y la
ayuda de módulos (`F1`). Se aplica al pulsar OK y las ventanas crecen con ella; en 4K,
un 200 % da el espacio de una pantalla 1080p con el texto nítido.

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

### Tus propios temas

**View → Theme → Edit Theme Colours...** abre una ventana con una muestra por
cada color, agrupadas (LEDs y medidores, cables y jacks, knobs de morph,
secuenciadores, knobs, módulos y canvas, aplicación). El canvas cambia mientras
arrastras un color; **Save** lo guarda como tema propio, y cerrar sin guardar
devuelve el tema elegido. Si marcas **Save the render style with the theme**,
el tema también enciende o apaga Wireframe, Hardware look y Flat knobs tal como
están en pantalla cada vez que se elige.

Los temas son archivos JSON pequeños en una carpeta `themes` junto a los ajustes
(**View → Theme → Open Themes Folder**). **Export Current Theme...** guarda el
tema en uso en un archivo que puedes pasar a otra persona, e **Import Theme
File...** instala uno: primero lo comprueba, luego lo copia a tu carpeta de
temas y lo elige. Un tema tuyo con el mismo nombre solo se sustituye si lo
confirmas.

**View → Theme → Community Themes...** muestra los temas que la gente comparte
en [animatek/nme-themes](https://github.com/animatek/nme-themes) en GitHub, con
la marca **New** en los que no habías visto y **Update** en los que han cambiado
desde que los instalaste. Un clic instala el tema y lo pone. NME mira esa lista
como mucho una vez al día al arrancar, y lo avisa en la barra de estado cuando
hay algo nuevo (un punto en View y en Theme, y la opción dice cuántos); **Check Now** mira en el
momento. En Editor Options se puede desactivar la comprobación diaria.
Cualquiera puede añadir un tema allí con un pull request: el README del repo
explica cómo.

Un tema puede traer su estilo de render. **Animatek Rack** enciende Flat knobs
al elegirlo; al elegir un tema que no trae ninguno vuelves al estilo que tenías
antes. Mientras estás en ese tema, Editor Options y los atajos lo siguen
cambiando como siempre.
