; ============================================================================
;  FLUXCORE-12 — Windows installer (Inno Setup)
;
;  Expected BuildDir layout
;  ------------------------
;  JUCE writes its build artefacts under
;      <cmake-build>/FLUXCORE12_artefacts/<Config>/
;  so pass the <Config> directory as BuildDir, e.g.
;      <cmake-build>/FLUXCORE12_artefacts/Release
;
;  Inside that directory this script expects:
;      {#BuildDir}\VST3\FLUXCORE-12.vst3\        (the VST3 bundle, a folder)
;      {#BuildDir}\Standalone\FLUXCORE-12.exe    (the standalone app)
;
;  Build the installer with:
;      iscc packaging\installer.iss "/DBuildDir=C:\path\to\build\FLUXCORE12_artefacts\Release"
;  If BuildDir is not supplied on the command line the default below is used.
; ============================================================================

#define AppName        "FLUXCORE-12"
#define AppPublisher   "TEKK Engineering Audio Labs"
#define AppVersion     "0.1.0"
#define OutputBaseName "FLUXCORE-12-Installer"

; BuildDir is overridable from the iscc command line via /DBuildDir=...
#ifndef BuildDir
  #define BuildDir "..\build\FLUXCORE12_artefacts\Release"
#endif

[Setup]
AppId={{5E0A4C3B-12CE-4F1E-9B2A-FLUXCORE0012}}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir=Output
OutputBaseFilename={#OutputBaseName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; 64-bit only
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64

[Files]
; VST3 bundle -> common VST3 folder. The .vst3 is a directory, so recurse it.
Source: "{#BuildDir}\VST3\{#AppName}.vst3\*"; DestDir: "{commoncf64}\VST3\{#AppName}.vst3"; \
    Flags: recursesubdirs createallsubdirs ignoreversion

; Standalone application -> Program Files\FLUXCORE-12
Source: "{#BuildDir}\Standalone\{#AppName}.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
; Start Menu shortcut to the standalone
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppName}.exe"

[Run]
Filename: "{app}\{#AppName}.exe"; Description: "Launch {#AppName}"; \
    Flags: nowait postinstall skipifsilent
