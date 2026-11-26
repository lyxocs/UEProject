param([ValidateSet('Crimson','Moon','Grove','Solo','Split','Ready','Native','Shop','Fight','SplitFight','KeyboardMouseFight','Pause','SplitPause','Fireball','Spell','ShopModes','ShopHaste','SplitShop','Haste')][string]$View='Solo',[ValidateSet('Default','TAAU','TSR')][string]$AAMethod='Default',[ValidateRange(0,7)][int]$Skill=0,[ValidateRange(1,5)][int]$Difficulty=3,[ValidateSet('Effect','Windup','Idle')][string]$CastStage='Effect')
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskArchive=Join-Path $taskRoot 'Saved\Builds\Windows'
$taskExe=Join-Path $taskArchive 'WarlockArena\Binaries\Win64\WarlockArena.exe'
$taskSettings=Join-Path $taskArchive 'WarlockArena\Saved\Config\Windows\GameUserSettings.ini'
$taskSavedSettings=$null
if($View -ne 'Native' -and (Test-Path -LiteralPath $taskSettings)){$taskSavedSettings=[IO.File]::ReadAllBytes($taskSettings)}
$taskSuffix=if($AAMethod -eq 'Default'){''}else{'-'+$AAMethod.ToLower()}
if($View -eq 'Spell'){$taskSuffix+=if($CastStage -eq 'Effect'){'-'+$Skill}else{'-'+$CastStage.ToLower()+'-'+$Skill}}
$taskOutput=Join-Path $taskRoot ('Saved\Validation\visual-'+$View.ToLower()+$taskSuffix+'.png')
$taskLog=Join-Path $taskRoot ('Saved\Validation\visual-'+$View.ToLower()+$taskSuffix+'.log')
$taskMap='/Game/WarlockArena/Maps/Arena'
if($View -in @('Split','Ready','SplitFight','SplitPause','SplitShop')){$taskMap+='?LocalPlayers=2?Input=Mixed'}
if($View -eq 'KeyboardMouseFight'){$taskMap+='?LocalPlayers=2?Input=KeyboardMouse'}
$taskCommands=@('DisableAllScreenMessages')
if($AAMethod -eq 'TAAU'){$taskCommands+=@('r.AntiAliasingMethod 2','r.TemporalAA.Upsampling 1')}
if($AAMethod -eq 'TSR'){$taskCommands+='r.AntiAliasingMethod 4'}
if($View -ne 'Native'){$taskCommands+=@('Scalability 2','r.ScreenPercentage 100')}
if($View -eq 'Shop'){$taskCommands+='ToggleShop'}
if($View -in @('ShopModes','SplitShop')){$taskCommands+=@('ke WarlockController ServerBuy 3','ke WarlockController ServerShopMode 3','ke WarlockController ToggleShop')}
if($View -eq 'ShopHaste'){$taskCommands+=@('set WarlockController SelectedProduct 20','ke WarlockController ServerBuy 20','ToggleShop')}
if($View -eq 'Haste'){$taskCommands+=@('PreviewSpell 6','set WarlockState DefenseChoice 1','ShowHUD')}
if($View -in @('Fight','SplitFight','KeyboardMouseFight')){$taskCommands+='BeginTestRound'}
if($View -eq 'Fireball'){$taskCommands+=@('BeginTestRound','ke WarlockPawn Cast 0')}
if($View -eq 'Spell'){$taskCommands+=@("PreviewSpell $Skill",'ShowHUD')}
if($View -in @('Crimson','Moon','Grove')){
    $taskIndex=@('Crimson','Moon','Grove').IndexOf($View)
    $taskCommands+="PreviewCharacter $taskIndex"
    $taskCommands+='ShowHUD'
}
$taskCaptureDelay=if($View -in @('Fight','SplitFight','KeyboardMouseFight')){12}else{5}
if($View -eq 'Fireball'){$taskCaptureDelay=1}
if($View -eq 'Haste'){$taskCaptureDelay=4.8}
if($View -eq 'Spell'){$taskCaptureDelay=@(4.85,5.1,4.65,4.5,4.85,5.35,4.65,4.98)[$Skill]}
if($View -eq 'Spell' -and $CastStage -eq 'Windup'){$taskCaptureDelay=4.2+@(0.36,0.7,0.42,0.30,0.45,0.60,0.33,0.39)[$Skill]*.75}
if($View -eq 'Spell' -and $CastStage -eq 'Idle'){$taskCaptureDelay=4.1}
if($View -in @('Pause','SplitPause')){$taskCommands+=@('TogglePause',("SetAIDifficulty "+$Difficulty),('Shot showui filename='+$taskOutput.Replace('\','/')+' -nosuffix'))}
else{$taskCommands+=("CaptureScene "+$taskOutput.Replace('\','/')+' '+$taskCaptureDelay)}
$taskArgs=@($taskMap,'-NoSplash','-NoSound',('-abslog='+$taskLog),('-ExecCmds="'+($taskCommands -join ',')+'"'))
# Offscreen uses FNullApplication's virtual monitor; unattended also suppresses
# per-monitor DPI in Windows. Neither can validate a normal desktop launch.
if($View -ne 'Native'){$taskArgs+=@('-RenderOffscreen','-unattended')}
if($View -ne 'Native'){$taskArgs+=@('-windowed','-ResX=1600','-ResY=900')}
$taskDuration=25
if($View -in @('Solo','Split','Native')){$taskArgs+=@('-csvCaptureFrames=300','-csvGpuStats');$taskDuration=40}
if($View -eq 'SplitFight'){$taskArgs+=@('-csvCaptureFrames=900','-csvGpuStats');$taskDuration=35}
$taskProcess=Start-Process -FilePath $taskExe -WorkingDirectory $taskArchive -ArgumentList $taskArgs -WindowStyle Hidden -PassThru
$taskStart=Get-Date
try{
    do {
        Start-Sleep -Milliseconds 500
        if($taskProcess.HasExited){throw "Game exited before capture; inspect $taskLog"}
        $taskCaptured=(Test-Path -LiteralPath $taskOutput) -and (Get-Item -LiteralPath $taskOutput).LastWriteTime -gt $taskStart
    }while((-not $taskCaptured -or ((Get-Date)-$taskStart).TotalSeconds -lt $taskDuration) -and ((Get-Date)-$taskStart).TotalSeconds -lt 58)
    if(-not $taskCaptured){throw "Screenshot not produced; inspect $taskLog"}
    [pscustomobject]@{Screenshot=$taskOutput;Log=$taskLog;ProcessId=$taskProcess.Id}
}finally{
    # Stop only the process created here, after verifying the executable path.
    if(-not $taskProcess.HasExited -and $taskProcess.Path -eq $taskExe){Stop-Process -Id $taskProcess.Id}
    if($null -ne $taskSavedSettings){[IO.File]::WriteAllBytes($taskSettings,$taskSavedSettings)}
}
