[CmdletBinding()]
param([string]$HostName,[string]$UserName)
. "$PSScriptRoot/Device-Common.ps1"
$target=Get-FrotzTarget $HostName $UserName
$out=Join-Path (Split-Path $PSScriptRoot -Parent) 'local/device-diagnostics.txt'
New-Item -ItemType Directory -Force (Split-Path $out) | Out-Null
$command='date -u; uname -m; id; df -h "$HOME"; dpkg-query -W frotz-zero libsdl2-2.0-0 libsdl2-mixer-2.0-0 libsdl2-ttf-2.0-0; ldd /usr/bin/frotz-zero; systemctl --user is-active APPLaunch.service; cat /proc/asound/cards; ls -l /dev/fb0 /dev/input/event*; journalctl --user -u APPLaunch.service --no-pager -n 40'
& ssh -o StrictHostKeyChecking=yes $target $command 2>&1 | Set-Content -LiteralPath $out -Encoding utf8
Write-Host "Bounded diagnostics: $out (review before sharing; story transcripts are not collected)."
