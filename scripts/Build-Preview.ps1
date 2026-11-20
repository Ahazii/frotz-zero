[CmdletBinding()]
param([switch]$Run,[string]$StoryFolder=(Join-Path (Split-Path $PSScriptRoot -Parent) 'stories'),[string]$Distribution='Ubuntu')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$linuxRoot=(& wsl -d $Distribution -- wslpath -a "$root").Trim()
if($LASTEXITCODE -ne 0){throw 'WSL is unavailable. See docs/WINDOWS.md.'}
& wsl -d $Distribution -- cmake -S $linuxRoot -B "$linuxRoot/build/desktop" -G Ninja '-DCMAKE_BUILD_TYPE=RelWithDebInfo'
if($LASTEXITCODE -ne 0){throw 'Configure failed'}
& wsl -d $Distribution -- cmake --build "$linuxRoot/build/desktop" -j 4
if($LASTEXITCODE -ne 0){throw 'Build failed'}
if($Run){$linuxStories=(& wsl -d $Distribution -- wslpath -a "$StoryFolder").Trim(); & wsl -d $Distribution -- env SDL_VIDEODRIVER=x11 "$linuxRoot/build/desktop/frotz-zero" --stories $linuxStories --scale 3}
