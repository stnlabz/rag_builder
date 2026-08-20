; rag_builder
; STN-LABZ Windows Installer
; Inno Setup
;
; Expected installer-source layout:
;
;   rag_builder.iss
;   rag_builder.exe
;   modules\
;       markdown\
;           markdown.dll
;           module.conf
;           ...
;       chunker\
;           chunker.dll
;           module.conf
;           ...
;       provenance\
;           provenance.dll
;           module.conf
;           ...
;       corpus_aggregator\
;           corpus_aggregator.dll
;           module.conf
;           ...
;
; Any additional files placed beneath modules\ are installed recursively
; with their owning module.

#define MyAppName "Rag Builder"
#define MyAppVersion "0.3"
#define MyAppPublisher "STN-LABZ, LLC."
#define MyAppURL "https://www.stn-labz.com"
#define MyAppExeName "rag_builder.exe"
#define DoubleAmp(Value) StringChange(Value, "&", "&&")
#define EscapeConstArgument(Value) StringChange(StringChange(StringChange(Value, "%", "%25"), ",", "%2c"), "}", "%7d")

[Setup]

; Unique rag_builder application identifier.
AppId={{AC8BD338-186F-43E2-87B2-CB06333416E0}}

AppName={#MyAppName}
AppVersion={#MyAppVersion}

AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}

; Install into native Program Files.
DefaultDirName={autopf}\STN-Labz\Rag Builder

DisableDirPage=yes

UninstallDisplayIcon={app}\{#MyAppExeName}

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

DefaultGroupName={#MyAppName}

; Administrative rights are required because rag_builder is installed
; system-wide and added to the system PATH.
PrivilegesRequired=admin

; Tell Windows that Setup modifies environment variables.
ChangesEnvironment=yes

OutputDir=output
OutputBaseFilename=rag_builder-{#MyAppVersion}-Setup

Compression=lzma2
SolidCompression=yes

WizardStyle=modern windows11


[Languages]

Name: "english"; MessagesFile: "compiler:Default.isl"


[Files]

; rag_builder Core.
Source: "rag_builder.exe"; \
    DestDir: "{app}"; \
    Flags: ignoreversion

; Install the complete module tree.
;
; This deliberately installs every file belonging to each module, including
; DLLs, module.conf files, contracts, and other module-owned support files.

; Optional project-level runtime/configuration files.
; These entries do not fail compilation when the files/directories are absent.

Source: "README.md"; \
    DestDir: "{app}"; \
    Flags: ignoreversion skipifsourcedoesntexist

Source: "LICENSE"; \
    DestDir: "{app}"; \
    Flags: ignoreversion skipifsourcedoesntexist

Source: "CHANGELOG.md"; \
    DestDir: "{app}"; \
    Flags: ignoreversion skipifsourcedoesntexist


[Dirs]

; Operational state remains outside Program Files.
;
; These directories are created when absent but are intentionally preserved
; during uninstall. The installer never deletes RAG input/output or policy
; authority state.
Name: "C:\stn-labz\rag"; Flags: uninsneveruninstall
Name: "C:\stn-labz\rag\input"; Flags: uninsneveruninstall
Name: "C:\stn-labz\rag\output"; Flags: uninsneveruninstall
Name: "C:\stn-labz\policies"; Flags: uninsneveruninstall


[Icons]

Name: "{group}\{#MyAppName}"; \
    Filename: "{app}\{#MyAppExeName}"


[Registry]

; Add rag_builder to the system PATH.
;
; Do NOT use uninsdeletevalue here.
; Deleting the Path value would destroy the entire Windows PATH.
Root: HKLM; \
    Subkey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; \
    ValueType: expandsz; \
    ValueName: "Path"; \
    ValueData: "{olddata};{app}"; \
    Check: NeedsAddPath(ExpandConstant('{app}'))


[Run]

; Optional post-install launch.
; For a CLI utility this is intentionally unchecked by default.
Filename: "{app}\{#MyAppExeName}"; \
    Description: "{cm:LaunchProgram,{#DoubleAmp(MyAppName)}}"; \
    WorkingDir: "{app}"; \
    Flags: nowait postinstall skipifsilent unchecked


[Code]

function NeedsAddPath(Param: string): Boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(
    HKLM,
    'SYSTEM\CurrentControlSet\Control\Session Manager\Environment',
    'Path',
    OrigPath
  ) then
  begin
    Result := True;
    Exit;
  end;

  Result :=
    Pos(
      ';' + Uppercase(Param) + ';',
      ';' + Uppercase(OrigPath) + ';'
    ) = 0;
end;


procedure RemovePath(PathToRemove: string);
var
  PathValue: string;
  SearchValue: string;
begin
  if RegQueryStringValue(
    HKLM,
    'SYSTEM\CurrentControlSet\Control\Session Manager\Environment',
    'Path',
    PathValue
  ) then
  begin
    SearchValue := ';' + PathToRemove;

    StringChangeEx(
      PathValue,
      SearchValue,
      '',
      True
    );

    if Pos(PathToRemove + ';', PathValue) = 1 then
      Delete(PathValue, 1, Length(PathToRemove) + 1);

    if CompareText(PathValue, PathToRemove) = 0 then
      PathValue := '';

    RegWriteExpandStringValue(
      HKLM,
      'SYSTEM\CurrentControlSet\Control\Session Manager\Environment',
      'Path',
      PathValue
    );
  end;
end;


procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
  begin
    RemovePath(ExpandConstant('{app}'));
  end;
end;
