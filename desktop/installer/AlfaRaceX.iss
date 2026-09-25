#ifndef AppVersion
#define AppVersion "0.2.0"
#endif
[Setup]
AppId={{C1375F12-BDE5-4D2E-9234-4C8166A75FE2}
AppName=AlfaRaceX Desktop
AppVersion={#AppVersion}
AppPublisher=AlfaRaceX
AppPublisherURL=https://github.com/AriotaG/AlfaRaceX-Framework
DefaultDirName={autopf}\AlfaRaceX
DefaultGroupName=AlfaRaceX
DisableDirPage=no
DisableProgramGroupPage=no
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
WizardStyle=modern
OutputDir=..\..\artifacts
OutputBaseFilename=AlfaRaceX-Setup
Compression=lzma2
SolidCompression=yes
UninstallDisplayIcon={app}\AlfaRaceX.exe
CloseApplications=yes
RestartApplications=no
SetupLogging=yes
VersionInfoVersion={#AppVersion}
[Languages]
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"
[Tasks]
Name: "desktopicon"; Description: "Crea un collegamento sul Desktop"; Flags: unchecked
[Files]
Source: "..\..\prerequisites\WebView2.exe"; Flags: dontcopy
Source: "..\..\publish\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\AlfaRaceX"; Filename: "{app}\AlfaRaceX.exe"
Name: "{autodesktop}\AlfaRaceX"; Filename: "{app}\AlfaRaceX.exe"; Tasks: desktopicon
[Run]
Filename: "{app}\AlfaRaceX.exe"; Description: "Avvia AlfaRaceX"; Flags: nowait postinstall skipifsilent runasoriginaluser
[Code]
function RuntimeInstalled: Boolean;
var Version: String;
begin
  Result := RegQueryStringValue(HKLM32, 'SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}', 'pv', Version) and (Version <> '') and (Version <> '0.0.0.0');
end;
function PrepareToInstall(var NeedsRestart: Boolean): String;
var InstalledVersion, NewVersion: Int64; ExitCode: Integer;
begin
  Result := '';
  if GetPackedVersion(ExpandConstant('{app}\AlfaRaceX.exe'), InstalledVersion) and StrToVersion('{#AppVersion}', NewVersion) then
    if ComparePackedVersion(InstalledVersion, NewVersion) > 0 then
      Result := 'Una versione più recente è già installata. Disinstallarla prima del downgrade. Backup e log rimangono conservati.';
  if (Result = '') and not RuntimeInstalled then
  begin
    ExtractTemporaryFile('WebView2.exe');
    if not Exec(ExpandConstant('{tmp}\WebView2.exe'), '/silent /install', '', SW_HIDE, ewWaitUntilTerminated, ExitCode) then
      Result := 'Impossibile avviare il prerequisito Microsoft WebView2.'
    else if not RuntimeInstalled then
      Result := 'Installazione WebView2 non completata. Codice: ' + IntToStr(ExitCode);
  end;
end;
