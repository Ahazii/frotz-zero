[CmdletBinding()]
param([string]$Distribution='Ubuntu')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$linuxRoot=(& wsl -d $Distribution -- wslpath -a "$root").Trim()
if($LASTEXITCODE -ne 0){throw 'WSL is unavailable. See docs/WINDOWS.md.'}
& wsl -d $Distribution -- python3 "$linuxRoot/scripts/build_device.py"
if($LASTEXITCODE -ne 0){throw 'Device build failed'}
