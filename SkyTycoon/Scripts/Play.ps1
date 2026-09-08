param(
    [string]$EngineRoot = 'D:\Epic Games\UE_5.7'
)
$ErrorActionPreference = 'Stop'
$taskProject = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../SkyTycoon.uproject'))
$taskEditor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe'
if (-not (Test-Path -LiteralPath $taskEditor)) { throw "UE editor not found: $taskEditor" }
if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot '../Binaries/Win64/UnrealEditor-SkyTycoon.dll'))) {
    throw 'Build SkyTycoonEditor first; see README.md.'
}
& $taskEditor $taskProject '/Engine/Maps/Entry?game=/Script/SkyTycoon.TycoonGameMode' -game -windowed -ResX=1000 -ResY=760 -NoSplash
