[CmdletBinding()]
param([string]$HostName,[string]$UserName)
. "$PSScriptRoot/Device-Common.ps1"
$target=Get-FrotzTarget $HostName $UserName
# The trap restores the user's normal launcher, including after Ctrl+C.
$command='set -eu; active=0; systemctl --user is-active --quiet APPLaunch.service && active=1; restore(){ if [ "$active" = 1 ]; then systemctl --user start APPLaunch.service; fi; }; trap restore EXIT; trap "exit 130" INT TERM; if [ "$active" = 1 ]; then systemctl --user stop APPLaunch.service; fi; /usr/bin/frotz-zero --device'
& ssh -t -o StrictHostKeyChecking=yes $target $command
Assert-FrotzSuccess 'Device app session'
