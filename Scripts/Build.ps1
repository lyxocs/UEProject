param([string]$EngineRoot, [ValidateSet('Editor','Game')][string]$Target = 'Editor', [switch]$LowMemory)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\UnrealPath.ps1"
$taskEngine = Resolve-WarlockEngine $EngineRoot
$taskProject = (Resolve-Path -LiteralPath "$PSScriptRoot\..\WarlockArena.uproject").Path
$taskTarget = if ($Target -eq 'Editor') { 'WarlockArenaEditor' } else { 'WarlockArena' }
$taskBuildOptions = @()
if ($LowMemory) { $taskBuildOptions = @('-NoUBA', '-MaxParallelActions=2') }
& "$taskEngine\Engine\Build\BatchFiles\Build.bat" $taskTarget Win64 Development $taskProject -WaitMutex -NoHotReloadFromIDE @taskBuildOptions
if ($LASTEXITCODE -ne 0) { throw "Unreal build failed with code $LASTEXITCODE" }
