# 8. Resolución de problemas

## El editor no encuentra el sintetizador

- Comprueba que hay cable en las **dos** direcciones: OUT del sintetizador → IN
  de la interfaz, y OUT de la interfaz → IN del sintetizador. El handshake
  necesita respuesta.
- Asegúrate de que ninguna otra aplicación (una DAW, el editor original) tiene
  ocupados los puertos MIDI.
- Verifica la selección de puertos en las opciones del editor si tienes varias
  interfaces.
- El protocolo del Nord Modular tiene un timeout de respuesta de 3 segundos; si
  la conexión se cae a media sesión, la barra de estado lo indica y el editor
  sigue reintentando el handshake.

## Linux: no aparece ningún dispositivo MIDI

Los kernels de Linux modernos exponen MIDI a través de la nueva capa UMP, que
las builds estándar de JUCE manejan incorrectamente (no encuentran dispositivos,
o mandan paquetes UMP a interfaces legacy). Animatek NME lleva un backend MIDI
parcheado que soporta ambas; usa el AppImage o los binarios oficiales. Si
compilas desde el código fuente, los parches de JUCE necesarios están en el
submódulo `JUCE/` incluido (mira la *Linux MIDI Note* del README).

## Un patch carga con módulos ausentes o cables equivocados

Asegúrate de estar en la versión actual; la serie 0.8.x arregló varios fallos de
decodificación de patches (cables 2.10 antiguos, cables encadenados descargados
del sintetizador, datos de módulo personalizados llegando desordenados). Si un
archivo concreto sigue fallando, abre un issue en GitHub y adjunta el `.pch`.

## Un slot muestra la insignia LOCAL

El patch de ese slot existe solo en el editor; se abrió con la opción **Local**,
o se cargó o construyó sin conexión, así que el sintetizador no lo tiene. Súbelo
(ábrelo en un slot A–D, o almacénalo en un banco) y la insignia desaparece.

## Mi asistente de IA no llega al editor

El puente MCP está **desactivado por defecto**. Actívalo en Editor Options
(`Ctrl+,`) → MCP Bridge y comprueba que la línea de estado dice que está
escuchando; el editor tiene que estar abierto para que las herramientas
funcionen. Mira [El puente MCP](09-mcp-bridge.md).

## Notas colgadas

Si se quedan notas sonando (normalmente por MIDI externo yendo directo al
sintetizador), usa el pánico del panel frontal. El teclado virtual del editor
siempre empareja cada note-on con su note-off.

## Han desaparecido mis variaciones

Las variaciones viven en el archivo auxiliar `.var` junto al `.pch`. Si moviste
o renombraste el archivo de patch, mueve o renombra su `.var` con él.

## Un atajo de teclado no hace nada

Si un atajo no responde y los de al lado sí, hay algo fuera del editor que se
queda con esa combinación antes. Un programa que registra un atajo para todo el
sistema lo recibe antes que cualquier aplicación, y al editor no le llega nada.

La pista es que el fallo sea selectivo: `Ctrl+Shift+I` y `Ctrl+Shift+R` no hacen
nada mientras `Ctrl+Shift+Z` y `Ctrl+Shift+1`-`4` van bien, y `Ctrl+I` y `Ctrl+R`
a secas también. Ni un teclado averiado ni un fallo del editor elegirían de esa
manera.

En Windows el sospechoso habitual es **AMD Software: Adrenalin Edition**, que se
reserva varias combinaciones con `Ctrl+Shift` de fábrica. Desactiva en su propia
configuración las que quieras recuperar y el editor volverá a recibirlas. El
propio Windows también usa `Ctrl+Shift` para cambiar de distribución de teclado
cuando hay más de un idioma instalado, en **Configuración > Hora e idioma >
Escritura > Configuración avanzada del teclado > Teclas de acceso rápido del
idioma de entrada**. Los grabadores de pantalla, las superposiciones de juego y
los paneles de las tarjetas gráficas merecen la misma revisión.

Todo lo que tiene atajo está también en un menú, así que nada queda fuera de tu
alcance mientras lo localizas.
