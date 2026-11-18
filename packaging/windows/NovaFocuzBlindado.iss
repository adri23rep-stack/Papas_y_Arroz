#define NovaName "Nova Focuz Blindado"
#define NovaVersion "0.4.0-lab"
#ifndef PortableDir
  #error "PortableDir must point to the verified portable Nova Qt folder"
#endif

[Setup]
AppId=NovaFocuzBlindado-Laboratorio-2026
AppName={#NovaName}
AppVersion={#NovaVersion}
AppVerName={#NovaName} {#NovaVersion}
AppPublisher=Proyecto Nova (laboratorio local)
DefaultDirName={localappdata}\Programs\NovaFocuzBlindado
DefaultGroupName={#NovaName}
UninstallDisplayIcon={app}\XuperTVNovaQt.exe
PrivilegesRequired=lowest
OutputBaseFilename=Nova-Focuz-Blindado-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
DisableProgramGroupPage=yes
DisableDirPage=no
SetupLogging=yes
CloseApplications=yes
RestartApplications=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile={#PortableDir}\LICENSE
; Existing private service.json/device_state.json never ship in this build
; and are never removed during upgrades/uninstall.

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "Crear acceso directo en el escritorio"; GroupDescription: "Accesos directos:"; Flags: unchecked

[Files]
Source: "{#PortableDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\tools\Importar-ConfigNovaLocal.ps1"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#NovaName}"; Filename: "{app}\XuperTVNovaQt.exe"; WorkingDir: "{app}"
Name: "{autoprograms}\Importar configuracion de Nova"; Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\Importar-ConfigNovaLocal.ps1"""; WorkingDir: "{app}"
Name: "{autodesktop}\{#NovaName}"; Filename: "{app}\XuperTVNovaQt.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\Importar-ConfigNovaLocal.ps1"""; Description: "Importar mi configuracion local de Nova (opcional)"; Flags: postinstall skipifsilent unchecked
Filename: "{app}\XuperTVNovaQt.exe"; Description: "Abrir {#NovaName}"; Flags: nowait postinstall skipifsilent unchecked
