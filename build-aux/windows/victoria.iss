; Inno Setup script for Victoria Files (Windows x64)
#ifndef AppVersion
  #define AppVersion "1.47.8"
#endif

[Setup]
AppId={{8D6B5E7F-3D38-4E6B-9E3A-7E2D4C9A1B50}
AppName=Victoria Files
AppVersion={#AppVersion}
AppPublisher=Victoria Project
AppPublisherURL=https://github.com/justlimorina/victoria-files-manager
DefaultDirName={autopf}\Victoria Files
DefaultGroupName=Victoria Files
AllowNoIcons=yes
LicenseFile=..\..\LICENSE
OutputDir=..\..\dist
OutputBaseFilename=victoria-files-{#AppVersion}-win64-setup
Compression=lzma2/normal
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\..\dist\Victoria\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Victoria Files"; Filename: "{app}\bin\victoria-files.exe"; WorkingDir: "{app}\bin"
Name: "{group}\{cm:UninstallProgram,Victoria Files}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Victoria Files"; Filename: "{app}\bin\victoria-files.exe"; WorkingDir: "{app}\bin"; Tasks: desktopicon

[Run]
Filename: "{app}\bin\victoria-files.exe"; Description: "{cm:LaunchProgram,Victoria Files}"; Flags: nowait postinstall skipifsilent
