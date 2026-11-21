# SPDX-License-Identifier: AGPL-3.0-or-later
# T-S9.5-installer: silent install / update / refusals / uninstall of the Windows installer, for
# "just me" and "all users" (Spec S9.5). Needs administrator rights for the all-users part (CI runner).
# T-S9.5-pages (after the 1.2.0 bug: Next on "Install for" did nothing): the installer's pages clicked
# through with window messages — Welcome, License, Install for (Just me), Components, Folder, Install,
# Finish — then the result is checked and removed. Needs a desktop session (the CI runner has one).
# Usage: pwsh scripts/test-installer.ps1 <path to LMW-Synth-<version>-setup.exe>
param([Parameter(Mandatory = $true)][string]$Setup)
$ErrorActionPreference = 'Stop'
$failures = 0
function Check([bool]$ok, [string]$what) {
    if ($ok) { Write-Output "ok   $what" } else { Write-Output "FAIL $what"; $script:failures++ }
}
function Run([string]$exe, [string[]]$arguments) {
    $p = Start-Process -FilePath $exe -ArgumentList $arguments -Wait -PassThru
    return $p.ExitCode
}
$Setup = (Resolve-Path $Setup).Path
$version = [regex]::Match((Split-Path $Setup -Leaf), 'LMW-Synth-(.+)-setup\.exe').Groups[1].Value
Check ($version -ne '') "version from the file name: $version"
$key = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\LMW-Synth'
$vstBinary = 'LMW-Synth.vst3\Contents\x86_64-win\LMW-Synth.vst3'

function Uninstall([string]$dir) {
    # _?= runs the uninstaller in place, so Start-Process can wait for it.
    $code = Run "$dir\uninstall.exe" @('/S', "_?=$dir")
    Remove-Item -Force -ErrorAction SilentlyContinue "$dir\uninstall.exe"
    Remove-Item -Force -ErrorAction SilentlyContinue $dir
    return $code
}

foreach ($mode in @('CURRENTUSER', 'ALLUSERS')) {
    if ($mode -eq 'CURRENTUSER') {
        $app = "$env:LOCALAPPDATA\Programs\LMW-Synth"; $vst = "$env:LOCALAPPDATA\Programs\Common\VST3"
        $reg = "HKCU:\$key"; $menu = "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\LMW-Synth.lnk"
        $other = 'ALLUSERS'
    } else {
        $app = "$env:ProgramFiles\LMW-Synth"; $vst = "$env:CommonProgramFiles\VST3"
        $reg = "HKLM:\$key"; $menu = "$env:ProgramData\Microsoft\Windows\Start Menu\Programs\LMW-Synth.lnk"
        $other = 'CURRENTUSER'
    }
    Write-Output "--- $mode"
    Check ((Run $Setup @('/S', "/$mode")) -eq 0) "$mode install exit code 0"
    Check (Test-Path "$app\LMW-Synth.exe") "$mode app in $app"
    Check (Test-Path "$app\uninstall.exe") "$mode uninstaller"
    Check (-not (Test-Path "$app\lmw9p.exe")) "$mode lmw9p not installed by default"
    Check (Test-Path "$vst\$vstBinary") "$mode VST3 plugin in $vst"
    Check (Test-Path $menu) "$mode Start menu entry"
    $menuDir = Split-Path $menu
    Check ((Test-Path "$menuDir\LMW-Synth Tutorial.lnk") -and (Test-Path "$menuDir\LMW-Synth Read me.lnk")) "$mode Start menu Tutorial and Read me"
    Check ((Test-Path "$app\docs\Tutorial.html") -and (Test-Path "$app\docs\README.html") -and (Test-Path "$app\docs\THIRD_PARTY.html") -and (Test-Path "$app\docs\images\synth.png")) "$mode documents as HTML (S9.8)"
    Check (-not (Test-Path "$app\README.md")) "$mode no .md files"
    Check ((Get-ItemProperty $reg).DisplayVersion -eq $version) "$mode Apps & features entry, version $version"

    # Update: the same installer again over it.
    Set-Content -Path "$vst\LMW-Synth.vst3\stale.txt" -Value 'from an older version'
    Check ((Run $Setup @('/S', "/$mode")) -eq 0) "$mode reinstall exit code 0"
    Check (-not (Test-Path "$vst\LMW-Synth.vst3\stale.txt")) "$mode plugin bundle replaced as a whole"

    # Refused: the other mode while this one is installed; an older installer over a newer version.
    Check ((Run $Setup @('/S', "/$other")) -eq 2) "$mode installed: $other install refused (exit code 2)"
    Set-ItemProperty $reg -Name DisplayVersion -Value '99.0.0'
    Check ((Run $Setup @('/S', "/$mode")) -eq 2) "$mode newer version installed: silent downgrade refused (exit code 2)"
    Set-ItemProperty $reg -Name DisplayVersion -Value $version

    # Uninstall: everything the installer put there is gone; the user's documents stay.
    Check ((Uninstall $app) -eq 0) "$mode uninstall exit code 0"
    Check (-not (Test-Path $app)) "$mode app folder removed"
    Check (-not (Test-Path "$vst\LMW-Synth.vst3")) "$mode plugin removed"
    Check (-not (Test-Path $menu)) "$mode Start menu entry removed"
    Check (-not (Test-Path "$menuDir\LMW-Synth Tutorial.lnk")) "$mode Start menu Tutorial removed"
    Check (-not (Test-Path $reg)) "$mode Apps & features entry removed"
}

