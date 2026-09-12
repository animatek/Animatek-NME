# Post-0.18.0 Development Plan

Date: 2026-09-10. Baseline: `v0.18.0` (`6bd60aa`).

Agreed direction with Javier: protect work first, then make the editor faster and
make sound exploration easier to organise and reverse. Patch version history and
library tags are explicit product priorities, not optional polish.

This is a plan, not a release announcement. Checkboxes record verified implementation
progress. Version numbers are proposed delivery targets, not fixed dates or promises
to bundle everything into one release. A completed phase must pass its exit criteria.

[ROADMAP.md](ROADMAP.md) is the entry point; this document owns the detailed backlog
for this review. [IMPROVEMENT_PLAN.md](IMPROVEMENT_PLAN.md) records the August review
and earlier work. Do not reintroduce already shipped canvas, preset or MDI features.

## 1. Evidence And Scope

The review followed patch replacement, file saving, MIDI delivery, bank transfers,
variations, extras persistence and browser/rendering paths. It was not an exhaustive
audit of every module, platform or plugin path.

- At the review baseline, 65 test cases / 2,074 assertions passed. See S1 for the
  subsequent implementation and expanded suite.
- A separate ASan/UBSan-instrumented probe reproduced a heap-use-after-free when a
  queued protocol send outlived `MidiDeviceManager`. No MIDI ports were opened.
- The temporary reproducer was `/tmp/opencode/nme-midi-lifetime-probe.cpp`. S1 has
  promoted its queued-send/destruction/timeout scenario into a repository test.
- S2-S7 were identified by tracing production code. Reproduce each in an isolated
  fixture before changing it; hardware effects still need G1 verification.
- Performance opportunities are code-based hypotheses. No new speedup percentages
  or whole-application profiling results were established by this review.
- The baseline tests covered pure layers. S1 adds connection-lifetime coverage;
  full synchronization, bank-transfer and `MainComponent` workflows remain T1 work.
  See [tests/CMakeLists.txt](../tests/CMakeLists.txt).

## 2. Delivery Order

| Target | Scope | Exit Condition |
|--------|-------|----------------|
| 0.18.1 | S1-S7 safety fixes and their regression tests | No known silent overwrite, LOCAL leakage or reproduced disconnect UAF in these scenarios; hardware smoke tests pass |
| 0.19.0 | T1/P3 integration, P1/P2 performance, D1 identity contract, R1 recovery | Deterministic transfer tests, responsive browser and recoverable four-slot sessions |
| 0.20.0 | V1 patch history, V2 bank history, L1 tags/favourites | History and tags survive restart, rename and restore without changing synth behaviour |
| 0.21.0 | C1 comparison, L2 content search/duplicates, G1 diagnostics, M1/M2 MCP | Selective recovery, useful library queries and verified automation |

Build only the parts of T1 needed for each 0.18.1 fix first. Do not delay a safety
fix for a wholesale state-machine rewrite. D1 must be settled before R1 persists
new session identities; 0.20.0 completes the user-facing history/library layer.

Dependencies: S1-S7 -> T1/P3 and D1 -> R1 -> V1/V2/L1 -> C1/L2/M2.
P1 can proceed alongside recovery work once its background-job lifetime tests exist.
G1 and M1 can ship earlier when their prerequisites are ready.

## 3. Safety Fixes: 0.18.1

Review priority: S1-S5 are high (memory safety, unintended writes or loss of work);
S6-S7 are medium (incomplete persistence/integration). Reproduction and regression
coverage determine readiness, not the order in which the review found them.

### S1. Disconnect Lifetime And Pending Sends

Software implemented locally on 2026-09-11; hardware/platform acceptance is still
open. No commit or release yet. Detail: [CHANGELOG.md](../CHANGELOG.md).

- [x] Remove the protocol sender callback before its device owner is destroyed.
- [x] Cancel queued work, reply waits and delayed callbacks belonging to the old
  connection; reconnect must not replay old edits against a new connection.
- [x] Preserve the existing close-transfer behaviour for an interrupted upload
  (verified against recorded frames; physical G1 check below).
- [x] Add a regression that queues a reply-waiting message and a second send,
  destroys/disconnects the device, then advances beyond the reply timeout.
- [x] Test disconnect, simulated reconnect and destruction under ASan/UBSan,
  including stale settings/list requests, parameter/ACK queues and posted upload
  completions. Interrupted bank uploads report failure exactly once.
