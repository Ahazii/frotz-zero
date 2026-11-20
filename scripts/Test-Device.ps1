[CmdletBinding()]
param([string]$HostName,[string]$UserName,[switch]$UseBuild,[switch]$Quiet)
. "$PSScriptRoot/Device-Common.ps1"
$target=Get-FrotzTarget $HostName $UserName
$root=Split-Path $PSScriptRoot -Parent
& ssh -o BatchMode=yes -o StrictHostKeyChecking=yes $target 'mkdir -p "$HOME/.cache/frotz-zero-tests/fixtures"'
Assert-FrotzSuccess 'Test staging'
& scp -o BatchMode=yes -o StrictHostKeyChecking=yes "$root/build/fixtures/lantern.z4" "$root/build/fixtures/lantern.z5" "$root/build/fixtures/lantern.z7" "$root/build/fixtures/lantern.z8" "$root/build/fixtures/graphics.z6" "$root/build/fixtures/graphics.blb" "${target}:.cache/frotz-zero-tests/fixtures/"
Assert-FrotzSuccess 'Public fixture copy'
& scp -o BatchMode=yes -o StrictHostKeyChecking=yes "$root/build/fixtures/intro.z4" "$root/build/fixtures/framed.z6" "${target}:.cache/frotz-zero-tests/fixtures/"
Assert-FrotzSuccess 'Intro and framed-text fixture copy'
& scp -o BatchMode=yes -o StrictHostKeyChecking=yes "$root/build/fixtures/automatic.z6" "$root/build/fixtures/automatic.blb" "${target}:.cache/frotz-zero-tests/fixtures/"
Assert-FrotzSuccess 'Automatic artwork and text fixture copy'
& scp -o BatchMode=yes -o StrictHostKeyChecking=yes "$root/build/fixtures/lantern.z3" "$root/build/fixtures/text-modes.z5" "$root/build/fixtures/pagination.z3" "${target}:.cache/frotz-zero-tests/fixtures/"
Assert-FrotzSuccess 'Universal reading fixture copy'
& scp -o BatchMode=yes -o StrictHostKeyChecking=yes "$PSScriptRoot/device-smoke.sh" "${target}:.cache/frotz-zero-tests/device-smoke.sh"
Assert-FrotzSuccess 'Test script copy'
$command='sh "$HOME/.cache/frotz-zero-tests/device-smoke.sh"'
if($UseBuild){
    & scp -o BatchMode=yes -o StrictHostKeyChecking=yes "$root/build/arm64/frotz-zero" "${target}:.cache/frotz-zero-tests/frotz-zero"
    Assert-FrotzSuccess 'Normal-user test binary copy'
    $command='chmod 755 "$HOME/.cache/frotz-zero-tests/frotz-zero"; FROTZ_ZERO_TEST_BINARY="$HOME/.cache/frotz-zero-tests/frotz-zero" sh "$HOME/.cache/frotz-zero-tests/device-smoke.sh"'
}
if($Quiet){$command='export FROTZ_ZERO_TEST_QUIET=1; '+$command}
& ssh -o BatchMode=yes -o StrictHostKeyChecking=yes $target $command
Assert-FrotzSuccess 'Device smoke tests'
$remote=(& ssh -o BatchMode=yes -o StrictHostKeyChecking=yes $target 'cat "$HOME/.cache/frotz-zero-tests/latest"').Trim()
if($remote -notmatch '^/home/[a-zA-Z0-9_.-]+/\.cache/frotz-zero-tests/run-[0-9-]+$'){throw 'Unexpected evidence path'}
$out=Join-Path $root 'local/device-tests';New-Item -ItemType Directory -Force $out | Out-Null
& scp -r -o BatchMode=yes -o StrictHostKeyChecking=yes "${target}:$remote" $out
Assert-FrotzSuccess 'Evidence copy'
& ssh -o BatchMode=yes -o StrictHostKeyChecking=yes $target 'systemctl --user is-active APPLaunch.service'
Write-Host "Device evidence saved under $out. Physical observation remains a separate check."
