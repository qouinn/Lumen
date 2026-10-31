; Lumen 1.5 — Windows installer (NSIS)
Unicode true
SetCompressor /SOLID lzma
ManifestDPIAware true

!define APPNAME   "Lumen"
!define VERSION   "1.5.0"
!define PUBLISHER "Grainfield Audio"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Lumen"
!ifndef BINDIR
  !define BINDIR "..\bin\windows"
!endif
!ifndef PAYLOAD
  !define PAYLOAD "..\dist\payload-lumen"
!endif
!ifndef OUTFILE
  !define OUTFILE "..\dist\Lumen-1.5-Windows-Setup.exe"
!endif

Name "${APPNAME} ${VERSION}"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\${APPNAME}"
RequestExecutionLevel admin
BrandingText "${APPNAME} ${VERSION} - ${PUBLISHER}"

VIProductVersion "1.5.0.0"
VIAddVersionKey "ProductName" "${APPNAME}"
VIAddVersionKey "CompanyName" "${PUBLISHER}"
VIAddVersionKey "FileDescription" "${APPNAME} sample effect instrument installer"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "(c) 2026 ${PUBLISHER}"

!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"

!define MUI_ICON   "lumen.ico"
!define MUI_UNICON "lumen.ico"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Install Lumen ${VERSION}"
!define MUI_WELCOMEPAGE_TEXT "Lumen is a sample effect instrument: load one sound and play it through 267 effect presets in 13 categories plus your own - granular clouds, shimmer, tape, glitch, breakcore, dreamy, cinematic, sci-fi, beautiful transforms and more - shaped with four big knobs. Drag a sample from FL Studio's browser straight onto it.$\r$\n$\r$\nThis will install the VST3 and CLAP plugins for 64-bit Windows. Installing over an older Lumen is fine - your projects keep their sounds.$\r$\n$\r$\nClose your DAW before continuing."
!define MUI_COMPONENTSPAGE_SMALLDESC
!define MUI_FINISHPAGE_TITLE "Lumen is installed"
!define MUI_FINISHPAGE_TEXT "Rescan plugins in your DAW.$\r$\n$\r$\nFL Studio: Options > Manage plugins > Find installed plugins. Lumen appears under Generators."
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\Lumen-Manual.pdf"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Open the manual"
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${PAYLOAD}\LICENSE.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "German"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "Lumen needs 64-bit Windows 10 or 11."
    Abort
  ${EndIf}
  SetRegView 64
  !insertmacro MUI_LANGDLL_DISPLAY
FunctionEnd

Function un.onInit
  SetRegView 64
FunctionEnd

Section "!Core files and manual" SecCore
  SectionIn RO
  SetOutPath "$INSTDIR"
  File "${PAYLOAD}\Lumen-Manual.pdf"
  File "${PAYLOAD}\LICENSE.txt"
  File "${PAYLOAD}\THIRD-PARTY.txt"
  File "lumen.ico"
  WriteUninstaller "$INSTDIR\Uninstall Lumen.exe"

  CreateDirectory "$SMPROGRAMS\${APPNAME}"
  CreateShortcut "$SMPROGRAMS\${APPNAME}\Lumen Manual.lnk" "$INSTDIR\Lumen-Manual.pdf"
  CreateShortcut "$SMPROGRAMS\${APPNAME}\Uninstall Lumen.lnk" "$INSTDIR\Uninstall Lumen.exe"

  WriteRegStr HKLM "${UNINSTKEY}" "DisplayName" "${APPNAME} ${VERSION}"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNINSTKEY}" "Publisher" "${PUBLISHER}"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayIcon" "$INSTDIR\lumen.ico"
  WriteRegStr HKLM "${UNINSTKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINSTKEY}" "UninstallString" '"$INSTDIR\Uninstall Lumen.exe"'
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoRepair" 1
  WriteRegDWORD HKLM "${UNINSTKEY}" "EstimatedSize" 4500
SectionEnd

Section "VST3 plugin (FL Studio, Ableton, Cubase, ...)" SecVST3
  SetOutPath "$COMMONFILES64\VST3\Lumen.vst3\Contents\x86_64-win"
  File "${BINDIR}\Lumen.vst3\Contents\x86_64-win\Lumen.vst3"
  SetOutPath "$COMMONFILES64\VST3\Lumen.vst3\Contents\Resources"
  File "lumen.ico"
  WriteRegStr HKLM "Software\${PUBLISHER}\${APPNAME}" "VST3" "$COMMONFILES64\VST3\Lumen.vst3"
SectionEnd

Section "CLAP plugin (Bitwig, Reaper, ...)" SecCLAP
  SetOutPath "$COMMONFILES64\CLAP"
  File "${BINDIR}\Lumen.clap"
  WriteRegStr HKLM "Software\${PUBLISHER}\${APPNAME}" "CLAP" "$COMMONFILES64\CLAP\Lumen.clap"
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecCore} "Manual, licence texts and the uninstaller."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecVST3} "Installs Lumen.vst3 to Common Files\VST3 (the standard VST3 folder)."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecCLAP} "Installs Lumen.clap to Common Files\CLAP (the standard CLAP folder)."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
  Delete "$COMMONFILES64\VST3\Lumen.vst3\Contents\x86_64-win\Lumen.vst3"
  Delete "$COMMONFILES64\VST3\Lumen.vst3\Contents\Resources\lumen.ico"
  RMDir  "$COMMONFILES64\VST3\Lumen.vst3\Contents\x86_64-win"
  RMDir  "$COMMONFILES64\VST3\Lumen.vst3\Contents\Resources"
  RMDir  "$COMMONFILES64\VST3\Lumen.vst3\Contents"
  RMDir  "$COMMONFILES64\VST3\Lumen.vst3"
  Delete "$COMMONFILES64\CLAP\Lumen.clap"

  Delete "$SMPROGRAMS\${APPNAME}\Lumen Manual.lnk"
  Delete "$SMPROGRAMS\${APPNAME}\Uninstall Lumen.lnk"
  RMDir  "$SMPROGRAMS\${APPNAME}"

  Delete "$INSTDIR\Lumen-Manual.pdf"
  Delete "$INSTDIR\LICENSE.txt"
  Delete "$INSTDIR\THIRD-PARTY.txt"
  Delete "$INSTDIR\lumen.ico"
  Delete "$INSTDIR\Uninstall Lumen.exe"
  RMDir  "$INSTDIR"

  DeleteRegKey HKLM "${UNINSTKEY}"
  DeleteRegKey HKLM "Software\${PUBLISHER}\${APPNAME}"
  DeleteRegKey /ifempty HKLM "Software\${PUBLISHER}"
SectionEnd
