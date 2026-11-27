param([ValidateSet('Baseline','Low','Fast','Balanced','High','Epic')][string]$Preset='Balanced',[switch]$Solo,[ValidateRange(2,8)][int]$Participants=4)
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskArchive=Join-Path $taskRoot 'Saved\Builds\Windows'
$taskExe=Join-Path $taskArchive 'WarlockArena\Binaries\Win64\WarlockArena.exe'
$taskSettings=Join-Path $taskArchive 'WarlockArena\Saved\Config\Windows\GameUserSettings.ini'
$taskBackup=if(Test-Path -LiteralPath $taskSettings){[IO.File]::ReadAllBytes($taskSettings)}else{$null}
$taskName='client-'+$Preset.ToLower()+$(if($Solo){'-solo'}else{'-split'})
if($Participants -ne 4){$taskName+='-'+$Participants+'p'}
$taskOutput=Join-Path $taskRoot ('Saved\Validation\'+$taskName+'.png')
$taskLog=Join-Path $taskRoot ('Saved\Validation\'+$taskName+'.log')
$taskMap='/Game/WarlockArena/Maps/Arena'+$(if($Solo){''}else{'?LocalPlayers=2?Input=Mixed'})
$taskCommands=@('DisableAllScreenMessages')
$taskCommands+=@('TogglePause',('SetParticipantCount '+$Participants),'TogglePause')
if($Preset -eq 'Baseline'){$taskCommands+=@('Scalability 2','r.AntiAliasingMethod 2','r.ScreenPercentage 100')}else{$taskCommands+='SetGraphicsPreset '+(@('Low','Fast','Balanced','High','Epic').IndexOf($Preset))}
$taskCommands+=@('r.VSync 0','t.MaxFPS 0','BeginTestRound',('CaptureScene '+$taskOutput.Replace('\','/')+' 12'))
$taskArgs=@($taskMap,'-NoSplash','-NoSound','-RenderOffscreen','-unattended','-windowed','-ResX=1600','-ResY=900','-csvCaptureFrames=1200','-csvGpuStats',('-abslog='+$taskLog),('-ExecCmds="'+($taskCommands -join ',')+'"'))
$taskStart=Get-Date
$taskProcess=Start-Process -FilePath $taskExe -WorkingDirectory $taskArchive -ArgumentList $taskArgs -WindowStyle Hidden -PassThru
try{
    do{Start-Sleep -Milliseconds 500;if($taskProcess.HasExited){throw "Client exited: $taskLog"}
        $taskDone=(Test-Path -LiteralPath $taskLog) -and (Select-String -LiteralPath $taskLog -Pattern 'Capture Ended. Writing CSV' -Quiet)
    }while((-not $taskDone -or ((Get-Date)-$taskStart).TotalSeconds -lt 20) -and ((Get-Date)-$taskStart).TotalSeconds -lt 58)
    if(-not $taskDone){throw "No completed profile: $taskLog"}
    if(-not(Test-Path -LiteralPath $taskOutput) -or (Get-Item -LiteralPath $taskOutput).LastWriteTime -lt $taskStart){throw 'No fresh screenshot'}
    [pscustomobject]@{Preset=$Preset;Log=$taskLog;Screenshot=$taskOutput}
}finally{
    if(-not $taskProcess.HasExited -and $taskProcess.Path -eq $taskExe){Stop-Process -Id $taskProcess.Id}
    if($null -ne $taskBackup){[IO.File]::WriteAllBytes($taskSettings,$taskBackup)}
}
