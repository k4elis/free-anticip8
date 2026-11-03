; Inno Setup — FREE ANTICIP8 (VST3 64-bit)
#define MyAppName "FREE ANTICIP8"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "FREE AUDIO"

[Setup]
AppId={{A8F01C8E-7B2D-4E11-9C3A-FREEANTICIP8}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={commoncf64}\VST3\{#MyAppName}.vst3
DisableDirPage=yes
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\..\dist\windows
OutputBaseFilename=FREE-ANTICIP8-Setup-Windows
Compression=lzma
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName={#MyAppName}
SetupIconFile=

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; Bundle VST3 complet (dossier Contents\...)
Source: "payload\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Desinstaller {#MyAppName}"; Filename: "{uninstallexe}"

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
end;
