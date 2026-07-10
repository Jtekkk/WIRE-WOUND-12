# FLUXCORE-12 — Packaging

This directory holds the Windows installer script (`installer.iss`, Inno Setup)
that bundles the built VST3 plugin and the Standalone app into a single
`FLUXCORE-12-Installer.exe`.

## Build layout

CMake (via JUCE) writes the plugin artefacts to:

```
<cmake-build-dir>/FLUXCORE12_artefacts/<Config>/
    VST3/FLUXCORE-12.vst3/        # the VST3 bundle (a folder)
    Standalone/FLUXCORE-12.exe    # the standalone application
```

The installer's `BuildDir` should point at the `<Config>` directory
(e.g. `.../FLUXCORE12_artefacts/Release`).

## Build the installer locally (Windows)

1. Configure and build the plugin in Release:

   ```cmd
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release --parallel
   ```

2. Install Inno Setup 6 (https://jrsoftware.org/isdl.php or `choco install innosetup -y`),
   then compile the installer, pointing `BuildDir` at the Release artefacts:

   ```cmd
   iscc packaging\installer.iss "/DBuildDir=%CD%\build\FLUXCORE12_artefacts\Release"
   ```

   If you omit `/DBuildDir`, the script falls back to
   `..\build\FLUXCORE12_artefacts\Release` (relative to this folder).

3. The installer is written to `packaging/Output/FLUXCORE-12-Installer.exe`.

Running it installs:

- `FLUXCORE-12.vst3` into `C:\Program Files\Common Files\VST3\`
- `FLUXCORE-12.exe` (standalone) into `C:\Program Files\FLUXCORE-12\`, with a
  Start Menu shortcut.

The installer is 64-bit only.

## CI

The GitHub Actions workflow (`.github/workflows/build.yml`) builds on Ubuntu,
Windows, and macOS, runs the native DSP tests, and on Windows also builds the
installer. The installer is uploaded as the workflow artifact
**`FLUXCORE-12-Windows-Installer`** (containing `FLUXCORE-12-Installer.exe`),
and the raw per-OS plugin artefacts are uploaded as
**`FLUXCORE-12-artefacts-<os>`**.

On a `v*` tag push, the `release` job gathers every artifact and publishes a
GitHub Release with the installer `.exe` and zipped per-OS artefacts attached.
