[CmdletBinding()]
param([string]$Package,[string]$HostName,[string]$UserName,[switch]$NonInteractive)
. "$PSScriptRoot/Device-Common.ps1"
$target=Get-FrotzTarget $HostName $UserName
if(!$Package){
    $root=Split-Path $PSScriptRoot -Parent
    $release=Get-Content -LiteralPath (Join-Path $root 'app-builder.json') -Raw | ConvertFrom-Json
    $Package=Join-Path $root "dist/frotz-zero_$($release.version)_arm64.deb"
}
$Package=(Resolve-Path -LiteralPath $Package).Path
$name=Split-Path $Package -Leaf
if($name -notmatch '^frotz-zero_[0-9][a-zA-Z0-9.+~_-]*_arm64\.deb$'){throw 'Expected a frotz-zero ARM64 Debian package'}
$sshArgs=@('-o','StrictHostKeyChecking=yes');if($NonInteractive){$sshArgs+=@('-o','BatchMode=yes')}
& ssh @sshArgs $target 'test "$(dpkg --print-architecture)" = arm64 && mkdir -p "$HOME/.cache/frotz-zero-deploy"'
Assert-FrotzSuccess 'Architecture check'
& scp @sshArgs $Package "${target}:.cache/frotz-zero-deploy/$name"
Assert-FrotzSuccess 'Package copy'
$hash=(Get-FileHash -LiteralPath $Package -Algorithm SHA256).Hash.ToLowerInvariant()
$command="cd `"`$HOME/.cache/frotz-zero-deploy`" && printf '%s  %s\n' '$hash' '$name' | sha256sum -c - && sudo "
if($NonInteractive){$command+='-n '}
$command+="apt-get install --reinstall -y './$name'"
if($NonInteractive){& ssh @sshArgs $target $command}else{& ssh -t @sshArgs $target $command}
Assert-FrotzSuccess 'Installation'
& ssh @sshArgs $target 'frotz-zero --version; ldd /usr/bin/frotz-zero; test -f /usr/share/APPLaunch/applications/frotz-zero.desktop'
Assert-FrotzSuccess 'Installed app verification'
Write-Host 'Installed. Open Frotz Zero from the launcher; no root application service was added.'
