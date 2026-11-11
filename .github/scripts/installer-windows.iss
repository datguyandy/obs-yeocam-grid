; Windows installer for the plugin (Inno Setup 6.3 or newer).
;
; Package-Windows.ps1 builds it in CI. To build by hand:
;   ISCC /DVersion=0.2.1 /DSourceDir=<folder with yeocam-grid.dll and data\>
;        /DOutputDir=<output folder> .github\scripts\installer-windows.iss
;
; Installs to the OBS 33+ plugin location for all users:
;   C:\ProgramData\obs-studio\plugins\<PluginName>\<PluginName>.dll
;   C:\ProgramData\obs-studio\plugins\<PluginName>\data\...

#ifndef PluginName
  #define PluginName "yeocam-grid"
#endif
#ifndef DisplayName
  #define DisplayName "Yeocam Grid"
#endif
#ifndef Version
  #define Version "0.0.0"
#endif
#ifndef Publisher
  #define Publisher "Yeocam Grid contributors"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\release\" + PluginName
#endif
#ifndef OutputDir
  #define OutputDir "..\..\release"
#endif
#ifndef OutputName
  #define OutputName PluginName + "-" + Version + "-windows-x64-installer"
#endif
; Oldest OBS Studio major version the plugin loads in.
#define MinObsMajor 33

[Setup]
; Never change AppId: it is how upgrades and the uninstaller find this plugin.
AppId={{8CF3D9C1-32F6-4B9B-925F-FCDF8C8D194B}
AppName={#DisplayName}
AppVersion={#Version}
AppVerName={#DisplayName} {#Version}
AppPublisher={#Publisher}
VersionInfoVersion={#Version}
VersionInfoDescription={#DisplayName} plugin for OBS Studio
DefaultDirName={commonappdata}\obs-studio\plugins\{#PluginName}
DisableDirPage=yes
DisableProgramGroupPage=yes
UsePreviousAppDir=no
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename={#OutputName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes
CloseApplications=no
UninstallDisplayName={#DisplayName} for OBS Studio
UninstallFilesDir={app}\uninstall

[Messages]
WelcomeLabel2=This will install [name/ver] on your computer.%n%nYeocam Grid is an OBS Studio source that repeats one camera in a grid of tiles. It needs OBS Studio {#MinObsMajor} or newer.%n%nPlease close OBS Studio before continuing.
FinishedLabelNoIcons=[name] is installed.%n%nIn OBS, click + under Sources and choose {#DisplayName}.

[InstallDelete]
; Data from an older version (removed files would otherwise linger).
Type: filesandordirs; Name: "{app}\data"
; Pre-OBS 33 layout inside the same ProgramData folder.
Type: filesandordirs; Name: "{app}\bin"
; Legacy copies in the OBS program folder (OBS 34 stops loading these, and two
; copies of the same plugin conflict).
Type: files; Name: "{code:ObsDir}\obs-plugins\64bit\{#PluginName}.dll"
Type: files; Name: "{code:ObsDir}\obs-plugins\64bit\{#PluginName}.pdb"
Type: filesandordirs; Name: "{code:ObsDir}\data\obs-plugins\{#PluginName}"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: dirifempty; Name: "{app}"

[Run]
Filename: "{code:ObsDir}\bin\64bit\obs64.exe"; WorkingDir: "{code:ObsDir}\bin\64bit"; \
  Description: "Start OBS Studio"; \
  Flags: postinstall nowait skipifsilent skipifdoesntexist runasoriginaluser

[Code]
function FindObsDir(var Dir: String): Boolean;
begin
  Result := RegQueryStringValue(HKLM64, 'SOFTWARE\OBS Studio', '', Dir) and (Dir <> '');
end;

function ObsDir(Param: String): String;
begin
  if not FindObsDir(Result) then
    Result := ExpandConstant('{commonpf64}\obs-studio');
end;

function IsObsRunning(): Boolean;
var
  Locator, Service, Processes: Variant;
begin
  Result := False;
  try
    Locator := CreateOleObject('WbemScripting.SWbemLocator');
    Service := Locator.ConnectServer('.', 'root\CIMV2');
    Processes := Service.ExecQuery(
      'SELECT ProcessId FROM Win32_Process WHERE Name = ''obs64.exe''');
    Result := Processes.Count > 0;
  except
    Log('Could not check for a running OBS: ' + GetExceptionMessage);
  end;
end;

{ OBS locks the plugin DLL while it runs. Ask until it is closed or the user gives up. }
function WaitForObsToClose(): Boolean;
begin
  Result := True;
  while IsObsRunning() do
    if SuppressibleMsgBox('OBS Studio is running.' + #13#10#13#10 +
        'Close OBS Studio, then click Retry.', mbError, MB_RETRYCANCEL, IDCANCEL) = IDCANCEL then
    begin
      Result := False;
      Exit;
    end;
end;

function InitializeSetup(): Boolean;
var
  Dir, Version: String;
  Major: Integer;
begin
  Result := True;
  if not FindObsDir(Dir) then
  begin
    Result := SuppressibleMsgBox('OBS Studio does not seem to be installed.' + #13#10#13#10 +
      'Install {#DisplayName} anyway?', mbConfirmation, MB_YESNO, IDYES) = IDYES;
    Exit;
  end;

  if GetVersionNumbersString(Dir + '\bin\64bit\obs64.exe', Version) then
  begin
    Major := StrToIntDef(Copy(Version, 1, Pos('.', Version) - 1), 0);
    Log(Format('Found OBS Studio %s in %s', [Version, Dir]));
    if (Major > 0) and (Major < {#MinObsMajor}) then
      Result := SuppressibleMsgBox('{#DisplayName} needs OBS Studio {#MinObsMajor} or newer, ' +
        'and this computer has OBS Studio ' + Version + '. The plugin will not load until you update OBS.' + #13#10#13#10 +
        'Install anyway?', mbConfirmation, MB_YESNO, IDNO) = IDYES;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  if WaitForObsToClose() then
    Result := ''
  else
    Result := 'OBS Studio is still running. Close it and run Setup again.';
end;

function InitializeUninstall(): Boolean;
begin
  Result := WaitForObsToClose();
end;
