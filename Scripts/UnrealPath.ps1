function Resolve-WarlockEngine {
    param([string]$EngineRoot)
    if (-not $EngineRoot) { $EngineRoot = $env:UE_ENGINE_ROOT }
    $taskHint = Join-Path $PSScriptRoot '..\Saved\EngineRoot.txt'
    if (-not $EngineRoot -and (Test-Path -LiteralPath $taskHint)) { $EngineRoot = (Get-Content -LiteralPath $taskHint -Raw).Trim() }
    if (-not $EngineRoot -or -not (Test-Path -LiteralPath (Join-Path $EngineRoot 'Engine\Build\Build.version'))) {
        throw 'Specify -EngineRoot or set UE_ENGINE_ROOT to your installed Unreal Engine directory.'
    }
    return (Resolve-Path -LiteralPath $EngineRoot).Path
}
