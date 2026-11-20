$ErrorActionPreference='Stop'
function Get-FrotzTarget([string]$HostName,[string]$UserName){
    $path=Join-Path (Split-Path $PSScriptRoot -Parent) 'deployment.local.json'
    if(Test-Path -LiteralPath $path){$config=Get-Content -LiteralPath $path -Raw | ConvertFrom-Json;if(!$HostName){$HostName=$config.host};if(!$UserName){$UserName=$config.user}}
    if(!$HostName){$HostName='frotz-zero'}
    if($HostName -notmatch '^[a-zA-Z0-9][a-zA-Z0-9_.:-]*$' -or ($UserName -and $UserName -notmatch '^[a-zA-Z0-9_][a-zA-Z0-9_.-]*$')){throw 'Invalid SSH host or user'}
    if($UserName){return "$UserName@$HostName"};return $HostName
}
function Assert-FrotzSuccess([string]$Action){if($LASTEXITCODE -ne 0){throw "$Action failed (exit $LASTEXITCODE)"}}
