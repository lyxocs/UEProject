param([string]$EngineRoot, [switch]$LowMemory, [switch]$ReuseCook)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\UnrealPath.ps1"
$taskEngine = Resolve-WarlockEngine $EngineRoot
$taskProject = (Resolve-Path -LiteralPath "$PSScriptRoot\..\WarlockArena.uproject").Path
$taskOutput = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\Saved\Builds\Windows'))
$taskCookOptions = @()
$taskCookAction = if ($ReuseCook) { '-skipcook' } else { '-cook' }
if ($LowMemory) { $taskCookOptions = @('-AdditionalCookerOptions=-noshaderworker', '-UbtArgs=-NoUBA -MaxParallelActions=2') }
& "$taskEngine\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=$taskProject" -noP4 -platform=Win64 -clientconfig=Development -build $taskCookAction -stage -pak -prereqs -nodebuginfo -archive "-archivedirectory=$taskOutput" -unattended -nocompileeditor -utf8output @taskCookOptions
if ($LASTEXITCODE -ne 0) { throw "Unreal packaging failed with code $LASTEXITCODE" }
