# 6. Archivos y formatos

## Archivos de patch `.pch`

Los patches son archivos `.pch` en el **formato de texto 3.0** estándar de Nord
Modular, compatibles con el editor original de Clavia y con Nomad/nmedit. Todo
lo que guardes en Animatek NME abre en los originales y viceversa.

- **Patches 2.10 antiguos** (el formato viejo `[Module N]`) se cargan de forma
  transparente, incluidos los cables encadenados y el enrutado correcto de
  salidas 1/2. Se marcan como **PCH2** en el navegador de presets, y el filtro
  **PCH2** del navegador los oculta cuando solo quieres los patches actuales. Al
  guardar se reescriben en formato 3.0.
- Las **notas de patch** se guardan en una sección `[Notes]`, una extensión de
  Nomad/nmedit que los editores originales ignoran sin causar problemas.

Abrir un patch pregunta a qué slot debe ir, o si cargarlo como **Local** (solo
en el editor, sin enviar nada al sintetizador). Mira
[Trabajar con el sintetizador](04-working-with-the-synth.md#abrir-un-patch-elegir-dónde-va).

## El archivo auxiliar `.var`

Las 8 variaciones de patch por slot (y las exclusiones de mutación por módulo)
viven en un archivo `.var` junto al patch: `MiPatch.pch` + `MiPatch.var`. Así el
`.pch` se mantiene estándar byte a byte. **Mantén los dos archivos juntos al
mover o hacer copia de seguridad de tus patches**: sin su archivo auxiliar, un
patch carga bien pero pierde sus variaciones.

## `.pchp` presets de módulo

Un preset de módulo es una instantánea con nombre de los parámetros de un tipo de
módulo, que se recupera desde la sección **Presets** del Inspector o desde el
menú contextual del módulo (mira
[Editar patches](03-editing-patches.md#presets-de-módulo)).

Los presets se guardan como un paquete `.pchp` por tipo de módulo en la carpeta
`Presets/` de la librería. El formato es texto plano y está pensado para editarse
a mano, ya que transcribir los presets del editor original se hace a mano. Los
valores se identifican por el nombre del parámetro y no por su posición, así que
un preset que nombra dos parámetros ajusta esos dos y deja el resto del módulo
como estaba. Los presets guardados por versiones anteriores a la 0.12 se migran
automáticamente al primer arranque.

## Snippets

Un snippet es un grupo reutilizable de módulos con sus cables y valores de
parámetro, guardado a partir de una selección e importado arrastrándolo. Los
snippets son archivos `.pch` normales guardados en la carpeta `Snippets/` de la
librería, así que funcionan en cualquier editor y se pueden compartir como
patches. Los módulos que no admiten duplicado (los singletons como Keyboard) se
filtran automáticamente al exportar.

## La librería de presets

El navegador de disco escanea recursivamente una carpeta configurable de
**librería de presets**:

```
<raíz de la librería>/
  Patches/    tus patches guardados (con la estructura de carpetas que quieras)
  Snippets/   snippets exportados
  Presets/    un paquete .pchp de presets por tipo de módulo
  Banks/      carpetas espejo Bank1 … Bank9 de "Backup All Banks"
```

La búsqueda cubre los nombres de archivo; los filtros acotan a patches, snippets
o copias de banco. Las copias de banco se cargan como cualquier otro patch.

## Carpetas de banco

**Save Bank to Disk** escribe los patches como `NN - Nombre.pch`; el prefijo
`NN` registra la posición dentro del banco, y **Send Bank to Synth** lo usa para
restaurar cada patch a su posición exacta.
