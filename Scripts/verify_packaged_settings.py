"""Restart the packaged client with isolated generated settings, restoring originals."""
import configparser,json,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];archive=ROOT/'Saved/Builds/Windows'
exe=archive/'WarlockArena/Binaries/Win64/WarlockArena.exe'
settings=archive/'WarlockArena/Saved/Config/Windows/GameUserSettings.ini'
output=ROOT/'Saved/Validation';original=settings.read_bytes() if settings.exists() else None
checks=[];section='/Script/WarlockArena.WarlockUserSettings'
def read():
    result=configparser.ConfigParser(interpolation=None,strict=False);result.read(settings,encoding='utf-8-sig');return result
def check(name,result):
    if not result:raise AssertionError(name)
    checks.append(name)
def run(name,commands):
    png=output/f'client-settings-{name}.png';log=output/f'client-settings-{name}.log'
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    start=time.time();command=','.join(commands+['r.VSync','t.MaxFPS','r.Lumen.DiffuseIndirect.Allow','r.Lumen.Reflections.Allow','r.ScreenPercentage','r.AntiAliasingMethod','r.TemporalAA.Quality','r.TSR.History.ScreenPercentage','getall WarlockUserSettings GraphicsPreset','getall WarlockUserSettings CastTriggerMode','getall WarlockMatchState ParticipantCount',f'CaptureScene {png.as_posix()} 4'])
    process=subprocess.Popen([str(exe),'/Game/WarlockArena/Maps/Arena','-NoSplash','-NoSound','-RenderOffscreen','-unattended','-windowed','-ResX=1600','-ResY=900',f'-abslog={log}',f'-ExecCmds={command}'],cwd=archive,startupinfo=startup)
    try:
        while time.time()-start<55:
            if process.poll() is not None:raise RuntimeError(f'Client exited; see {log}')
            if png.exists() and png.stat().st_mtime>start:break
            time.sleep(.25)
        else:raise TimeoutError(str(log))
        time.sleep(.2)
    finally:
        if process.poll() is None:process.terminate();process.wait(timeout=10)
    text=log.read_text(encoding='utf8',errors='replace')
    check(name+' no fatal or material fallback',not re.search(r'Fatal error|Assertion failed|Failed to compile Material|Default Material will be used',text))
    return read(),text
