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
    start=time.time();command=','.join(commands+['r.VSync','t.MaxFPS','r.Lumen.DiffuseIndirect.Allow','r.Lumen.Reflections.Allow','r.ScreenPercentage','getall WarlockUserSettings GraphicsPreset',f'CaptureScene {png.as_posix()} 4'])
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
    cfg,log=run('save',['SetGraphicsPreset 1','EditDisplaySetting 4 1','EditDisplaySetting 5 1'])
    check('new cap saved',cfg.getfloat(section,'FrameRateLimit')==120)
    check('VSync saved',cfg.getboolean(section,'bUseVSync'))
    check('fast preset and 75 percent saved',cfg.getint(section,'GraphicsPreset')==1 and cfg.getfloat('ScalabilityGroups','sg.ResolutionQuality')==75)
    cfg,log=run('reload',[])
    check('cap survives process restart',cfg.getfloat(section,'FrameRateLimit')==120 and re.search(r't.MaxFPS\s*=\s*"?120',log) is not None)
    check('VSync survives process restart',cfg.getboolean(section,'bUseVSync') and re.search(r'r.VSync\s*=\s*"?1',log) is not None)
    check('fast GI remains disabled after restart',cfg.getint('ScalabilityGroups','sg.GlobalIlluminationQuality')==0 and re.search(r'r.Lumen.DiffuseIndirect.Allow\s*=\s*"?0',log) is not None)
    check('fast reflections remain disabled after restart',cfg.getint('ScalabilityGroups','sg.ReflectionQuality')==0 and re.search(r'r.Lumen.Reflections.Allow\s*=\s*"?0',log) is not None)
    check('scale survives process restart',cfg.getfloat('ScalabilityGroups','sg.ResolutionQuality')==75 and re.search(r'r.ScreenPercentage\s*=\s*"?75',log) is not None)
    settings.write_text('',encoding='utf8')
    cfg,log=run('fresh',[])
    check('fresh installation chooses balanced',cfg.getint(section,'GraphicsPreset')==2)
    check('fresh installation uses cap 120 and VSync off',cfg.getfloat(section,'FrameRateLimit')==120 and not cfg.getboolean(section,'bUseVSync'))
    check('fresh installation has bounded render scale',50<=cfg.getfloat('ScalabilityGroups','sg.ResolutionQuality')<=85)
finally:
    if original is not None:settings.write_bytes(original)
report={'passed':len(checks),'checks':checks,'scope':'actual packaged process restarts; existing generated config restored'}
(output/'packaged-settings.json').write_text(json.dumps(report,indent=2),encoding='utf8');print(json.dumps(report,indent=2))