- [ ] Verify real port-open failure, disconnect/reconnect and interrupted uploads
  with the G1 on supported platforms; no crash, stale delivery or stuck bulk-receive
  state is allowed. Follow the S1 section of [MANUAL_TEST_PLAN.md](MANUAL_TEST_PLAN.md).

Verification: ten new cases in [test_midi_lifecycle.cpp](../tests/test_midi_lifecycle.cpp).
The pre-fix checks failed on outstanding waits and stale reconnect requests; ASan
also reproduced the settings-readback use-after-free. The full Linux Debug app
build succeeds with existing warnings. All 75 cases / 2,153 assertions pass in
Debug and ASan/UBSan, with five consecutive headless sanitizer runs. No MIDI ports
were opened. The recorder uses the real protocol/connection classes; it does not
substitute for OS-port or hardware timing tests.

Sources: [MidiDeviceManager.cpp](../source/midi/MidiDeviceManager.cpp),
[NmProtocol.cpp](../source/midi/NmProtocol.cpp),
[ConnectionManager.cpp](../source/midi/ConnectionManager.cpp).

### S2. Preserve The Last Good Backup

`saveAllBanksToDisk()` currently deletes the old mirror before fetching the new one.
The UI warns about this, but a failed backup should not destroy a good backup.

- [x] Download to a separate staging generation, leaving the previous mirror intact.
  `.nme-backup-staging` beside the mirror; leftovers from an interrupted run are
  cleared at the start of the next one.
- [x] Track missing sections, parse failures, write failures and cancellation as
  incomplete results. A non-null parsed patch is not proof of a complete download.
  Parse and write failures and fetch timeouts were already recorded; the new
  check also requires the run to have reached the end of the item list, which is
  what catches a disconnect ending the loop without recording anything.
- [x] Publish a new complete generation only after every required item is verified.
  Keep the last complete generation available if publication is interrupted.
  A failed publication keeps the complete staged copy and logs its path.
- [ ] Test cancellation after the first item, disconnect, partial downloads, full
  disk and publication failure. The previous complete backup must remain readable.
  Partly done: `tests/test_bank_backup_safety.cpp` covers the two refusals (no
  connection, no patch list) and asserts the previous backup survives byte for
  byte. The rest needs a patch list on the connection, so it waits on T1's
  injectable seam. Not verified on hardware.
- [x] Preserve mirror semantics for genuinely deleted synth patches only after a
  complete bank list and successful new backup establish that they are absent.
  Applied at publication, and the backup refuses to start without a loaded list.

Implemented 2026-09-12, except the test row above. The UI warning was rewritten:
it told the user the folders are emptied first, which is no longer what happens.

Sources: [BankTransferManager.cpp](../source/sync/BankTransferManager.cpp),
`ConnectionManager::finalizePatch()`, [PatchParser.cpp](../source/model/PatchParser.cpp).
Version browsing and retention are V2, not prerequisites for this safety fix.

### S3. Bind Delayed Uploads To The Intended Patch

`loadPatchFromFile()` captures a destination slot but reads `currentPatch()` inside
a delayed callback. Focus can change between scheduling and execution.

- [x] Address the intended slot explicitly, never resolve its contents via focus.
- [x] Bind deferred work to a patch generation; cancel it when that slot's patch
  is replaced, the connection changes or a newer operation supersedes it.
  `slotPatchGeneration[]` is bumped at all four sites that replace a slot's patch.
- [x] Audit the sibling settings/store/upload callbacks for the same mismatch.
  Only `showPatchSettingsDialog()` shared it, in both directions, and is fixed.
  The others already guard on `activeSlot` or carry their own generation.
- [ ] Test loading into A then focusing B before 200 ms, two rapid loads into A,
  New Patch during the delay, and disconnect/reconnect during the delay.
- [ ] Assert both destination and serialized contents; no stale upload may land.

Implemented 2026-09-11. The fix was verified by reading the code, not by a test:
the 200 ms window needs the controllable scheduler T1 calls for, so the last two
boxes stay open.

Source: [MainComponent.cpp](../source/MainComponent.cpp), `loadPatchFromFile()` and
`showPatchSettingsDialog()`.

### S4. Make LOCAL A Real Boundary