# --- T-S9.5-pages: the pages clicked as a user does (nothing installed now: the 1.2.0 bug case).
# Win32 messages, not UI Automation (on the CI desktop UI Automation saw the window but none of its
# buttons): the wizard's buttons are dialog items 1 (Next / I Agree / Install / Finish), 2 (Cancel),
# 3 (Back); MUI2 shows the page title in item 1037 (the header; Welcome and Finish have none).
Write-Output "--- pages (clicked)"
Add-Type -TypeDefinition @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class Wiz {
    [DllImport("user32.dll")] static extern IntPtr GetDlgItem(IntPtr dlg, int id);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowText(IntPtr w, StringBuilder s, int n);
    [DllImport("user32.dll")] static extern bool IsWindowEnabled(IntPtr w);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr w);
    [DllImport("user32.dll")] static extern bool PostMessage(IntPtr w, uint msg, IntPtr wp, IntPtr lp);
    public static string Text(IntPtr dlg, int id) {
        IntPtr c = GetDlgItem(dlg, id);
        if (c == IntPtr.Zero) return "";
        var s = new StringBuilder(512); GetWindowText(c, s, 512); return s.ToString();
    }
    public static bool Ready(IntPtr dlg, int id) {
        IntPtr c = GetDlgItem(dlg, id);
        return c != IntPtr.Zero && IsWindowEnabled(c) && IsWindowVisible(c);
    }
    // WM_COMMAND with BN_CLICKED from the button, as a mouse click sends it.
    public static bool Click(IntPtr dlg, int id) {
        IntPtr c = GetDlgItem(dlg, id);
        return c != IntPtr.Zero && PostMessage(dlg, 0x0111, (IntPtr)id, c);
    }
}
"@
function Wait-For([scriptblock]$probe, [int]$seconds = 20) {
    $until = (Get-Date).AddSeconds($seconds)
    while ((Get-Date) -lt $until) { $r = & $probe; if ($r) { return $r }; Start-Sleep -Milliseconds 300 }
    return $null
}
$p = Start-Process -FilePath $Setup -PassThru
$h = Wait-For { $p.Refresh(); if ($p.MainWindowHandle -ne [IntPtr]::Zero) { $p.MainWindowHandle } }
Check ($null -ne $h) "pages: the installer window opens"
function Page([string]$title, [string]$button, [string]$what, [int]$seconds = 20) {
    # Wait until the header shows $title (any title when empty) and button 1 reads $button, then click it.
    $ok = Wait-For { ($title -eq '' -or [Wiz]::Text($h, 1037) -like "$title*") -and
                     [Wiz]::Text($h, 1).Replace('&', '') -like "$button*" -and [Wiz]::Ready($h, 1) } $seconds
    if (-not $ok) { Write-Output ("     now: header '{0}', button '{1}'" -f [Wiz]::Text($h, 1037), [Wiz]::Text($h, 1)) }
    Check ([bool]$ok) "pages: $what"
    if ($ok) { [void][Wiz]::Click($h, 1); Start-Sleep -Milliseconds 500 }
    return [bool]$ok
}
if ($h) {
    $null = (Page '' 'Next' 'Welcome, Next') -and
            (Page 'License Agreement' 'I Agree' 'License, I Agree') -and
            (Page 'Install for' 'Next' 'Install for (Just me), Next') -and
            (Page 'Choose Components' 'Next' 'Next leads on to Choose Components (1.2.0: stuck), Next') -and
            (Page 'Choose Install Location' 'Install' 'Install Location, Install') -and
            (Page '' 'Finish' 'Finish page, Finish' 120)
    Check ($p.WaitForExit(20000)) "pages: the installer ends"
}
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
Start-Sleep -Seconds 2
Get-Process -Name 'LMW-Synth' -ErrorAction SilentlyContinue | Stop-Process -Force   # started by the Finish page
$app = "$env:LOCALAPPDATA\Programs\LMW-Synth"
Check (Test-Path "$app\LMW-Synth.exe") "pages: installed just for me in $app"
Check ((Get-ItemProperty "HKCU:\$key" -ErrorAction SilentlyContinue).DisplayVersion -eq $version) "pages: Apps & features entry"
if (Test-Path "$app\uninstall.exe") { Check ((Uninstall $app) -eq 0) "pages: uninstall exit code 0" }

if ($failures -eq 0) { Write-Output "test-installer: all passed" } else { Write-Output "test-installer: $failures failed" }
exit $failures
