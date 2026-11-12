; Inno Setup script for the OmniChain Windows installer (built in CI)
#define AppVersion GetEnv("OMNI_VERSION")
#if AppVersion == ""
  #define AppVersion "dev"
#endif
#define Art "..\build\OmniChain_artefacts\Release"

[Setup]
AppId={{6C1C0B7E-2F4B-4E6E-9A51-0F7A2C3D9B11}
AppName=OmniChain
AppVersion={#AppVersion}
AppPublisher=OmniChain
AppPublisherURL=https://davepomerlo.github.io/omnichain/
DefaultDirName={autopf}\OmniChain
DefaultGroupName=OmniChain
DisableProgramGroupPage=yes
OutputDir=..
OutputBaseFilename=OmniChain-Windows-Setup
LicenseFile=..\LICENSE
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\OmniChain.exe

[Types]
Name: "full"; Description: "Standalone app and DAW plugins"
Name: "app"; Description: "Standalone app only"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "app"; Description: "OmniChain app (play without a DAW)"; Types: full app custom
Name: "vst3"; Description: "VST3 plugin (FL Studio, Ableton, Cubase, Reaper, Studio One...)"; Types: full
Name: "clap"; Description: "CLAP plugin (Bitwig, Reaper, Studio One 7)"; Types: full

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Components: app

[Files]
Source: "{#Art}\Standalone\OmniChain.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: app
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion; Components: app
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Art}\VST3\OmniChain.vst3\*"; DestDir: "{commoncf64}\VST3\OmniChain.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
Source: "{#Art}\CLAP\OmniChain.clap"; DestDir: "{commoncf64}\CLAP"; Flags: ignoreversion; Components: clap

[Icons]
Name: "{group}\OmniChain"; Filename: "{app}\OmniChain.exe"; Components: app
Name: "{group}\Uninstall OmniChain"; Filename: "{uninstallexe}"
Name: "{autodesktop}\OmniChain"; Filename: "{app}\OmniChain.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\OmniChain.exe"; Description: "Start OmniChain now"; Flags: nowait postinstall skipifsilent; Components: app
