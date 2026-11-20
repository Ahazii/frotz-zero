[CmdletBinding()]
param([string]$StoryFolder=(Join-Path (Split-Path $PSScriptRoot -Parent) 'stories'),[string]$HostName,[string]$UserName)
. "$PSScriptRoot/Device-Common.ps1"
$target=Get-FrotzTarget $HostName $UserName
$StoryFolder=(Resolve-Path -LiteralPath $StoryFolder).Path
if(!(Test-Path -LiteralPath $StoryFolder -PathType Container)){throw 'StoryFolder must be a directory'}
& ssh -o StrictHostKeyChecking=yes $target 'mkdir -p "$HOME/Games/FrotzZero/stories"'
Assert-FrotzSuccess 'Story folder creation'
& scp -r -o StrictHostKeyChecking=yes "$StoryFolder/." "${target}:Games/FrotzZero/stories/"
Assert-FrotzSuccess 'Story copy'
Write-Host 'Copied your local folder. Choose Refresh library on the Zero.'
