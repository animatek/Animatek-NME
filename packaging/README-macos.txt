Animatek NME - Nord Modular Editor G1 (macOS)
==============================================

This is a UNIVERSAL build: it runs natively on both Apple Silicon (M1/M2/M3/M4)
and Intel Macs. macOS 10.15 Catalina or newer.

IMPORTANT - FIRST LAUNCH
------------------------
The app is not notarized by Apple (no paid developer account), so macOS will
block the first launch. This is expected. To run it:

  1. Move AnimatekNME.app to your Applications folder.
  2. Open Terminal and run:

       xattr -cr /Applications/AnimatekNME.app

  3. Launch the app. If macOS still complains, right-click (Ctrl+click) the
     app > Open > Open. You only need to do this once.

If you ever see "AnimatekNME.app is damaged and can't be opened", that is
Gatekeeper quarantine, not real damage - step 2 above fixes it.

CONNECTING YOUR NORD MODULAR
----------------------------
Connect a MIDI interface to the synth's PC IN/OUT ports, open the app and
pick the MIDI ports under Device > MIDI Settings.

Animatek NME is free software (GPL-3). Source code:
https://github.com/animatek/Animatek-NME
Support the project: https://www.patreon.com/collection/2038913
