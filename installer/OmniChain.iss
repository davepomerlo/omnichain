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
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\OmniChain.exe
CloseApplications=yes
SetupLogging=yes

[Types]
Name: "full"; Description: "Standalone app and DAW plugins"
Name: "app"; Description: "Standalone app only"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "app"; Description: "OmniChain app (play without a DAW)"; Types: full app custom
Name: "vst3"; Description: "VST3 plugin (FL Studio, Ableton, Cubase, Reaper, Studio One...)"; Types: full
Name: "clap"; Description: "CLAP plugin (Bitwig, Reaper, Studio One 7)"; Types: full
Name: "library"; Description: "Factory library: 58 amp and pedal captures + 102 impulse responses (about 27 MB)"; Types: full app custom

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Components: app

[Files]
Source: "{#Art}\Standalone\OmniChain.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: app
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion; Components: app
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Art}\VST3\OmniChain.vst3\*"; DestDir: "{commoncf64}\VST3\OmniChain.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
Source: "{#Art}\CLAP\OmniChain.clap"; DestDir: "{commoncf64}\CLAP"; Flags: ignoreversion; Components: clap
Source: "..\OmniChain Library\*"; DestDir: "{commonappdata}\OmniChain\OmniChain Library"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: library

[Icons]
Name: "{group}\OmniChain"; Filename: "{app}\OmniChain.exe"; Components: app
Name: "{group}\Uninstall OmniChain"; Filename: "{uninstallexe}"
Name: "{autodesktop}\OmniChain"; Filename: "{app}\OmniChain.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\OmniChain.exe"; Description: "Start OmniChain now"; Flags: nowait postinstall skipifsilent; Components: app

[Code]
{ Windows error 183 ("already exists") happens when an older, hand-copied OmniChain left a FILE where the
  installer needs a FOLDER (for example a single OmniChain.vst3 file copied out of the bundle), or a folder
  where it needs a file. Clear those leftovers before copying. Only OmniChain's own paths are touched. }

procedure NeedFolder(const P: String);
begin
  if FileExists(P) and not DirExists(P) then
    if DeleteFile(P) then Log('Removed old file in the way of a folder: ' + P)
    else Log('Could not remove ' + P);
end;

procedure NeedFile(const P: String);
begin
  if DirExists(P) then
    if DelTree(P, True, True, True) then Log('Removed old folder in the way of a file: ' + P)
    else Log('Could not remove ' + P);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  Vst3: String;
begin
  Result := '';
  NeedFolder(ExpandConstant('{app}'));
  NeedFile(ExpandConstant('{app}\OmniChain.exe'));
  NeedFolder(ExpandConstant('{commonappdata}\OmniChain'));
  NeedFolder(ExpandConstant('{commonappdata}\OmniChain\OmniChain Library'));
  if WizardIsComponentSelected('vst3') then
  begin
    Vst3 := ExpandConstant('{commoncf64}\VST3\OmniChain.vst3');
    NeedFolder(ExpandConstant('{commoncf64}\VST3'));
    NeedFolder(Vst3);
    NeedFolder(Vst3 + '\Contents');
    NeedFolder(Vst3 + '\Contents\x86_64-win');
    NeedFolder(Vst3 + '\Contents\Resources');
  end;
  if WizardIsComponentSelected('clap') then
  begin
    NeedFolder(ExpandConstant('{commoncf64}\CLAP'));
    NeedFile(ExpandConstant('{commoncf64}\CLAP\OmniChain.clap'));
  end;
  if FileExists(ExpandConstant('{commoncf64}\VST3\OmniChain.vst3')) then
    Result := 'An old OmniChain.vst3 file in ' + ExpandConstant('{commoncf64}\VST3') + ' could not be replaced. ' +
              'Close your DAW, delete that file, and run the installer again.';
end;
