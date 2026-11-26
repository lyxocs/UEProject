param([string]$EngineRoot,[ValidateRange(1,100000)][int]$Episodes=1000,[int]$Seed=7919,[ValidateRange(1,5)][int]$Tier=5,[int]$Seconds=240,[string]$Output='Saved/Training/run',[string]$Policy,[switch]$Evaluate,[switch]$Baseline,[switch]$Untrained,[switch]$Packaged)
$ErrorActionPreference='Stop'
. "$PSScriptRoot\UnrealPath.ps1"
$taskProject=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\WarlockArena.uproject'))
$taskRoot=Split-Path $taskProject
$taskExecutable=if($Packaged){Join-Path $taskRoot 'Saved/Builds/Windows/WarlockArena/Binaries/Win64/WarlockArena.exe'}else{Join-Path (Resolve-WarlockEngine $EngineRoot) 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'}
if(-not(Test-Path -LiteralPath $taskExecutable)){throw 'Training executable missing; build/package the current source first'}
$taskOutput=if([IO.Path]::IsPathRooted($Output)){[IO.Path]::GetFullPath($Output)}else{[IO.Path]::GetFullPath((Join-Path $taskRoot $Output))}
if(-not $taskOutput.StartsWith(($taskRoot+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase)){throw 'Training output must stay inside this project'}
New-Item -ItemType Directory -Force -Path $taskOutput | Out-Null
$taskArgs=@(('"'+$taskProject+'"'),'/Game/WarlockArena/Maps/Arena','-game','-nullrhi','-unattended','-NoSound','-NoSplash','-WarlockTrain',"-TrainEpisodes=$Episodes","-TrainSeed=$Seed","-TrainTier=$Tier","-TrainSeconds=$Seconds",('-TrainOutput="'+$taskOutput+'"'),('-abslog="'+(Join-Path $taskOutput 'engine.log')+'"'),'-ExecCmds="t.MaxFPS 0"')
if($Evaluate){$taskArgs+='-TrainEvaluate'}
if($Baseline){$taskArgs+='-TrainBaseline'}
if($Untrained){$taskArgs+='-TrainUntrained'}
if($Policy){$taskPolicy=(Resolve-Path -LiteralPath $Policy).Path;$taskArgs+=('-TrainPolicy="'+$taskPolicy+'"')}
if($Packaged){$taskArgs=$taskArgs[1..($taskArgs.Count-1)]}
$taskWatch=[Diagnostics.Stopwatch]::StartNew()
$taskProcess=Start-Process -FilePath $taskExecutable -ArgumentList $taskArgs -WorkingDirectory (Split-Path $taskExecutable) -WindowStyle Hidden -PassThru
$taskLast=-1
while(-not $taskProcess.WaitForExit(1000)){
    $taskLog=Join-Path $taskOutput 'engine.log'
    if(Test-Path -LiteralPath $taskLog){$taskLines=Select-String -LiteralPath $taskLog -Pattern 'WARLOCK_RL episodes=';if($taskLines.Count -ne $taskLast -and $taskLines.Count -gt 0){$taskLast=$taskLines.Count;Write-Output $taskLines[-1].Line}}
}
if($taskProcess.ExitCode -ne 0){throw "Training failed ($($taskProcess.ExitCode)); inspect $taskOutput/engine.log"}
$taskReport=Join-Path $taskOutput 'report.json'
if(-not(Test-Path -LiteralPath $taskReport)){throw 'Training did not produce a completion report'}
$taskWatch.Stop()
@{Packaged=[bool]$Packaged;ProcessWallSeconds=$taskWatch.Elapsed.TotalSeconds} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $taskOutput 'process-timing.json') -Encoding utf8
Get-Content -LiteralPath $taskReport
