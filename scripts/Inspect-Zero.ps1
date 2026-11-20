[CmdletBinding()]
param([string]$HostName,[string]$UserName)
. "$PSScriptRoot/Device-Common.ps1"
$target=Get-FrotzTarget $HostName $UserName
$root = Split-Path $PSScriptRoot -Parent
$out = Join-Path $root 'local'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$probe = @'
set -u
echo '=== System ==='
uname -a
cat /etc/os-release
dpkg --print-architecture
getconf GNU_LIBC_VERSION
id
echo '=== Firmware ==='
cat /etc/rpi-issue 2>/dev/null || true
echo '=== Devices ==='
ls -l /dev/fb* /dev/dri/card* /dev/input/event* /dev/snd/* 2>/dev/null || true
cat /proc/bus/input/devices
echo '=== Framebuffer ==='
cat /sys/class/graphics/fb*/virtual_size /sys/class/graphics/fb*/bits_per_pixel 2>/dev/null || true
echo '=== Libraries ==='
dpkg-query -W 'libsdl2*' 'libfreetype*' 'libpng*' 'libjpeg*' 'libxkbcommon*' 'libasound*' 'libc6' 'libstdc++6' 'applaunch*' 2>/dev/null || true
echo '=== Audio ==='
cat /proc/asound/cards 2>/dev/null || true
aplay -l 2>/dev/null || true
echo '=== Launcher ==='
systemctl --user status APPLaunch.service --no-pager -n 8 2>/dev/null || true
systemctl status APPLaunch.service --no-pager -n 8 2>/dev/null || true
find /usr/share/APPLaunch/applications -maxdepth 1 -name '*.desktop' 2>/dev/null | head -8
echo '=== Fonts ==='
fc-match 'DejaVu Sans'
fc-match 'JetBrains Mono'
'@
Write-Host 'Enter your SSH password locally if asked. It will not be saved.'
$probe | ssh -o StrictHostKeyChecking=yes -o ConnectTimeout=10 $target "tr -d '\r' | sh" | Tee-Object -FilePath (Join-Path $out 'device-inspection.txt')
if ($LASTEXITCODE -ne 0) { throw "SSH inspection failed ($LASTEXITCODE)" }
Write-Host "Inspection saved to $out\device-inspection.txt"