Omitting `PatchSynchronizer` is insufficient: parameter and undo callbacks also send
directly. Local editing must not change an unrelated patch on the hardware.

- [ ] Establish one slot-aware policy used by UI, undo, variations, assignments
  and MCP instead of adding ad hoc guards to individual controls.
- [ ] Prevent outgoing patch edits and incoming hardware edits from modifying the
  wrong side of a LOCAL slot; an explicit fetch/upload is a separate operation.
- [ ] Keep LOCAL until a successful transfer establishes correspondence. A failed
  upload must not claim the model is synchronized.
- [ ] Test Inspector/canvas/knob edits, delete/undo, morphs, variations and MCP
  against a LOCAL slot while another slot remains connected and editable.
- [ ] Verify zero unintended edit messages for the LOCAL slot, then verify an
  explicit upload restores normal synchronization only after success.

Sources: `MainComponent::setSlotLocal()`, `wireSlotView()`, `rebuildUndoContext()` and
[PatchActions.h](../source/undo/PatchActions.h).

### S5. Keep File Ownership With The Document

Receiving another patch from the synth currently replaces the model without clearing
the previous slot's file association.

- [ ] Clear the save target when a genuinely different patch arrives from a bank
  or the hardware; preserve it only for a positively identified reread.
- [ ] Do not use a matching name alone as proof of document identity.
- [ ] Test opening `Bass.pch`, loading another bank patch into the same slot, and
  pressing Save. `Bass.pch` must remain unchanged; Save must request a new target.
- [ ] Test same-name/different-content patches and background-slot deliveries.

Source: [MainComponent.cpp](../source/MainComponent.cpp), the patch-data callback,
`saveSlotPatch()` and `suggestedSaveFileForSlot()`.

### S6. Save Patch And Variations Coherently

- [ ] Report `.var` write failures instead of returning success after only `.pch`.
- [ ] Remove or explicitly replace an obsolete sidecar when the new document has
  neither variations nor mutation exclusions.
- [ ] Stage the file set and define interrupted-save recovery. Two independent
  file renames are not an atomic multi-file transaction.
- [ ] Test failure between writes, an unwritable sidecar, overwriting a path with
  an old `.var`, and successful reopen of both files with values/exclusions intact.
- [ ] Apply the same truthful error reporting to UI and MCP saves.

Sources: `MainComponent::saveSlotPatchToFile()`,
[PatchVariations.cpp](../source/model/PatchVariations.cpp),
[PchFileIO.cpp](../source/model/PchFileIO.cpp).

### S7. Feed Custom Values Into NewModule

- [x] Collect custom-class parameters in descriptor order in the synchronizer,
  including zero values, and pass them to the existing encoder.
- [x] Test the production synchronizer path, not only a hand-constructed message.
  `tests/test_synchronizer_new_module.cpp`, on the recording transport from S1.
- [x] Check OscA and another custom-bearing module against serialization/reference
  data; cover ordinary add and applicable import paths.
  OscA (one custom value) and NoteSeqB (two), both through `createModule`.
  Import paths do not go through the synchronizer and are not covered here.
- [ ] Verify the G1 reread preserves the intended custom settings. Do not attribute
  unrelated oscillator freezes to this fix without hardware evidence.

Implemented 2026-09-11. Note for the record: this fix was written once before, on
2026-09-10, and reported in the 0.18.0 changelog. The code was never committed and
the release shipped without it, which is why the regression test exists now.

Sources: [PatchSynchronizer.cpp](../source/sync/PatchSynchronizer.cpp),
[NewModuleMessage.cpp](../source/protocol/NewModuleMessage.cpp),
[test_new_module_message.cpp](../tests/test_new_module_message.cpp).

## 4. Integration And Performance: 0.19.0

### T1. Deterministic MIDI Workflow Tests

S1 supplies transport recording and real-timer lifetime tests. A controllable
scheduler and the broader transfer/synchronizer coverage below are still pending.

- [ ] Introduce the smallest injectable transport and controllable scheduling seam
  needed to record outgoing frames and feed incoming messages without MIDI devices.
- [ ] Exercise disconnects, stale/duplicate/late ACKs, missing sections, slot changes,
  concurrent edit requests and cancellation. Assert final model and wire sequence.
- [ ] Cover `ConnectionManager`, `PatchSynchronizer`, bank transfers and the extracted
  patch-operation boundary in CTest and ASan/UBSan CI.