try:
    settings.parent.mkdir(parents=True,exist_ok=True)
    # Generated test fixture, never a source asset or a player's original file.
    settings.write_text('[ScalabilityGroups]\n'+''.join(f'sg.{key}Quality=2\n' for key in ['ViewDistance','AntiAliasing','Shadow','GlobalIllumination','Reflection','PostProcess','Texture','Effects','Foliage','Shading'])+'sg.ResolutionQuality=100\n\n[/Script/Engine.GameUserSettings]\nVersion=5\nbUseVSync=False\nResolutionSizeX=1600\nResolutionSizeY=900\nFullscreenMode=2\nFrameRateLimit=90\n\n[WarlockDisplay]\nRevision=3\n',encoding='utf8')
    cfg,log=run('migration',[])
    check('legacy cap preserved',cfg.getfloat(section,'FrameRateLimit')==90)
    check('legacy window resolution preserved',cfg.getint(section,'ResolutionSizeX')==1600 and cfg.getint(section,'ResolutionSizeY')==900)
    check('legacy custom quality preserved',cfg.getint(section,'GraphicsPreset')==-1 and cfg.getint('ScalabilityGroups','sg.GlobalIlluminationQuality')==2)
    cfg,log=run('save',['SetGraphicsPreset 1','EditDisplaySetting 4 1','EditDisplaySetting 5 1','TogglePause','SetParticipantCount 8','TogglePause'])
    check('eight-seat total saved',cfg.getint('WarlockRoster','Participants')==8)
    check('new cap saved',cfg.getfloat(section,'FrameRateLimit')==120)
    check('VSync saved',cfg.getboolean(section,'bUseVSync'))
    check('fast preset and 85 percent saved',cfg.getint(section,'GraphicsPreset')==1 and cfg.getfloat('ScalabilityGroups','sg.ResolutionQuality')==85)
    check('fast uses HQ TAA without TSR history cost',cfg.getint(section,'AntiAliasingMode')==0 and re.search(r'r.AntiAliasingMethod\s*=\s*"?2',log) is not None and re.search(r'r.TemporalAA.Quality\s*=\s*"?2',log) is not None)
    cfg,log=run('reload',[])
    check('eight-seat total survives actual process restart',re.search(r'\.ParticipantCount = 8\b',log) is not None)
    check('cap survives process restart',cfg.getfloat(section,'FrameRateLimit')==120 and re.search(r't.MaxFPS\s*=\s*"?120',log) is not None)
    check('VSync survives process restart',cfg.getboolean(section,'bUseVSync') and re.search(r'r.VSync\s*=\s*"?1',log) is not None)
    check('fast GI remains disabled after restart',cfg.getint('ScalabilityGroups','sg.GlobalIlluminationQuality')==0 and re.search(r'r.Lumen.DiffuseIndirect.Allow\s*=\s*"?0',log) is not None)
    check('fast reflections remain disabled after restart',cfg.getint('ScalabilityGroups','sg.ReflectionQuality')==0 and re.search(r'r.Lumen.Reflections.Allow\s*=\s*"?0',log) is not None)
    check('scale survives process restart',cfg.getfloat('ScalabilityGroups','sg.ResolutionQuality')==85 and re.search(r'r.ScreenPercentage\s*=\s*"?85',log) is not None)
    for name,commands,mode,method,history in [('tsr-save',['EditDisplaySetting 10 1'],1,4,100),('tsr-reload',[],1,4,100),('tsr-fine',['EditDisplaySetting 10 1'],2,4,200),('taa-return',['EditDisplaySetting 10 1'],0,2,200)]:
        cfg,log=run(name,commands)
        check(name+' persists explicit AA mode',cfg.getint(section,'AntiAliasingMode')==mode)
        check(name+' applies actual renderer AA method',re.search(r'r.AntiAliasingMethod\s*=\s*"?'+str(method),log) is not None)
        if method==4:check(name+' applies expected TSR history resolution',re.search(r'r.TSR.History.ScreenPercentage\s*=\s*"?'+str(history),log) is not None)
        check(name+' retains performance lighting and scale',cfg.getint('ScalabilityGroups','sg.GlobalIlluminationQuality')==0 and cfg.getfloat('ScalabilityGroups','sg.ResolutionQuality')==85)
    cfg,log=run('audio-cast-save',['EditDisplaySetting 11 1','EditDisplaySetting 12 -1','EditDisplaySetting 13 1'])
    check('music volume saved independently',abs(cfg.getfloat(section,'MusicVolume')-.38)<.001)
    check('skill effects volume saved independently',abs(cfg.getfloat(section,'EffectsVolume')-.65)<.001)
    check('press trigger mode saved',cfg.getint(section,'CastTriggerMode')==0)
    cfg,log=run('audio-cast-reload',[])
    check('music volume survives actual process restart',abs(cfg.getfloat(section,'MusicVolume')-.38)<.001)
    check('FX volume survives actual process restart',abs(cfg.getfloat(section,'EffectsVolume')-.65)<.001)
    check('actual loaded input mode survives process restart',cfg.getint(section,'CastTriggerMode')==0 and re.search(r'\.CastTriggerMode = 0\b',log) is not None)
    settings.write_text('',encoding='utf8')
    cfg,log=run('fresh',[])
    check('fresh installation retains four-seat default',re.search(r'\.ParticipantCount = 4\b',log) is not None)
    check('fresh installation chooses balanced',cfg.getint(section,'GraphicsPreset')==2)
    check('fresh installation uses cap 120 and VSync off',cfg.getfloat(section,'FrameRateLimit')==120 and not cfg.getboolean(section,'bUseVSync'))
    check('fresh installation has bounded render scale',50<=cfg.getfloat('ScalabilityGroups','sg.ResolutionQuality')<=100)
    check('fresh installation chooses high-quality TAA',cfg.getint(section,'AntiAliasingMode')==0 and re.search(r'r.AntiAliasingMethod\s*=\s*"?2',log) is not None)
    cfg,log=run('auto-budget-save',['SetGraphicsPreset 3'])
    check('high preset explicitly enables bounded rendering budget',cfg.getboolean(section,'bAutoRenderBudget'))
    cfg,log=run('auto-budget-reload',[])
    check('render budget selection survives restart',cfg.getboolean(section,'bAutoRenderBudget'))
    cfg,log=run('fixed-budget',['EditDisplaySetting 14 1'])
    check('budget can be explicitly disabled',not cfg.getboolean(section,'bAutoRenderBudget'))
    check('fixed budget restores requested full scene scale',re.search(r'r.ScreenPercentage\s*=\s*"?100',log) is not None)
    check('fresh installation defaults to release targeting',cfg.getint(section,'CastTriggerMode')==1)
    keys=['W','Down','Left','Right','Z','F','D','R','T','E','C','Y','K','X','B','Enter','M','Q','Delete','J','H','F1']
    settings.write_text('[/Script/WarlockArena.WarlockUserSettings]\nVersion=5\nbClientSettingsInitialized=True\nGraphicsPreset=1\nResolutionSizeX=1600\nResolutionSizeY=900\nFullscreenMode=2\nFrameRateLimit=120\nbUseVSync=False\n'+''.join('KeyboardBindings='+k+'\n' for k in keys),encoding='utf8')
    for name in ['keyboard','keyboard-reload']:
        cfg,log=run(name,['getall WarlockUserSettings KeyboardBindings'])
        exported=log.split('.KeyboardBindings =',1)[1] if '.KeyboardBindings =' in log else ''
        loaded=[value.strip() for value in re.findall(r'\]\s+\d+: ([^\r\n]+)',exported)][:22]
        check(name+' loads all 22 saved keyboard bindings',loaded==keys)
finally:
    if original is not None:settings.write_bytes(original)
    else:settings.unlink(missing_ok=True)
report={'passed':len(checks),'checks':checks,'scope':'actual packaged process restarts; existing generated config restored'}
(output/'packaged-settings.json').write_text(json.dumps(report,indent=2),encoding='utf8');print(json.dumps(report,indent=2))
