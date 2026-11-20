[CmdletBinding()]
param([string]$HostName,[string]$UserName,[string]$PublicKeyPath=(Join-Path $env:USERPROFILE '.ssh\id_ed25519.pub'))
. "$PSScriptRoot/Device-Common.ps1"
$target=Get-FrotzTarget $HostName $UserName
$line=(Get-Content -LiteralPath $PublicKeyPath -Raw).Trim()
if($line -notmatch '^(ssh-ed25519|ssh-rsa|ecdsa-sha2-nistp256) [A-Za-z0-9+/=]+(?: .*)?$'){throw 'Not a recognized OpenSSH public key'}
# Only public key material is transmitted. Single quotes are escaped for POSIX sh.
$quoted="'"+$line.Replace("'", "'\''")+"'"
$script=@'
set -eu
umask 077
mkdir -p "$HOME/.ssh"
touch "$HOME/.ssh/authorized_keys"
key=PUBLIC_KEY
if ! grep -qxF "$key" "$HOME/.ssh/authorized_keys"; then
  printf '%s\n' "$key" >> "$HOME/.ssh/authorized_keys"
fi
chmod 700 "$HOME/.ssh"
chmod 600 "$HOME/.ssh/authorized_keys"
printf 'Existing public key authorized for this account.\n'
'@
$script=$script.Replace('PUBLIC_KEY',$quoted)
Write-Host 'Enter the Zero password locally. Your private key stays on Windows.'
$script | ssh -o StrictHostKeyChecking=yes $target "tr -d '\r' | sh"
if($LASTEXITCODE -ne 0){throw 'Key authorization failed'}
ssh -o BatchMode=yes -o StrictHostKeyChecking=yes $target 'printf key-authentication-ok'
if($LASTEXITCODE -ne 0){Write-Warning 'The key was installed but may need unlocking locally with ssh-add.'}