- [ ] Use synthetic fixtures and explicitly approved/anonymized captures. Tests must
  never auto-connect, write synth banks or depend on a developer's patch library.

This is protocol/workflow simulation, not an audio or DSP emulator. Keep real G1
tests for timing tolerance and behaviour the simulation cannot establish.

### P1. Asynchronous Library Indexing

- [ ] Move recursive discovery and legacy-format sniffing off the message thread.
  `paintListBoxItem()` must render cached data and never open a file.
- [ ] Publish immutable batches of results on the message thread; support progress,
  cancellation and replacement of an in-flight scan when the library root changes.
- [ ] Share the index between the integrated browser and the floating browser.
- [ ] Preserve selection by file/document identity, not row number, while results
  change; an old scan must never repopulate a new library.
- [ ] Test closing a browser during scanning, inaccessible directories, duplicate
  filenames, large libraries and slow storage, without disrupting MIDI processing.

Sources: [PresetBrowserWindow.cpp](../source/ui/PresetBrowserWindow.cpp),
[PchFileIO.cpp](../source/model/PchFileIO.cpp).

### P2. Revision-Driven Extras Persistence

- [ ] Replace repeated whole-patch change detection with explicit structural and
  extras revisions, covering UI, undo/redo, MCP, morphs and variation edits.
- [ ] Recalculate structural fingerprints only on relevant changes; reuse a
  connector-owner lookup within a calculation instead of searching for each end.
- [ ] Persist from immutable snapshots, debounce writes and retry/report failures.
  Failed writes must not clear dirty state; no mutable model pointers on workers.
- [ ] Test that every relevant edit invalidates the right revision and that an idle
  session does not keep rebuilding fingerprints or rewriting unchanged files.

Sources: `MainComponent::extrasRevision()`, `flushAllExtras()` and
[PatchExtras.cpp](../source/model/PatchExtras.cpp).

### P3. One Patch-Transition Boundary

- [ ] Consolidate the common steps of New, file load, synth fetch and MCP replacement
  behind explicit destination, origin, generation and synchronization policies.
- [ ] Preserve their different meanings: a synth fetch must not be uploaded back,
  a LOCAL file load must stay local, and an explicit upload must await completion.
- [ ] Centralize reference detachment, pending-work invalidation, undo reset, extras
  flush/bind and file association. Retain intended per-slot focus behaviour.
- [ ] Extract only where tests need a seam or repeated ownership rules warrant it.
  A whole `MainComponent` rewrite is not part of this phase.

### Performance Acceptance

- [ ] Record a Release-build baseline: platform, patch/library size, idle CPU,
  browser scan/search latency, message-thread stalls and paint-time percentiles.
- [ ] Repeat with one/four slots, dense wiring, active meters and slow storage.
- [ ] Report comparable before/after measurements; establish budgets from that
  baseline rather than presenting unmeasured speedup targets as results.
- [ ] Preserve the existing partial repaints and light/meter caches. Consider more
  canvas caching only if profiling identifies a remaining bottleneck.

## 5. Data Contract And Recovery

### D1. Distinguish Identity, History And Variations

| Concept | Meaning |
|---------|---------|
| Editor release | App version such as 0.19.0; published through the existing release process |
| Patch identity | A logical document that can be renamed and have multiple revisions |
| Patch generation | A live slot incarnation used to reject stale asynchronous work |
| Patch revision | An immutable recoverable snapshot of patch content and required extras |
| Musical variation | One of the existing eight parameter/morph snapshots in `.var`; not a full structural revision |
| Library tag | User metadata such as `bass`, `fm` or `live`; not a Git tag or module search tag |

- [ ] Audit reuse of `extrasId` before choosing an identity scheme. Preserve existing
  `.pch`, `.var` and `.nmx` data; do not replace identities based on filename alone.
- [ ] Specify Save As versus an independent copy: a new independent document must
  not accidentally share mutable extras/history with its source.
- [ ] Separate logical identity from a content digest. The current structural
  fingerprint is not a complete revision digest: it omits parameter values.
- [ ] Handle ambiguous matches for synth patches explicitly; the wire has no editor
  identity. Same name/topology does not authorize merging histories or tags.
- [ ] Version new metadata/manifest schemas and test migration and unsupported
  versions. Keep standard `.pch` interchange and existing sidecars readable.

