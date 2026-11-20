[CmdletBinding()]
param([string]$HostName,[string]$UserName)
. "$PSScriptRoot/Device-Common.ps1"
$target=Get-FrotzTarget $HostName $UserName
$out=Join-Path (Split-Path $PSScriptRoot -Parent) 'local/key-diagnostics.txt'
New-Item -ItemType Directory -Force (Split-Path $out) | Out-Null
$script=@'
id
printf 'Permissions and ownership:\n'
stat -c '%a %U %G %n' "$HOME" "$HOME/.ssh" "$HOME/.ssh/authorized_keys"
printf 'Authorized public key fingerprints:\n'
ssh-keygen -lf "$HOME/.ssh/authorized_keys"
printf 'Public-key authentication configuration:\n'
sudo -n /usr/sbin/sshd -T 2>/dev/null | grep -E '^(pubkeyauthentication|authorizedkeysfile|strictmodes|authenticationmethods) ' || true
'@
Write-Host 'Enter the Pi password locally. This collects permissions and public fingerprints only.'
$script | ssh -o StrictHostKeyChecking=yes $target "tr -d '\r' | sh" | Tee-Object -FilePath $out
if($LASTEXITCODE -ne 0){throw 'SSH diagnostic failed'}
Write-Host "Saved $out"
