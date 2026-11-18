# SPDX-License-Identifier: AGPL-3.0-or-later
# T-S9.5-installer: silent install / update / refusals / uninstall of the Windows installer, for
# "just me" and "all users" (Spec S9.5). Needs administrator rights for the all-users part (CI runner).
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
    Check (-not (Test-Path $reg)) "$mode Apps & features entry removed"
}

if ($failures -eq 0) { Write-Output "test-installer: all passed" } else { Write-Output "test-installer: $failures failed" }
exit $failures
