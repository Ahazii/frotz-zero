[CmdletBinding()]
param([string]$HostName,[string]$UserName)
. "$PSScriptRoot/Device-Common.ps1"
$target=Get-FrotzTarget $HostName $UserName
$root=Split-Path $PSScriptRoot -Parent
& ssh -o BatchMode=yes -o StrictHostKeyChecking=yes $target 'mkdir -p "$HOME/Games/FrotzZero/stories/FrotzZero-Demos"'
Assert-FrotzSuccess 'Speaker test staging'
& scp -o BatchMode=yes -o StrictHostKeyChecking=yes "$root/build/fixtures/audio.blb" "${target}:Games/FrotzZero/stories/FrotzZero-Demos/000-speaker-check.blb"
Assert-FrotzSuccess 'Original sound fixture copy'
Write-Host 'Prepared the original CC0 fixture; the launcher and display are unchanged.'
Write-Host 'Wake with Tab then Enter if needed. Open Frotz Zero normally, Refresh, Load Story, Speaker Check (000-speaker-check.blb), then New Game.'
Write-Host 'Press P for three tones. Ctrl+Esc opens Mute; return to the story and press P to repeat. Q returns to the library.'
Write-Host 'Restore your preferred Mute/Volume settings afterward; they persist.'
