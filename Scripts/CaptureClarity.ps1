param([ValidateSet('TAA','TSR','TSRHigh')][string]$Method='TSR',[ValidateRange(50,100)][int]$Scale=100,[ValidateSet('Arena','Split','Portrait')][string]$View='Arena',[ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Label='current',[ValidateRange(1,3)][int]$AAQuality=2,[switch]$ShadowPolish)
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskArchive=Join-Path $taskRoot 'Saved\Builds\Windows'
$taskExe=Join-Path $taskArchive 'WarlockArena\Binaries\Win64\WarlockArena.exe'
$taskSettings=Join-Path $taskArchive 'WarlockArena\Saved\Config\Windows\GameUserSettings.ini'
$taskBackup=if(Test-Path -LiteralPath $taskSettings){[IO.File]::ReadAllBytes($taskSettings)}else{$null}
$taskName='clarity-'+$Label+'-'+$View.ToLower()+'-'+$Method.ToLower()+'-'+$Scale
if($ShadowPolish){$taskName+='-shadows'}
$taskPng=Join-Path $taskRoot ('Saved\Validation\'+$taskName+'.png');$taskLog=Join-Path $taskRoot ('Saved\Validation\'+$taskName+'.log')
$taskMap='/Game/WarlockArena/Maps/Arena'+$(if($View -eq 'Split'){'?LocalPlayers=2?Input=Mixed'}else{''})
$taskCommands=@('DisableAllScreenMessages','Scalability 1',('sg.AntiAliasingQuality '+$AAQuality),('r.AntiAliasingMethod '+$(if($Method -eq 'TAA'){'2'}else{'4'})),('r.ScreenPercentage '+$Scale),'r.VSync 0','t.MaxFPS 0')
if($Method -eq 'TSRHigh'){$taskCommands+='sg.AntiAliasingQuality 3'}
if($ShadowPolish){$taskCommands+=@('r.Shadow.Virtual.ProjectionMethodDirectional 0','r.Shadow.Virtual.SMRT.RayCountDirectional 4','r.Shadow.Virtual.SMRT.SamplesPerRayDirectional 4')}
if($View -eq 'Portrait'){$taskCommands+='PreviewCharacter 0'}
$taskCommands+=@('set WarlockMatchState Phase RESULT','r.AntiAliasingMethod','sg.AntiAliasingQuality','r.ScreenPercentage',('CaptureScene '+$taskPng.Replace('\','/')+' 6'))
$taskArgs=@($taskMap,'-NoSplash','-NoSound','-RenderOffscreen','-unattended','-windowed','-ResX=1920','-ResY=1080','-csvCaptureFrames=450','-csvGpuStats',('-abslog='+$taskLog),('-ExecCmds="'+($taskCommands -join ',')+'"'))
$taskStart=Get-Date;$taskProcess=Start-Process -FilePath $taskExe -WorkingDirectory $taskArchive -ArgumentList $taskArgs -WindowStyle Hidden -PassThru
try{
    do{Start-Sleep -Milliseconds 500;if($taskProcess.HasExited){throw "Client exited: $taskLog"};$taskDone=(Test-Path -LiteralPath $taskLog) -and (Select-String -LiteralPath $taskLog -Pattern 'Capture Ended. Writing CSV' -Quiet)
    }while((-not $taskDone -or ((Get-Date)-$taskStart).TotalSeconds -lt 10) -and ((Get-Date)-$taskStart).TotalSeconds -lt 58)
    if(-not $taskDone -or -not(Test-Path -LiteralPath $taskPng) -or (Get-Item -LiteralPath $taskPng).LastWriteTime -lt $taskStart){throw 'No fresh completed capture'}
    [pscustomobject]@{Screenshot=$taskPng;Log=$taskLog}
}finally{if(-not $taskProcess.HasExited -and $taskProcess.Path -eq $taskExe){Stop-Process -Id $taskProcess.Id};if($null -ne $taskBackup){[IO.File]::WriteAllBytes($taskSettings,$taskBackup)}}
