param([string]$EngineRoot, [ValidateSet('Editor','Solo','TwoPads','Mixed','KeyboardMouse','Host','Join')][string]$Mode = 'Solo', [string]$Server = '127.0.0.1', [switch]$PrintCommand, [switch]$Packaged, [int]$Width=0, [int]$Height=0, [switch]$Windowed)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\UnrealPath.ps1"
$taskEngine=$null
if (-not $Packaged -or $Mode -eq 'Editor') { $taskEngine = Resolve-WarlockEngine $EngineRoot }
$taskProject = (Resolve-Path -LiteralPath "$PSScriptRoot\..\WarlockArena.uproject").Path
$taskMap = '/Game/WarlockArena/Maps/Arena'
$taskProjectArg = '"' + $taskProject + '"'
switch ($Mode) {
    'Editor' { $taskArgs = @($taskProjectArg, $taskMap, '-ModelContextProtocolStartServer', '-ModelContextProtocolPort=8100') }
    'TwoPads' { $taskArgs = @($taskProjectArg, ($taskMap+'?LocalPlayers=2?Input=TwoPads'), '-game') }
    'Mixed' { $taskArgs = @($taskProjectArg, ($taskMap+'?LocalPlayers=2?Input=Mixed'), '-game') }
    'KeyboardMouse' { $taskArgs = @($taskProjectArg, ($taskMap+'?LocalPlayers=2?Input=KeyboardMouse'), '-game') }
    'Host' { $taskArgs = @($taskProjectArg, ($taskMap+'?listen'), '-game') }
    'Join' { if ($Server -notmatch '^[A-Za-z0-9.\-:]+$') { throw 'Invalid server address' }; $taskArgs = @($taskProjectArg, $Server, '-game') }
    default { $taskArgs = @($taskProjectArg, $taskMap, '-game') }
}
if($Width -or $Height){if($Width -lt 640 -or $Height -lt 360){throw 'Specify both Width >= 640 and Height >= 360'};$taskArgs+=@("-ResX=$Width","-ResY=$Height")}
if($Windowed){$taskArgs+='-windowed'}
$taskExecutable = "$taskEngine\Engine\Binaries\Win64\UnrealEditor.exe"
if ($Packaged -and $Mode -ne 'Editor') {
    $taskExecutable = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\Saved\Builds\Windows\WarlockArena\Binaries\Win64\WarlockArena.exe'))
    if (-not (Test-Path -LiteralPath $taskExecutable)) { throw 'Packaged build not found. Run Scripts/Package.ps1 first.' }
    $taskArgs = $taskArgs[1..($taskArgs.Count-1)]
}
if ($PrintCommand) { [pscustomobject]@{Executable=$taskExecutable; Arguments=$taskArgs}; return }
# Interactive editor/game explicitly requested by this script's caller.
Start-Process -FilePath $taskExecutable -ArgumentList $taskArgs
