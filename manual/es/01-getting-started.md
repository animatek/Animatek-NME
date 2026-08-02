# 1. Primeros pasos

## Qué necesitas

- Un **Clavia Nord Modular G1** (teclado o rack) o un Micro Modular.
- Una interfaz MIDI conectada al MIDI IN **y** al MIDI OUT del sintetizador. El
  editor habla SysEx en las dos direcciones, así que una conexión de un solo
  sentido no basta.
- Los binarios de Animatek NME para tu plataforma, distribuidos a través de
  [Patreon](https://www.patreon.com/c/animatek).

## Instalación

**Linux.** Dos opciones:

- *AppImage*: dale permisos de ejecución y lánzalo con
  `chmod +x AnimatekNME-x.y.z-x86_64.AppImage && ./AnimatekNME-x.y.z-x86_64.AppImage`.
  El AppImage lo lleva todo dentro, incluido el backend MIDI parcheado (mira
  [Resolución de problemas](08-troubleshooting.md) si las builds estándar de tu
  distribución no muestran dispositivos MIDI).
- *Binario suelto*: descomprime y ejecuta `AnimatekNME`. No hay instalación; los
  ajustes se guardan en tu perfil de usuario.

**Windows.** Descomprime y ejecuta `AnimatekNME.exe`. No hace falta instalador.

**macOS.** Descomprime, mueve `AnimatekNME.app` a Aplicaciones y ábrelo. El
binario es universal (Apple Silicon + Intel). La primera vez puede que tengas
que autorizarlo en Ajustes del Sistema → Privacidad y seguridad.

## Conectar el sintetizador

1. Conecta el MIDI IN/OUT del sintetizador a tu interfaz y enciéndelo.
2. Abre Animatek NME. El editor escanea los puertos MIDI y hace el handshake del
   Nord Modular automáticamente; cuando encuentra el sintetizador, la barra de
   estado muestra la conexión y el editor descarga el patch del slot activo.
3. Si tienes varias interfaces MIDI, elige los puertos correctos en las opciones
   del editor.

Una vez conectado todo va en vivo: mover un knob en el editor cambia el sonido
al instante, y mover un knob en el panel frontal actualiza el editor.

## Tu primer patch

- Pulsa `Ctrl+1`–`Ctrl+4` para elegir slot (A–D).
- Pulsa `Enter` o haz doble clic en el canvas para abrir **Quick Add** y escribe
  el nombre de un módulo. Prueba `keyboard`, luego `oscA`, luego `2 outputs`.
- Arrastra cables entre los conectores de color: Keyboard *Note* → OscA *Pitch*,
  OscA *Out* → 2 Outputs *L*.
- Toca una nota (con tu teclado MIDI, o con el teclado virtual en `Ctrl+6`).
- Guarda con `Ctrl+S`. Los patches son archivos `.pch` estándar, compatibles con
  los editores originales.

## Dónde se guarda todo

El editor mantiene una carpeta de **librería de presets** (configurable desde el
navegador de presets) con subcarpetas `Patches/`, `Snippets/`, `Presets/` y
`Banks/`. Los backups de bancos, los snippets, los presets de módulo y tus
patches guardados acaban ahí, y los patches aparecen en el navegador integrado
(`Ctrl+B`).
