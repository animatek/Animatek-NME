# Release Checklist

Repeatable checklist for tagged releases of Animatek NME. Run top to bottom;
every box must pass before tagging.

## 1. Pre-flight

- [ ] `git status` clean on `main`, all intended PRs/commits merged and pushed.
- [ ] Version set in the single source of truth: `CMakeLists.txt`
      (`project(AnimatekNME VERSION x.y.z)`). Confirm the app's About/version display matches.
- [ ] `CHANGELOG.md`: move Unreleased entries under the release version + date and remove
      superseded or duplicate descriptions.
- [ ] `ROADMAP.md` / `STATUS.md` reflect what actually shipped.

## 2. Build targets

- [ ] Linux: clean configure+build, Release config, no warnings-as-errors surprises:
      `cmake -B build-rel -DCMAKE_BUILD_TYPE=Release && cmake --build build-rel -j$(nproc)`
- [ ] Windows build (CI) produces an artifact that launches.
- [ ] macOS universal build (CI) produces an artifact that launches (Gatekeeper note in
      release text until signing exists).

## 3. Smoke tests (no synth required)

- [ ] App launches; main window restores size/position; all 13 themes can be selected;
      `Ctrl+T` cycles them; Nord is the fresh-install default.
- [ ] Wireframe toggles from View, Editor Options, and `Ctrl+W`; survives restart and remains
      legible with Classic, Nord, Frost, and one dark Bitwig theme.
- [ ] New patch → add modules via Quick Add (Enter and double-click) → cable two modules
      → undo/redo → save `.pch` → reload it (modules, cables, params, notes intact).
- [ ] Open a factory patch from `nomad-0-3_2` patches; canvas renders, no console errors.
- [ ] Preset browser opens, Disk filter lists the library, double-click loads.
- [ ] Variations: capture, recall, copy, initialize, and timed interpolation work; save/reopen
      the patch and confirm values, morph knobs, active variation, and mutation exclusions
      round-trip through the `.var` sidecar without changing `.pch` compatibility.
- [ ] Patch Mutator opens from `MUT`, View, and `Ctrl+8`; Mutate, Randomize, Interpolate,
      sequential/independent Cross, Quick Locks/Solo, temp storage, and variation drag work.
- [ ] SysEx Monitor opens with `Ctrl+9`; Enable, TX/RX filters, Auto-scroll, Clear, and Copy
      work, and capture remains disabled after closing it.
- [ ] Floaters (Knob, Keyboard, Patch Notes, Mutator, SysEx Monitor) open, remember position,
      accept `Ctrl+5..9` while focused, and reopen correctly after restart.

## 4. Hardware tests (Nord Modular G1 connected)

- [ ] Connect via MIDI settings; status bar shows Connected and the active slot's patch loads.
- [ ] Slot switching: from the editor (slot bar and Ctrl+1..4) and from the front panel —
      patch loads both ways.
- [ ] Turn a hardware knob → editor updates; drag an editor knob → synth responds.
- [ ] Mutator stress: rapidly audition several children on a large patch at Balanced speed;
      the synth remains connected and the final sound matches the selected child.
- [ ] Start a 10-second variation interpolation, immediately switch A/B/C/D from both editor
      and front panel, and confirm interpolation stops with no parameter spill into the new slot.
- [ ] While Mutator parameters are queued, load a bank patch and open a disk patch; confirm
      pending changes are discarded and the newly loaded patch is not modified.
- [ ] Edit a parameter during full patch upload; confirm it is applied after successful upload.
      Repeat with an interrupted/rejected upload and confirm retained changes are discarded.
- [ ] Verify Safe and Balanced send speeds on a large patch. Treat Fast and Maximum as
      hardware-dependent options, not release requirements.
- [ ] Keyboard Floater: note on/off, drone, repeat (rate + gate sliders).
- [ ] Upload a patch (File → Open → plays on synth); Store to Bank round-trips.
- [ ] Save Bank to Disk on a small bank; Send Bank to Synth restores it.
- [ ] Disconnect/reconnect MIDI: editor recovers without restart.

## 5. Package & publish

- [ ] CI artifacts downloaded and renamed `AnimatekNME-x.y.z-<platform>`.
- [ ] Launch each packaged artifact and confirm its reported version is the intended release.
- [ ] `git tag vx.y.z && git push --tags`.
- [ ] Release notes: paste the CHANGELOG section, note known limitations
      (e.g. stuck MIDI-IN notes are cleared with the synth's front-panel panic).
- [ ] Review `RELEASE_NOTES_x.y.z.md` against the final build and use it for the distribution post.
- [ ] Patreon post with binaries/links.

## 6. Post-release

- [ ] Confirm an empty `Unreleased` section remains at the top of `CHANGELOG.md` for the next cycle.
- [ ] Sweep GitHub issues: close shipped, label the rest.
