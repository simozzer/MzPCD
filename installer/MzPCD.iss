;
; The Windows installer: the VST3 in a Setup.exe.
;
;     .\build.ps1 -Release         builds the plugin, then this, into dist\
;
; build.ps1 passes the version read from CMakeLists.txt, so it is written down once. Compiled
; by hand without it, the installer says 0.0.0 rather than a version it was not built from.
;
; Setup asks whether to install for everyone or just this user. Everyone (the default) puts
; the VST3 in C:\Program Files\Common Files\VST3, which every DAW scans; just me needs no
; administrator rights and uses %LOCALAPPDATA%\Programs\Common\VST3, the folder build.ps1
; installs to, which some hosts need adding to their plugin paths.
;
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

#define Artefacts "..\build\MzPCD_artefacts\Release"

[Setup]
; The installer's identity, as the plugin codes are the plugin's: never change it, or a new
; version installs beside the old one rather than upgrading it.
AppId={{14BB2CB3-68B0-4F2C-8B5C-3B0DCF4A11A5}
AppName=MzPCD
AppVersion={#AppVersion}
AppVerName=MzPCD {#AppVersion}
AppPublisher=simozzer
AppPublisherURL=https://github.com/simozzer/MzPCD
AppSupportURL=https://github.com/simozzer/MzPCD/issues
AppUpdatesURL=https://github.com/simozzer/MzPCD/releases
AppCopyright=Created by Simon Moscrop
VersionInfoVersion={#AppVersion}

; Nothing goes here but the licence and the uninstaller - the plugin goes in the VST3 folder.
DefaultDirName={autopf}\MzPCD
DisableDirPage=yes
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE

PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog commandline

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0

OutputDir=..\dist
OutputBaseFilename=MzPCD-{#AppVersion}-win64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=MzPCD {#AppVersion}

; A DAW with the plugin loaded holds MzPCD.vst3 open; let Setup offer to close it.
CloseApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; {autocf} is Common Files for everyone and %LOCALAPPDATA%\Programs\Common for just me.
Source: "{#Artefacts}\VST3\MzPCD.vst3\*"; DestDir: "{autocf}\VST3\MzPCD.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
