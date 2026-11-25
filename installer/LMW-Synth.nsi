; SPDX-License-Identifier: AGPL-3.0-or-later
; LMW-Synth Windows installer (Spec S9.5, Plan Phase 13b). NSIS 3, Unicode.
; Built by `cmake --build <build> --config Release --target installer`, which passes:
;   VERSION   e.g. 0.13.1
;   STAGE     folder with LMW-Synth.exe, LMW-Synth.vst3 (bundle folder), lmw9p.exe, lmw-mcp.exe, LICENSE.txt and
;             docs (README, Tutorial, THIRD_PARTY, Terms as HTML with their images, scripts/md2html.py, S9.8)
;   OUTFILE   the installer to write
;
; Install for (a page, or /CURRENTUSER or /ALLUSERS on the command line):
;   Just me    no administrator rights: the app in %LOCALAPPDATA%\Programs\LMW-Synth, the VST3 plugin in
;              %LOCALAPPDATA%\Programs\Common\VST3 (Steinberg's per-user folder), Start menu and the
;              Apps & features entry for this user (HKCU).
;   All users  the installer starts itself again with administrator rights (UAC): the app in
;              Program Files\LMW-Synth, the VST3 plugin in C:\Program Files\Common Files\VST3 (scanned
;              by every DAW), Start menu and Apps & features for all users (HKLM).
; Installing over an older version replaces it; an older installer over a newer version asks first
; (a silent install refuses). LMW-Synth installed in the other mode is refused (a DAW would find two
; copies of the plugin). Install and uninstall wait until the standalone and every DAW that has the
; plugin loaded are closed. Patches, presets and settings (Documents, %APPDATA%) stay on uninstall.
; Silent: /S [/ALLUSERS|/CURRENTUSER] [/D=folder]; exit code 2 when it refuses (nothing changed).

Unicode true
ManifestDPIAware true
RequestExecutionLevel user
!include "MUI2.nsh"
!include "nsDialogs.nsh"
!include "LogicLib.nsh"
!include "WordFunc.nsh"
!include "FileFunc.nsh"
!include "x64.nsh"

; The Windows makensis splits a `File` path at the last backslash (mini-daw crosscheck #8);
; the Linux one at '/'.
!ifdef NSIS_WIN32_MAKENSIS
  !define SEP "\"
!else
  !define SEP "/"
!endif
!ifndef VERSION
  !error "VERSION is not defined"
!endif

!define PRODUCT "LMW-Synth"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\LMW-Synth"
!define PRODUCT_KEY "Software\LMW-Synth"
!define VST3_BINARY "LMW-Synth.vst3\Contents\x86_64-win\LMW-Synth.vst3"

Name "${PRODUCT} ${VERSION}"
OutFile "${OUTFILE}"
InstallDir "$LOCALAPPDATA\Programs\LMW-Synth"
SetCompressor /SOLID lzma
BrandingText "${PRODUCT} ${VERSION}"
ShowInstDetails show

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "${PRODUCT}"
VIAddVersionKey "FileDescription" "${PRODUCT} installer"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "LMW-Synth authors. GNU AGPL-3.0-or-later."
VIAddVersionKey "CompanyName" "LMW"

Var AllUsers        ; "1" all users, "0" just me
Var ModeFixed       ; "1" when the command line chose the mode (no page)
Var Elevated        ; "1" in the copy started again with administrator rights
Var IsAdmin
Var VstDir
Var RadioMe
Var RadioAll

; ---------------------------------------------------------------------------------------------
; Helpers shared by installer and uninstaller.

; A running program or a loaded plugin cannot be replaced or removed: opening the file for writing
; fails then. Ask to close it; silent: give up (exit code 2).
!macro WAIT_UNTIL_FREE FILE WHAT
  !define ID ${__LINE__}
  retry_${ID}:
  IfFileExists "${FILE}" 0 done_${ID}
  ClearErrors
  FileOpen $9 "${FILE}" a
  IfErrors busy_${ID}
  FileClose $9
  Goto done_${ID}
  busy_${ID}:
  MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "${WHAT}$\n$\nClose it, then choose Retry." /SD IDCANCEL IDRETRY retry_${ID}
  SetErrorLevel 2
  Abort "${PRODUCT} is in use; nothing was changed."
  done_${ID}:
  !undef ID
!macroend

!macro CHECK_ADMIN
  StrCpy $IsAdmin "0"
  UserInfo::GetAccountType
  Pop $0
  StrCmp $0 "Admin" 0 +2
  StrCpy $IsAdmin "1"
!macroend

; ---------------------------------------------------------------------------------------------
; Installer.

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "${PRODUCT} needs 64-bit Windows." /SD IDOK
    SetErrorLevel 2
    Abort
  ${EndIf}
  SetRegView 64
  !insertmacro CHECK_ADMIN
  StrCpy $ModeFixed "0"
  StrCpy $Elevated "0"
  ${GetParameters} $R0
  ClearErrors
  ${GetOptions} $R0 "/ELEVATED" $R1
  ${IfNot} ${Errors}
    StrCpy $Elevated "1"
  ${EndIf}
  ClearErrors
  ${GetOptions} $R0 "/ALLUSERS" $R1
  ${IfNot} ${Errors}
    StrCpy $AllUsers "1"
    StrCpy $ModeFixed "1"
  ${Else}
    ClearErrors
    ${GetOptions} $R0 "/CURRENTUSER" $R1
    ${IfNot} ${Errors}
      StrCpy $AllUsers "0"
      StrCpy $ModeFixed "1"
    ${Else}
      ; Default: the mode of an existing installation, else just me.
      StrCpy $AllUsers "0"
      ReadRegStr $1 HKLM "${UNINSTALL_KEY}" "DisplayVersion"
      StrCmp $1 "" +2
      StrCpy $AllUsers "1"
      ${If} ${Silent}
        StrCpy $ModeFixed "1"
      ${EndIf}
    ${EndIf}
  ${EndIf}
  ${If} $ModeFixed == "1"
    ${If} $AllUsers == "1"
    ${AndIf} $IsAdmin != "1"
      MessageBox MB_ICONSTOP "Installing for all users needs administrator rights." /SD IDOK
      SetErrorLevel 2
      Abort
    ${EndIf}
    Call CheckOtherMode
    Call ApplyMode
  ${EndIf}
FunctionEnd

; LMW-Synth installed in the other mode: refuse (two plugins with one ID would confuse a DAW).
; Returns with the error flag set when refused (page: stay; .onInit: quit).
Function CheckOtherMode
  ${If} $AllUsers == "1"
    ReadRegStr $1 HKCU "${UNINSTALL_KEY}" "DisplayVersion"
    StrCpy $2 "just for you"
  ${Else}
    ReadRegStr $1 HKLM "${UNINSTALL_KEY}" "DisplayVersion"
    StrCpy $2 "for all users"
  ${EndIf}
  ; ReadRegStr sets the error flag when the value is missing (not installed in the other mode, the
  ; usual case): clear it here, so only a refusal below sets it (1.2.0 bug: Next did nothing).
  ClearErrors
  ${If} $1 != ""
    MessageBox MB_ICONEXCLAMATION "${PRODUCT} $1 is already installed $2.$\n$\nRemove it first (Settings, Apps, ${PRODUCT}) or choose that option here, so a DAW does not find the plugin twice." /SD IDOK
    ${If} ${Silent}
    ${OrIf} $ModeFixed == "1"
      SetErrorLevel 2
      Abort
    ${EndIf}
    SetErrors
  ${EndIf}
FunctionEnd

; Folders, shell context and registry root for the chosen mode; an older installer over a newer
; installation asks first.
Function ApplyMode
  ; The user's folders first: with SetShellVarContext all, $LOCALAPPDATA is ProgramData.
  SetShellVarContext current
  StrCpy $3 "$LOCALAPPDATA\Programs\LMW-Synth"
  StrCpy $4 "$LOCALAPPDATA\Programs\Common\VST3"
  ${If} $AllUsers == "1"
    SetShellVarContext all
    StrCpy $VstDir "$COMMONFILES64\VST3"
    ${If} $INSTDIR == $3
      StrCpy $INSTDIR "$PROGRAMFILES64\LMW-Synth"
    ${EndIf}
  ${Else}
    StrCpy $VstDir $4
    ${If} $INSTDIR == "$PROGRAMFILES64\LMW-Synth"
      StrCpy $INSTDIR $3
    ${EndIf}
  ${EndIf}
  ReadRegStr $1 SHCTX "${UNINSTALL_KEY}" "InstallLocation"
  ${If} $1 != ""
    StrCpy $INSTDIR $1
  ${EndIf}
  ReadRegStr $1 SHCTX "${UNINSTALL_KEY}" "DisplayVersion"
  ${If} $1 != ""
    ${VersionCompare} "$1" "${VERSION}" $2
    ${If} $2 == "1"
      MessageBox MB_YESNO|MB_ICONQUESTION "A newer ${PRODUCT} ($1) is installed. Replace it with the older version ${VERSION}?" /SD IDNO IDYES keep
      SetErrorLevel 2
      Quit
      keep:
    ${EndIf}
  ${EndIf}
FunctionEnd

Function SkipWhenElevated
  ${If} $Elevated == "1"
    Abort
  ${EndIf}
FunctionEnd

Function ModePage
  ${If} $ModeFixed == "1"
    Abort
  ${EndIf}
  !insertmacro MUI_HEADER_TEXT "Install for" "Who will use ${PRODUCT} on this computer?"
  nsDialogs::Create 1018
  Pop $0
  ${NSD_CreateRadioButton} 0 0 100% 12u "&Just me (no administrator rights needed)"
  Pop $RadioMe
  ${NSD_CreateLabel} 12u 14u 96% 34u "The app in %LOCALAPPDATA%\Programs\LMW-Synth, the VST3 plugin in %LOCALAPPDATA%\Programs\Common\VST3 (Steinberg's folder for one user). Some DAWs only find it after you add that folder to their plugin folders."
  Pop $0
  ${NSD_CreateRadioButton} 0 56u 100% 12u "&All users of this computer (asks for administrator rights)"
  Pop $RadioAll
  ${NSD_CreateLabel} 12u 70u 96% 26u "The app in Program Files\LMW-Synth, the VST3 plugin in C:\Program Files\Common Files\VST3, where every DAW looks for plugins."
  Pop $0
  ${If} $AllUsers == "1"
    ${NSD_Check} $RadioAll
  ${Else}
    ${NSD_Check} $RadioMe
  ${EndIf}
  nsDialogs::Show
FunctionEnd

Function ModePageLeave
  ${NSD_GetState} $RadioAll $0
  ${If} $0 == ${BST_CHECKED}
    StrCpy $AllUsers "1"
  ${Else}
    StrCpy $AllUsers "0"
  ${EndIf}
  Call CheckOtherMode
  ${If} ${Errors}
    Abort
  ${EndIf}
  ${If} $AllUsers == "1"
  ${AndIf} $IsAdmin != "1"
    ; Start this installer again with administrator rights; this copy ends.
    ClearErrors
    ExecShell "runas" "$EXEPATH" "/ALLUSERS /ELEVATED"
    ${If} ${Errors}
      MessageBox MB_ICONEXCLAMATION "Administrator rights were not given. Choose 'Just me', or try again." /SD IDOK
      Abort
    ${EndIf}
    Quit
  ${EndIf}
  Call ApplyMode
FunctionEnd

; Start the app without administrator rights also from an elevated installer (via Explorer).
Function RunApp
  Exec '"$WINDIR\explorer.exe" "$INSTDIR\LMW-Synth.exe"'
FunctionEnd

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Start ${PRODUCT}"
!define MUI_FINISHPAGE_RUN_FUNCTION RunApp

!define MUI_PAGE_CUSTOMFUNCTION_PRE SkipWhenElevated
!insertmacro MUI_PAGE_WELCOME
!define MUI_PAGE_CUSTOMFUNCTION_PRE SkipWhenElevated
!insertmacro MUI_PAGE_LICENSE "${STAGE}${SEP}LICENSE.txt"
Page custom ModePage ModePageLeave
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "${PRODUCT} (required)" SecMain
  SectionIn RO
  ; An earlier installation of this mode in another folder is removed first.
  ReadRegStr $1 SHCTX "${UNINSTALL_KEY}" "InstallLocation"
  ${If} $1 != ""
  ${AndIf} $1 != $INSTDIR
  ${AndIf} ${FileExists} "$1\uninstall.exe"
    !insertmacro WAIT_UNTIL_FREE "$1\LMW-Synth.exe" "${PRODUCT} is running."
    ExecWait '"$1\uninstall.exe" /S _?=$1'
    Delete "$1\uninstall.exe"
    RMDir "$1"
  ${EndIf}
  !insertmacro WAIT_UNTIL_FREE "$INSTDIR\LMW-Synth.exe" "${PRODUCT} is running."
  !insertmacro WAIT_UNTIL_FREE "$VstDir\${VST3_BINARY}" "A program (a DAW) has the ${PRODUCT} plugin loaded."

  SetOutPath "$INSTDIR"
  File "${STAGE}${SEP}LMW-Synth.exe"
  File "${STAGE}${SEP}LICENSE.txt"
  ; The documents as HTML (S9.8; a browser opens them, Windows often has nothing for .md); the .md
  ; copies of 1.2.0 and older go.
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\THIRD_PARTY.md"
  RMDir /r "$INSTDIR\docs"
  File /r "${STAGE}${SEP}docs"
  ; The plugin bundle is replaced as a whole (no files of an older version stay in it).
  RMDir /r "$VstDir\LMW-Synth.vst3"
  SetOutPath "$VstDir"
  File /r "${STAGE}${SEP}LMW-Synth.vst3"
  SetOutPath "$INSTDIR"

  ; What the uninstaller needs to know (it may run without this installer's settings).
  WriteINIStr "$INSTDIR\install.ini" "install" "allusers" "$AllUsers"
  WriteINIStr "$INSTDIR\install.ini" "install" "vst3" "$VstDir"
  WriteINIStr "$INSTDIR\install.ini" "install" "version" "${VERSION}"
  WriteUninstaller "$INSTDIR\uninstall.exe"
  CreateShortcut "$SMPROGRAMS\${PRODUCT}.lnk" "$INSTDIR\LMW-Synth.exe"
  CreateShortcut "$SMPROGRAMS\${PRODUCT} Tutorial.lnk" "$INSTDIR\docs\Tutorial.html"
  CreateShortcut "$SMPROGRAMS\${PRODUCT} Read me.lnk" "$INSTDIR\docs\README.html"
  CreateShortcut "$SMPROGRAMS\${PRODUCT} Terms.lnk" "$INSTDIR\docs\Terms.html"            ; Spec S7.10

  WriteRegStr SHCTX "${PRODUCT_KEY}" "InstallDir" "$INSTDIR"
  WriteRegStr SHCTX "${UNINSTALL_KEY}" "DisplayName" "${PRODUCT}"
  WriteRegStr SHCTX "${UNINSTALL_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr SHCTX "${UNINSTALL_KEY}" "Publisher" "LMW"
  WriteRegStr SHCTX "${UNINSTALL_KEY}" "URLInfoAbout" "https://github.com/maarten-boot/software-synth-LMW"
  WriteRegStr SHCTX "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\LMW-Synth.exe"
  WriteRegStr SHCTX "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr SHCTX "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr SHCTX "${UNINSTALL_KEY}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
  WriteRegDWORD SHCTX "${UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD SHCTX "${UNINSTALL_KEY}" "NoRepair" 1
  WriteRegDWORD SHCTX "${UNINSTALL_KEY}" "EstimatedSize" 30000
SectionEnd

Section /o "Command-line tools (lmw9p, lmw-mcp)" SecTool
  SetOutPath "$INSTDIR"
  File "${STAGE}${SEP}lmw9p.exe"
  File "${STAGE}${SEP}lmw-mcp.exe"
SectionEnd

Section /o "Desktop shortcut" SecDesktop
  CreateShortcut "$DESKTOP\${PRODUCT}.lnk" "$INSTDIR\LMW-Synth.exe"
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMain} "The standalone synthesizer, the VST3 plugin, the tutorial and read-me pages (HTML) and Start menu entries."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecTool} "lmw9p.exe (read and set the synthesizer over 9P from a terminal) and lmw-mcp.exe (lets an AI assistant such as Claude or Gemini set up sounds; see the read-me)."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "An ${PRODUCT} shortcut on the desktop."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; ---------------------------------------------------------------------------------------------
; Uninstaller.

Function un.onInit
  SetRegView 64
  ReadINIStr $AllUsers "$INSTDIR\install.ini" "install" "allusers"
  ReadINIStr $VstDir "$INSTDIR\install.ini" "install" "vst3"
  ${If} $AllUsers == "1"
    UserInfo::GetAccountType
    Pop $0
    ${If} $0 != "Admin"
      ${If} ${Silent}
        SetErrorLevel 2
        Abort
      ${EndIf}
      ; Start this uninstaller again with administrator rights; this copy ends.
      ClearErrors
      ExecShell "runas" "$EXEPATH" "_?=$INSTDIR"
      ${If} ${Errors}
        MessageBox MB_ICONEXCLAMATION "Removing ${PRODUCT} for all users needs administrator rights."
      ${EndIf}
      Quit
    ${EndIf}
    SetShellVarContext all
  ${Else}
    SetShellVarContext current
  ${EndIf}
FunctionEnd

Section "Uninstall"
  !insertmacro WAIT_UNTIL_FREE "$INSTDIR\LMW-Synth.exe" "${PRODUCT} is running."
  ${If} $VstDir != ""
    !insertmacro WAIT_UNTIL_FREE "$VstDir\${VST3_BINARY}" "A program (a DAW) has the ${PRODUCT} plugin loaded."
    RMDir /r "$VstDir\LMW-Synth.vst3"
  ${EndIf}
  ; Only what the installer put there: patches, presets (Documents) and settings stay.
  Delete "$INSTDIR\LMW-Synth.exe"
  Delete "$INSTDIR\lmw9p.exe"
  Delete "$INSTDIR\lmw-mcp.exe"
  Delete "$INSTDIR\LICENSE.txt"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\THIRD_PARTY.md"
  RMDir /r "$INSTDIR\docs"
  Delete "$INSTDIR\install.ini"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\${PRODUCT}.lnk"
  Delete "$SMPROGRAMS\${PRODUCT} Tutorial.lnk"
  Delete "$SMPROGRAMS\${PRODUCT} Read me.lnk"
  Delete "$SMPROGRAMS\${PRODUCT} Terms.lnk"
  Delete "$DESKTOP\${PRODUCT}.lnk"
  DeleteRegKey SHCTX "${UNINSTALL_KEY}"
  DeleteRegKey SHCTX "${PRODUCT_KEY}"
SectionEnd
