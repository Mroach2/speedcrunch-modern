#define APPTITLE "SpeedCrunch"
; Staged install tree produced by `cmake --install build --prefix stage`
; (SpeedCrunch.exe plus the deployed Qt runtime).
#define STAGEDIR "..\stage"
#define SPEEDCRUNCHEXE STAGEDIR + "\" + APPTITLE + ".exe"
#define NUMERICVERSION GetFileVersion(SPEEDCRUNCHEXE)
#define VERSION "1.0" ; GetFileVersionString(SPEEDCRUNCHEXE)
#define URL "http://speedcrunch.org"
#define COPYRIGHT "2004-2013 " + URL

[Setup]
AllowNoIcons=yes
AppName={#APPTITLE}
AppPublisher={#APPTITLE}
AppPublisherURL={#URL}
AppSupportURL={#URL}
AppUpdatesURL={#URL}
AppVerName={#APPTITLE} {#VERSION}
Compression=lzma/ultra
DefaultDirName={autopf}\{#APPTITLE}
DefaultGroupName={#APPTITLE}
DisableProgramGroupPage=true
LicenseFile=COPYING.rtf
InternalCompressLevel=ultra
OutputBaseFilename={#APPTITLE}-windows-x64-setup
OutputDir=.
ShowLanguageDialog=no
SolidCompression=yes
VersionInfoCompany={#URL}
VersionInfoCopyright=Copyright (C) {#COPYRIGHT}
VersionInfoDescription=Keyboard-oriented high-precision scientific calculator
VersionInfoTextVersion={#VERSION}
VersionInfoVersion={#NUMERICVERSION}

[Languages]
; Installer wizard in English only — the app itself ships all translations.
; The Inno 5-era language list referenced .isl files Inno 6 no longer bundles.
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#STAGEDIR}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\{#APPTITLE}"; Filename: "{app}\{#APPTITLE}.exe"
Name: "{group}\{cm:UninstallProgram,{#APPTITLE}}"; Filename: "{uninstallexe}"
Name: "{userdesktop}\{#APPTITLE}"; Filename: "{app}\{#APPTITLE}.exe"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\{#APPTITLE}"; Filename: "{app}\{#APPTITLE}.exe"; Tasks: quicklaunchicon

[Run]
Filename: "{app}\{#APPTITLE}.exe"; Description: "{cm:LaunchProgram,{#APPTITLE}}"; Flags: nowait postinstall skipifsilent
