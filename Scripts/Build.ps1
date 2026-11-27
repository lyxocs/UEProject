param([string]$EngineRoot, [ValidateSet('Editor','Game','Server')][string]$Target = 'Editor', [switch]$LowMemory)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\UnrealPath.ps1"
$taskEngine = Resolve-WarlockEngine $EngineRoot
$taskProject = (Resolve-Path -LiteralPath "$PSScriptRoot\..\WarlockArena.uproject").Path
$taskTarget = if ($Target -eq 'Editor') { 'WarlockArenaEditor' } elseif ($Target -eq 'Server') { 'WarlockArenaServer' } else { 'WarlockArena' }
if ($Target -eq 'Server' -and (Test-Path -LiteralPath "$taskEngine\Engine\Build\InstalledBuild.txt")) { throw 'Dedicated Server requires a source engine build. See Docs/29_DedicatedServerAndLobby.md.' }
$taskBuildOptions = @()
if ($LowMemory) { $taskBuildOptions = @('-NoUBA', '-MaxParallelActions=2') }
& "$taskEngine\Engine\Build\BatchFiles\Build.bat" $taskTarget Win64 Development $taskProject -WaitMutex -NoHotReloadFromIDE @taskBuildOptions
if ($LASTEXITCODE -ne 0) { throw "Unreal build failed with code $LASTEXITCODE" }
