; The Windows installer for the Patreon builds: Inno Setup 6, built by build-binaries.yml from the
; same folder as the ZIP (AnimatekNME.exe, README.txt, LICENSE.txt, BUILD-INFO.txt).
;
;   iscc /DAppVersion=0.25.0 /DStageDir=..\..\stage packaging\windows\AnimatekNME.iss
;
; Installed for the current user, in %LOCALAPPDATA%\Programs: no administrator, no UAC prompt.
; A new version replaces the previous one in place. Settings, themes and patches live in the
; user's profile and documents, and neither installing nor uninstalling touches them.

#ifndef AppVersion
  #define AppVersion "dev"
#endif
#ifndef StageDir
  #define StageDir "..\..\stage"
#endif

[Setup]
; Fixed for good: it is how Windows knows a new version replaces an installed one.
AppId={{B6713B37-00E4-40E0-85BE-3C04C218DAB6}
AppName=Animatek NME
AppVersion={#AppVersion}
AppVerName=Animatek NME {#AppVersion}
AppPublisher=Animatek
AppPublisherURL=https://animatek.net/animatek-nme-eng/
AppSupportURL=https://github.com/animatek/Animatek-NME/issues
DefaultDirName={autopf}\Animatek NME
DefaultGroupName=Animatek NME
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; An editor left open holds its exe: offer to close it rather than fail half-way.
CloseApplications=yes
RestartApplications=no
UninstallDisplayIcon={app}\AnimatekNME.exe
UninstallDisplayName=Animatek NME
OutputDir=.
OutputBaseFilename=AnimatekNME-{#AppVersion}-windows-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "A desktop shortcut"; Flags: unchecked

[Files]
Source: "{#StageDir}\AnimatekNME.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\README.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\BUILD-INFO.txt"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\Animatek NME"; Filename: "{app}\AnimatekNME.exe"
Name: "{group}\Uninstall Animatek NME"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Animatek NME"; Filename: "{app}\AnimatekNME.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\AnimatekNME.exe"; Description: "Start Animatek NME"; Flags: nowait postinstall skipifsilent
