; Inno Setup script for the Windows installer.
;   iscc /DAppVersion=0.1.0-beta1 /DBuildDir=<path to HalcyonReverb_artefacts\Release> /DOutputDir=<dir> Reblxerd.iss

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\build\HalcyonReverb_artefacts\Release"
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif

[Setup]
AppId={{6B0E3B52-5C1D-4E8A-9A57-2F4C7D1B8E31}
AppName=Reblxerd
AppVersion={#AppVersion}
AppPublisher=CreativeSpxce
DefaultDirName={autopf}\Reblxerd
DefaultGroupName=Reblxerd
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=Reblxerd-{#AppVersion}-win-setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
InfoBeforeFile=Read Me.txt
UninstallDisplayName=Reblxerd

[Types]
Name: "full"; Description: "VST3 plug-in and standalone app"
Name: "custom"; Description: "Choose"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in (Ableton Live, FL Studio, and other DAWs)"; Types: full custom; Flags: fixed
Name: "app"; Description: "Standalone app (use without a DAW)"; Types: full

[Files]
Source: "{#BuildDir}\VST3\Reblxerd.vst3\*"; DestDir: "{commoncf64}\VST3\Reblxerd.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\Standalone\Reblxerd.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Reblxerd"; Filename: "{app}\Reblxerd.exe"; Components: app

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Reblxerd.vst3"
