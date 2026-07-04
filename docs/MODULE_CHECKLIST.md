# Module Visual & Functional Checklist

Status guide:
- `[ ]` Not reviewed
- `[~]` Reviewed, has issues (details noted)
- `[x]` Reviewed, looks correct

For each module, check:
- **Layout**: Correct size, connectors/knobs/sliders in right positions
- **Graphics**: Custom displays rendering correctly (envelopes, waveforms, filters, etc.)
- **Buttons**: Waveform selectors and radio buttons showing correct icons
- **LEDs**: Signal indicator LEDs lighting up (green/yellow/red based on signal)
- **Meters**: VU/level meters showing activity
- **Labels**: Text labels and value displays correct
- **Parameters**: All knobs/sliders/buttons interactive and sending correct values

---

## In/Out (10)

| # | Module | Layout | Graphics | LEDs/Meters | Buttons | Notes |
|---|--------|--------|----------|-------------|---------|-------|
| 1 | Keyboard | [x] | [x] | [x] | [x] | Outputs=cuadrados, inputs=círculos, label Rel/vel multilínea centrado |
| 2 | AudioIn | [x] | [x] | [x] | [x] | VU meters animados (verde/amarillo/rojo), LEDs clip rojos activados por ledOnValue=127 |
| 3 | 4Output | [x] | [x] | [x] | [x] | OK |
| 4 | 2Output | [x] | [x] | [x] | [x] | Botones Destination y Mute con bevel OK |
| 5 | 1Output | [x] | [x] | [x] | [x] | Botones Dest y Mute con bevel OK |
| 63 | KeyboardPatch | [x] | [x] | [x] | [x] | Etiquetas multilínea posición corregida |
| 65 | MIDIGlobal | [x] | [x] | [x] | [x] | OK |
| 67 | NoteDetect | [x] | [x] | [x] | [x] | Display muestra nombre de nota (C4, D#3…) |
| 100 | KeybSplit | [x] | [x] | [x] | [x] | Lower y Upper muestran nombre de nota |
| 127 | PolyAreaIn | [x] | [x] | [x] | [x] | Labels L/R corregidas, escala dB entre VU en negro |

## Oscillator (16)

| # | Module | Layout | Graphics | LEDs/Meters | Buttons | Notes |
|---|--------|--------|----------|-------------|---------|-------|
| 7 | OscA | [x] | [x] | [x] | [x] | Waveform selector (sine/tri/saw/square icons), TextDisplay nota, mute, static icon |
| 8 | OscB | [x] | [x] | [x] | [x] | Igual que OscA, wf_saw_inv icon añadido |
| 9 | OscC | [x] | [x] | [x] | [x] | Sine only slave, badge wf_sine x=237 y=5, connector colors corregidos |
| 10 | OscSlvB | [x] | [x] | [x] | [x] | Partial display 1:1, flechas ◄►, static icon wf_square |
| 11 | OscSlvC | [x] | [x] | [x] | [x] | Partial display, flechas, static icon wf_saw |
| 12 | OscSlvD | [x] | [x] | [x] | [x] | Partial display, flechas, static icon wf_tri |
| 13 | OscSlvE | [x] | [x] | [x] | [x] | Partial display, flechas, static icon wf_sine |
| 14 | OscSlvA | [x] | [x] | [x] | [x] | Partial display, flechas, waveform radio selector |
| 31 | Noise | [x] | [x] | [x] | [x] | Static icon wf_noise (x=207 y=5) |
| 58 | DrumSynth | [x] | [x] | [x] | [x] | MTune=drumHz (20–748 Hz), STune=drumPartials (1:1–×6.26), drum icon badge |
| 85 | OscSlvFM | [x] | [x] | [x] | [x] | Partial display, flechas, static icon wf_sine |
| 95 | PercOsc | [x] | [x] | [x] | [x] | Static icon wf_percosc; Click/Decay bajados (y=30), Punch movido (x=222) |
| 96 | FormantOsc | [x] | [x] | [x] | [x] | Static icon wf_formant (x=238 y=2), display oscHzFormat |
| 97 | MasterOsc | [x] | [x] | [x] | [x] | display oscHzFormat (440·2^((v-69)/12) Hz) |
| 106 | OscSineBank | [x] | [x] | [x] | [x] | Partial ratios todos los tune; badge wf_sine bajo conector am osc6 |
| 107 | SpectralOsc | [x] | [x] | [x] | [x] | Static icon wf_spectral; label centrado; Partials (Odd/All) movido x=213 |

## LFO (14)

| # | Module | Layout | Graphics | LEDs/Meters | Buttons | Notes |
|---|--------|--------|----------|-------------|---------|-------|
| 24 | LFOA | [x] | [x] | [x] | [x] | LFO display animado (fase+rate); waveform icons sine/tri/saw/saw_inv/square/noise; lfoHzFormat; phaseFormat |
| 25 | LFOB | [x] | [x] | [x] | [x] | LFO display; waveform icons; PW knob afecta duty cycle en display |
| 26 | LFOC | [x] | [x] | [x] | [x] | LFO display; waveform icons |
| 27 | LFOSlvB | [x] | [x] | [x] | [x] | Partial display, badge wf_sine, wf_saw_inv corregido |
| 28 | LFOSlvC | [x] | [x] | [x] | [x] | Partial display; wf_saw_inv icon |
| 29 | LFOSlvD | [x] | [x] | [x] | [x] | Partial display; wf_saw_inv icon |
| 30 | LFOSlvE | [x] | [x] | [x] | [x] | Partial display; wf_saw_inv icon |
| 33 | ClkRndGen | [x] | [x] | [x] | [x] | decorator_rndgen_diskret badge (x=220 y=9) |
| 34 | RndStepGen | [x] | [x] | [x] | [x] | decorator_rndgen_diskret badge (x=213 y=6); partial display |
| 35 | RndPulsGen | [x] | [x] | [x] | [x] | decorator.rndgen.logic badge (x=214 y=6) |
| 68 | ClkGen | [x] | [x] | [x] | [x] | bpmFormat display (piecewise linear, bpm) |
| 80 | LFOSlvA | [x] | [x] | [x] | [x] | LFO display animado con rate (p2); phaseFormat; partial display |
| 99 | PatternGen | [x] | [x] | [x] | [x] | Step: botón incremento ▲▼ + display OFF/1-127; doble-click desactivado en increment |
| 110 | RandomGen | [x] | [x] | [x] | [x] | decorator_rndgen badge (x=215 y=6); partial display |

## Envelope (6)

| # | Module | Layout | Graphics | LEDs/Meters | Buttons | Notes |
|---|--------|--------|----------|-------------|---------|-------|
| 20 | ADSR | [x] | [x] | [ ] | [x] | ADSR display; attack-shape buttons env_log/lin/exp |
| 23 | Mod-Env | [x] | [x] | [ ] | [x] | Modulated envelope display |
| 46 | AHD | [x] | [x] | [ ] | [x] | AHD display implementado |
| 52 | Multi-Env | [x] | [x] | [ ] | [x] | Multi-env display; iconos bipolar/uni-exp/uni-lin |
| 71 | EnvFollower | [x] | [x] | [ ] | [x] | |
| 84 | AD-Env | [x] | [x] | [ ] | [x] | AD display implementado |

## Filter (11)

| # | Module | Layout | Graphics | LEDs/Meters | Buttons | Notes |
|---|--------|--------|----------|-------------|---------|-------|
| 32 | FilterBank | [x] | [x] | [ ] | [x] | Min/Max action buttons centrados bajo los faders |
| 45 | VocalFilter | [x] | [x] | [ ] | [x] | Vowel display (A/E/I/O/U/Y/AA/AE/OE), formatters |
| 49 | FilterD | [x] | [x] | [ ] | [x] | Bracket routing in→HP/BP/LP; conector in reposicionado a x=189 |
| 50 | FilterC | [x] | [x] | [ ] | [x] | Bracket routing in→HP/BP/LP |
| 51 | FilterE | [x] | [x] | [ ] | [x] | Filter display, reverse button order |
| 86 | FilterA | [x] | [x] | [ ] | [x] | Curva LP 6dB (ds-2-8) dibujada en código |
| 87 | FilterB | [x] | [x] | [ ] | [x] | Curva HP 6dB (ds-2-7) dibujada en código |
| 92 | FilterF | [x] | [x] | [ ] | [x] | Filter display implemented |
| 103 | EqMid | [x] | [x] | [ ] | [x] | EQ display implemented |
| 104 | EqShelving | [x] | [x] | [ ] | [x] | EQ display implemented |
| 108 | Vocoder | [x] | [x] | [ ] | [x] | Display routing, botones shift -2/-1/0/+1/+2/INV, Emp con curva |

## Mixer (13)

| # | Module | Layout | Graphics | LEDs/Meters | Buttons | Notes |
|---|--------|--------|----------|-------------|---------|-------|
| 18 | X-Fade | [x] | [x] | [ ] | [x] | dec-13 bitmap; buttons OK |
| 19 | Mixer (3) | [x] | [x] | [ ] | [x] | layout OK |
| 40 | Mixer (8) | [x] | [x] | [ ] | [x] | layout OK |
| 44 | GainControl | [x] | [x] | n/a | [x] | dec-2 bitmap; shift button cyclic |
| 47 | Pan | [x] | [x] | [ ] | [x] | dec-18 bitmap |
| 76 | OnOff | [x] | [x] | n/a | [x] | dec-14 bitmap; On/Off cyclic button |
| 79 | 4-1Switch | [x] | [x] | [ ] | [x] | layout OK |
| 81 | Amplifier | [x] | [x] | n/a | [x] | dec-12 bitmap; fmtAmpGain ("x1.00") |
| 88 | 1-4Switch | [x] | [x] | [ ] | [x] | layout OK |
| 111 | LevMult | [x] | [x] | [ ] | [x] | dec-12 bitmap |
| 112 | LevAdd | [x] | [x] | [ ] | [x] | dec-11 bitmap |
| 113 | 1to2Fade | [x] | [x] | [ ] | [x] | dec-7+dec-8 bitmaps |
| 114 | 2to1Fade | [x] | [x] | [ ] | [x] | dec-7+dec-9 bitmaps |

## Audio (15) ✓ DONE

| # | Module | Layout | Graphics | LEDs/Meters | Buttons | Notes |
|---|--------|--------|----------|-------------|---------|-------|
| 21 | Compressor | [x] | [x] | [ ] | [x] | compressor-display: piecewise-linear ported from Java (threshold/ratio/refLevel/limiter); VU pend. |
| 53 | Sample&Hold | [x] | [x] | n/a | n/a | Solo connectors + decoration-1 (sample-hold circuit icon) |
| 54 | Quantizer | [x] | [x] | n/a | [x] | knob + textDisplay (value+1) + decoration-6 |
| 57 | InvLevShift | [x] | [x] | n/a | [x] | mode radio (lev_shift icons) + inv toggle + decoration-6 |
| 61 | Clip | [x] | [x] | n/a | [x] | clip-display: ported from Java (center-based lines, vclip, sym); uses component-ids from XML |
| 62 | Overdrive | [x] | [x] | n/a | n/a | overdrive-display: angle-based bezier ported from Java (FilterIterator setBezier) |
| 74 | WaveWrap | [x] | [x] | n/a | n/a | wavewrap-display: zigzag ported from Java (9 peaks, div=16*vwrap+1) |
| 78 | Delay | [x] | [x] | n/a | n/a | textDisplay fmtDelayTime + decoration-5 |
| 82 | Diode | [x] | [x] | n/a | [x] | mode radio (wf_sine/diode_half/diode_full) programmatic icons |
| 83 | Shaper | [x] | [x] | n/a | [x] | mode radio (5 shaper transfer curves) programmatic icons |
| 94 | StereoChorus | [x] | [x] | n/a | [x] | 2 knobs + bypass button |
| 102 | Phaser | [x] | [x] | [ ] | [x] | phaser-display: EXP/LOG cubics ported from Java (peaks/spread/feedback); audio-out LED pend. |
| 105 | Expander | [x] | [x] | [ ] | [x] | expander-display: piecewise-linear ported from Java (threshold/ratio/gate); VU pend. |
| 117 | RingMod | [x] | [x] | n/a | [x] | 2 knobs + resetButton + decoration-3 (ring mod symbol) |
| 118 | Digitizer | [x] | [x] | n/a | [x] | increment btn + 2 knobs + 2 off/on btns + 2 textDisplays + groupboxes |

## Control (10)

| # | Module | Layout | Graphics | LEDs/Meters | Buttons | Notes |
|---|--------|--------|----------|-------------|---------|-------|
| 16 | PortamentoB | [x] | [x] | n/a | [x] | Flecha incorrecta removida |
| 22 | PartialGen | [x] | [x] | n/a | [x] | Rango corregido a 1-128 |
| 39 | Smooth | [x] | [x] | [x] | [x] | OK |
| 43 | Constant | [x] | [x] | [x] | [x] | OK |
| 48 | PortamentoA | [x] | [x] | [x] | [x] | OK |
| 66 | ControlMixer | [x] | [x] | [x] | [x] | OK |
| 72 | NoteScaler | [x] | [x] | n/a | [x] | Formato bipolar ASCII +/- con 1 decimal |
| 75 | NoteQuant | [x] | [x] | n/a | [x] | Formato bipolar ASCII +/- |
| 98 | KeyQuant | [x] | [x] | n/a | [x] | Valor inicial ajustado a 0 |
| 115 | NoteVelScal | [x] | [x] | n/a | [x] | Gráfico completo (slopes con clipping) |

## Logic (10) ✓ DONE

| # | Module | Layout | Graphics | LEDs/Meters | Buttons | Notes |
|---|--------|--------|----------|-------------|---------|-------|
| 36 | PosEdgeDelay | [x] | [x] | [x] | [x] | |
| 37 | LogicDelay | [x] | [x] | [x] | [x] | |
| 38 | Pulse | [x] | [x] | [x] | [x] | |
| 59 | CompareLev | [x] | [x] | [x] | [x] | |
| 64 | NegEdgeDelay | [x] | [x] | [x] | [x] | |
| 69 | ClkDiv | [x] | [x] | [x] | [x] | |
| 70 | LogicInv | [x] | [x] | [x] | [x] | |
| 73 | LogicProc | [x] | [x] | [x] | [x] | |
| 77 | ClkDivFix | [x] | [x] | [x] | [x] | |
| 89 | CompareAB | [x] | [x] | [x] | [x] | |

## Sequencer (4) ✓ DONE

| # | Module | Layout | Graphics | LEDs/Meters | Buttons | Notes |
|---|--------|--------|----------|-------------|---------|-------|
| 15 | NoteSeqA | [x] | [x] | [x] | [x] | 16-step LED array via MeterMessage; sequencer only advances from real clock |
| 17 | EventSeq | [x] | [x] | [x] | [x] | 16-step LED array via MeterMessage; step toggles/buttons OK |
| 90 | NoteSeqB | [x] | [x] | [x] | [x] | Piano-roll editor, zoom buttons, scrollbar, loop/rec/stop icons, 16-step LED array |
| 91 | CtrlSeq | [x] | [x] | [x] | [x] | 16-step LED array via MeterMessage; sequencer only advances from real clock |

---

## Known Global Issues

### LEDs
- [x] LEDs driven by globalLightValues (NMInfo 0x39) — clip LEDs by ledOnValue
- [x] Green/yellow/red color based on signal level (meters)
- [x] LED arrays rendered as arrays (sequencers use MeterMessage active-step values)

### Meters
- [x] VU meters animados verde/amarillo/rojo en tiempo real (NMInfo 0x3A)
- [x] Escala dB (0 a -30) entre barras cuando hay espacio

### Waveform Buttons
- [x] All oscillator/LFO waveform selector buttons render correctly: sine, tri, saw, saw_inv, square, noise
- [x] Static waveform badges on slave oscillators and LFOs (11×9px, dark rounded box, white icon)

### Custom Displays Already Implemented
- [x] ADSR, AD, AHD, Multi-Env envelope displays
- [x] LFO waveform display (sine, triangle, saw, inv-saw, square)
- [x] Overdrive, Clip, WaveWrap distortion displays
- [x] FilterE, FilterF filter response displays
- [x] EqMid, EqShelving EQ displays
- [x] Compressor, Expander dynamics displays
- [x] Phaser display

### Custom Displays Missing
- [x] Spectral Oscillator display
- [x] Formant Oscillator display
- [x] Step sequencer position indicators

---

## Connector I/O Audit (2026-06-11)

Automated cross-check of `data/classic-theme.xml` connectors against
`modules.xml` descriptors (the script lives in the session log; rerun by
comparing theme connector component-ids/classes per module against
descriptor connectors type/signal):

- **Structure: PASS.** All 382 themed connectors map 1:1 to descriptor
  connectors across the 109 themed modules — none missing, none duplicated,
  no unknown component-ids (Morph m0 has no theme; it is not instantiable).
- **Shape: PASS by construction.** Input circles / output squares are drawn
  from the descriptor `isOutput` flag, never from the theme, so direction
  cannot disagree with `modules.xml`.
- **Color: 43 theme classes disagreed with the descriptor signal** (e.g. the
  ADSR gate jack classed `cAUDIO` while the gate cable is logic-yellow).
  Fixed globally in 0.6.0: jacks are now colored from the descriptor's
  signal type — identical to cable coloring — with the theme class kept only
  as a fallback. Most-affected groups: slave osc/LFO master-slave inputs,
  envelope gate/retrig inputs, sequencer clock/reset inputs.