Decide the storage backend during this task, using expected collection sizes and
migration tests. Do not add a database, Git dependency or new file format merely to
implement a checkbox. Indexes should be rebuildable; user-authored metadata is not.

### R1. Four-Slot Session Recovery

- [ ] Track unsaved document changes, including structural edits, parameters, notes
  and variations; distinguish disk state from hardware synchronization state.
- [ ] Autosave recoverable snapshots of all four slots to a recovery location,
  separate from user `.pch` files, backups and the public preset list.
- [ ] Publish only complete recovery generations; after a simulated crash at each
  write boundary, recover the last complete generation rather than mixed files.
- [ ] Offer recovery on startup and Save/Discard/Cancel before destructive document
  replacement or quit. Normal sound auditioning needs a lightweight recovery path,
  not a blocking save dialog on every preset click.
- [ ] Recover into LOCAL state without automatic upload or bank writes. Coordinate
  startup auto-connect so it cannot silently overwrite recovered documents.
- [ ] Provide bounded retention and clear storage/error status; never erase the only
  valid recovery copy merely because the next write was attempted.

Acceptance: crash/restart restores both areas of all four patches, wiring, values,
assignments, notes, variations and exclusions. Original files and synth banks remain
unchanged until the user explicitly saves or sends the recovered work.

## 6. Version History And Tags: 0.20.0

### V1. Patch History

- [ ] Add named checkpoints and automatic revisions on successful saves; do not
  create a permanent revision for every knob movement.
- [ ] Show time, optional label and a compact change summary. Allow pinning a good
  revision so automatic retention never removes it.
- [ ] Store enough content to reconstruct modules, cables, parameters, assignments,
  notes, variations and exclusions, independent of the original file path.
- [ ] Preview or open an older revision locally. Restore must preserve the current
  state as a recoverable checkpoint; overwriting a file/uploading is explicit.
- [ ] Keep library tags and favourites attached to the logical patch. Restoring an
  old sound must not unexpectedly roll back today's library organisation.
- [ ] Test rename, restart, independent copy, missing source file, interrupted save,
  incompatible metadata and retention with pinned revisions.

First useful workflow: save a patch, experiment, label a good result, then recover
the earlier sound without losing either version. No Git knowledge is required.

### V2. Versioned Bank Backups And Exact Restore

- [ ] Build on S2's complete generations: history by date, completeness status,
  failures and a manifest with bank/position/content checksums.
- [ ] Add an exact-position restore mode distinct from the existing consecutive
  folder upload. A bank with patches at 1, 5 and 99 must restore to 1, 5 and 99.
- [ ] Preview the destination changes. Distinguish leaving unused positions alone
  from clearing them for an exact mirror; clearing requires explicit confirmation.
- [ ] Preserve/protect the temporary working slot's unsaved editor content and
  explain that transfer can interrupt its sound; do not promise seamless playback.
- [ ] Validate all files before writing hardware, then track upload and store results.
  A timed-out Store is unconfirmed, not success; verify by rereading where supported.
- [ ] Keep old complete generations until retention is deliberately applied. Test
  cancellation, damaged manifests, failed stores and sparse-bank restoration.

The disk transaction can protect backups, but multiple hardware bank writes cannot
be made atomic. Report partial restore progress and a recovery path honestly.

### L1. Library Tags And Favourites

- [ ] Add user tags and a separate favourite flag to patches; support batch tagging,
  tag rename/removal and filtering alongside the existing text/type filters.
- [ ] Define matching: all selected tags must match; normalize surrounding whitespace
  and case for matching while preserving the user's displayed spelling.
- [ ] Keep metadata across app restarts, patch revisions and recognized renames/moves.
  Ambiguous external moves/copies require reconciliation, not silent metadata merging.
- [ ] Keep library tags separate from `ModuleTags.cpp` and module descriptors.
  Tagging or favouriting must never send SysEx or alter the patch's sound.
- [ ] Exclude internal history/recovery files from normal preset discovery and from
  recursive indexing loops. Provide an explicit metadata export/backup path.
- [ ] Test same-name patches in different folders, independent copies, batch undo,
  removal of a tag used by many files and a rebuild of the disposable index.

First useful workflow: mark favourite bass patches, tag some `fm` and `live`, filter
that subset, audition it with the existing arrows, and reopen with everything intact.

## 7. Comparison, Discovery And Automation

### C1. Semantic Patch And Variation Comparison

