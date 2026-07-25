# Private Patreon Builds

The `Build Patreon Binaries (Private Draft)` GitHub Actions workflow builds the
Linux, Windows, and macOS applications without publishing the binaries.

Each successful platform build produces:

- `AnimatekNME-x.y.z-<platform>.zip`
- `AnimatekNME-x.y.z-<platform>.zip.sha256`
- `AnimatekNME-x.y.z-<platform>.zip.sigstore.json`

The workflow stores these files in an unpublished GitHub Draft Release. Draft
Releases are visible only to repository collaborators with write access. The
workflow intentionally does not use public GitHub Actions artifacts.

## Manual test build

1. Push the intended source commit to `main`.
2. Open **Actions > Build Patreon Binaries (Private Draft)**.
3. Choose **Run workflow**, select `main`, and leave **Platform** set to `all`.
4. Wait for the Linux, Windows, and macOS jobs to finish.
5. Open the Draft Release URL shown in the workflow summary.
6. Download the ZIP, checksum, and Sigstore bundle for each platform.
7. Verify each ZIP before testing it.

Linux and macOS:

```bash
sha256sum -c AnimatekNME-x.y.z-linux.zip.sha256
gh attestation verify AnimatekNME-x.y.z-linux.zip \
  --repo animatek/Animatek-NME
```

Windows PowerShell:

```powershell
Get-FileHash .\AnimatekNME-x.y.z-windows.zip -Algorithm SHA256
gh attestation verify .\AnimatekNME-x.y.z-windows.zip `
  --repo animatek/Animatek-NME
```

Compare the PowerShell hash with the value in the `.sha256` file.

## Patreon publication

Only upload a build to Patreon after all three platform jobs pass and the
applications have completed their manual smoke tests. Attach all three files
for each platform so patrons can verify both integrity and provenance.

Every ZIP includes:

- the application;
- the platform README;
- `LICENSE.txt`;
- `BUILD-INFO.txt`, containing the exact source commit and workflow run.

Do not publish the GitHub Draft Release. Patreon is the distribution channel
for these binaries.

## Version-tag builds

Pushing a new tag in the form `vX.Y.Z` starts the same private build
automatically. The tag version must exactly match the version in
`CMakeLists.txt`, otherwise the workflow stops before compiling.

The existing `v0.11.0` tag predates this workflow. Test 0.11.0 by running the
workflow manually from `main`; do not move or recreate the existing tag.

## Failed and partial runs

The matrix does not stop the other platforms when one platform fails. This
makes it possible to see all platform errors in one run. A failed run may leave
a partial Draft Release containing only the successful packages. Do not
distribute it. Delete the partial draft after diagnosing the failure, then run
the workflow again.
