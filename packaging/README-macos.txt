Animatek NME - Nord Modular Editor G1 (macOS)
==============================================

This is a UNIVERSAL build: it runs natively on both Apple Silicon (M1/M2/M3/M4)
and Intel Macs. macOS 10.13 High Sierra or newer.

IMPORTANT - FIRST LAUNCH
------------------------
The app is not notarized by Apple (no paid developer account), so macOS will
block the first launch. This is expected. To run it:

  1. Move AnimatekNME.app to your Applications folder.
  2. Open Terminal and run:

       xattr -cr /Applications/AnimatekNME.app

  3. Launch the app. If macOS still complains:

       macOS 14 Sonoma and older: right-click (Ctrl+click) the app >
       Open > Open.

       macOS 15 Sequoia and newer: the right-click trick no longer works.
       Double-click the app once, let it be blocked, then go to System
       Settings > Privacy & Security, scroll to the bottom and click
       "Open Anyway" next to the AnimatekNME message.

     You only need to do this once.

If you ever see "AnimatekNME.app is damaged and can't be opened", that is
Gatekeeper quarantine, not real damage - step 2 above fixes it.

CONNECTING YOUR NORD MODULAR
----------------------------
Connect a MIDI interface to the synth's PC IN/OUT ports, open the app and
pick the MIDI ports under Device > MIDI Settings.

MANUAL / MANUAL DE USUARIO
--------------------------
The full user manual is online, in both languages:

    English:  https://animatek.net/animatek-nme-eng/manual/
    Espanol:  https://animatek.net/animatek-nme/manual/

It covers installation and connection, a tour of the interface, editing
patches, working with the four slots, the floating tools, file formats,
the complete keyboard shortcut reference and troubleshooting.

El manual de usuario completo esta en la web, en los dos idiomas, con
la instalacion y conexion, la interfaz, la edicion de patches, los cuatro
slots, las herramientas flotantes, los formatos de archivo, la referencia
de atajos y la resolucion de problemas.

Animatek NME is free software (GPL-3). Source code:
https://github.com/animatek/Animatek-NME
Support the project: https://www.patreon.com/collection/2038913