- [ ] Compare against the last saved revision, another history revision, another
  patch or one of the existing variations. Start with read-only comparison.
- [ ] Group differences into parameters, wiring, modules, assignments and extras;
  optionally ignore layout-only changes. Use formatted values and readable names.
- [ ] Match modules safely: reused container indices are not sufficient across
  unrelated revisions. Expose ambiguous matches instead of applying guesses.
- [ ] Add selective recovery first for parameters on verified compatible modules,
  then structural changes only with dependency validation and one undo transaction.
- [ ] Test add/delete/index reuse, differing module types and stale sidecars.
  A comparison or preview must not change either source or transmit MIDI.

### L2. Content Search And Duplicate Detection

- [ ] Extend P1's index with module types/counts and tag/favourite queries, so a
  search can find patches using a Vocoder or a particular oscillator family.
- [ ] Distinguish exact duplicates from structurally similar patches. A shared
  topology does not imply the same values or the same sound.
- [ ] Offer review groups and navigation, never automatic file deletion/merging.
- [ ] Test invalid/unreadable files, same-name/different-content patches and index
  rebuilding without loss of user metadata.

### G1. Exportable Diagnostic Report

- [ ] Capture app/platform version and transfer state, timeout/error counters and
  optionally a bounded SysEx trace, extending the existing monitor.
- [ ] Default to no patch content, local paths or device serials. Show a preview
  and let the user approve optional sensitive fields before local export.
- [ ] No automatic upload or telemetry. Capturing diagnostics must not change
  protocol timing materially or grow memory without a limit.
- [ ] Test export with the synth disconnected, during a failed transfer and with
  all optional private data excluded.

### M1. MCP Assignments

- [ ] Expose knob/morph assignment, removal and listing through the normal undo
  and synchronization policy. Reuse the existing roadmap item rather than creating
  a second assignment implementation.
- [ ] Validate slot/section/module/parameter identities and collisions; reject stale
  targets. Test LOCAL state and transfer-in-progress behaviour.
- [ ] Update both the C++ handler and `mcp-bridge/server.py`; verify UI and hardware
  results through the same integration tests as manual assignment.

### M2. Grouped MCP Operations

- [ ] Define a batch with explicit target slot and expected patch generation;
  prevalidate its operations before making changes.
- [ ] Apply the valid batch as one undo transaction with a coalesced synchronization
  step. Test an invalid middle operation, cancellation and connection loss.
- [ ] State the atomicity boundary: editor rollback is possible; rollback of already
  sent hardware messages is not guaranteed. Do not report an initiated store as a
  verified successful bank write.
- [ ] Keep disk writes and destructive bank operations outside the first batch API;
  they require their own explicit confirmation/result contracts.

## 8. Release Gates And Working Rules

- [ ] Each fix gets a failing regression first, the smallest correct change, and
  evidence that the test passes afterward. Record any remaining hardware uncertainty.
- [ ] Test pure logic and integration under CTest/ASan/UBSan; add focused static
  analysis only where it provides actionable findings, not a style-rewrite backlog.
- [ ] Verify Linux, Windows and macOS builds and file-lifetime behaviour. Test crash
  recovery/publication on the actual filesystems used, including the NTFS workspace.
- [ ] Follow [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md) and extend
  [MANUAL_TEST_PLAN.md](MANUAL_TEST_PLAN.md) with the new user workflows.
- [ ] Record completed work and real verification in the project and shared CODE
  changelogs. Keep pending tasks here; link issues/PRs as they are actually created.
- [ ] Do not silently change `.pch` compatibility, the synth protocol, send-speed
  defaults or existing undo semantics while adding metadata and history.

Out of scope: a DSP emulator, cloud/community hosting, a canvas rewrite without
profiling, and plugin productization. The experimental VST/CLAP track stays separate.

## 9. First Work Package

1. Promote the queued-send lifetime reproducer into the test target and fix S1.
2. Protect the existing bank backup with S2 and fault-injection tests.
3. Fix S3-S5 with explicit slot/document generation and LOCAL policy tests.
4. Fix S6-S7, then run the complete desktop and G1 smoke-test pass for 0.18.1.
5. Begin P1 and the D1/R1 design, keeping history and tags as the next product goals.

When a task is finished, mark only its verified checkboxes and add its issue/commit
reference. A planned release does not become a completed release by editing this file.
