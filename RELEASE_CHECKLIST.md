# Release Checklist

Repeatable checklist for tagged releases of Animatek NME. Run top to bottom;
every box must pass before tagging.

## 1. Pre-flight

- [ ] `git status` clean on `main`, all intended PRs/commits merged and pushed.
- [ ] Version bumped consistently: `CMakeLists.txt` (`project(... VERSION x.y.z)` and the
      `juce_add_gui_app`/compile-definition strings), `Main.cpp` `getApplicationVersion()`.
- [ ] `CHANGELOG.md`: move "(in development)" entries under the release version + date.
- [ ] `ROADMAP.md` / `STATUS.md` reflect what actually shipped.

## 2. Build targets

- [ ] Linux: clean configure+build, Release config, no warnings-as-errors surprises:
      `cmake -B build-rel -DCMAKE_BUILD_TYPE=Release && cmake --build build-rel -j$(nproc)`
- [ ] Windows build (CI) produces an artifact that launches.
- [ ] macOS universal build (CI) produces an artifact that launches (Gatekeeper note in
      release text until signing exists).

## 3. Smoke tests (no synth required)

- [ ] App launches; main window restores size/position; theme switch works.
- [ ] New patch → add modules via Quick Add (Enter and double-click) → cable two modules
      → undo/redo → save `.pch` → reload it (modules, cables, params, notes intact).
- [ ] Open a factory patch from `nomad-0-3_2` patches; canvas renders, no console errors.
- [ ] Preset browser opens, Disk filter lists the library, double-click loads.
- [ ] Floaters (Knob, Keyboard, Patch Notes) open, remember position, reopen on restart.

## 4. Hardware tests (Nord Modular G1 connected)

- [ ] Connect via MIDI settings; status bar shows Connected and the active slot's patch loads.
- [ ] Slot switching: from the editor (slot bar and Ctrl+1..4) and from the front panel —
      patch loads both ways.
- [ ] Turn a hardware knob → editor updates; drag an editor knob → synth responds.
- [ ] Keyboard Floater: note on/off, drone, repeat (rate + gate sliders).
- [ ] Upload a patch (File → Open → plays on synth); Store to Bank round-trips.
- [ ] Save Bank to Disk on a small bank; Send Bank to Synth restores it.
- [ ] Disconnect/reconnect MIDI: editor recovers without restart.

## 5. Package & publish

- [ ] CI artifacts downloaded and renamed `AnimatekNME-x.y.z-<platform>`.
- [ ] `git tag vx.y.z && git push --tags`.
- [ ] Release notes: paste the CHANGELOG section, note known limitations
      (e.g. stuck MIDI-IN notes are cleared with the synth's front-panel panic).
- [ ] Patreon post with binaries/links.

## 6. Post-release

- [ ] Open next `x.y.z+1 (in development)` section in CHANGELOG.md.
- [ ] Sweep GitHub issues: close shipped, label the rest.
