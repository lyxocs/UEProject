"""Check the compiled player keeps no stale binding page when pause is reopened."""
import json,re,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];package=root/'Saved/Builds/Windows';exe=package/'WarlockArena/Binaries/Win64/WarlockArena.exe'
settings=package/'WarlockArena/Saved/Config/Windows/GameUserSettings.ini';old=settings.read_bytes() if settings.exists() else None
out=root/'Saved/Validation';log=out/'packaged-keybind-menu.log'
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
query='getall WarlockController bKeybindMenuOpen'
commands=','.join(['TogglePause','OpenPausePage 3',query,'TogglePause',query,'TogglePause',query,'Quit'])
try:
 result=subprocess.run([str(exe),'/Game/WarlockArena/Maps/Arena','-NoSplash','-NoSound','-RenderOffscreen','-unattended','-windowed','-ResX=1280','-ResY=720',f'-abslog={log}',f'-ExecCmds={commands}'],cwd=package,startupinfo=startup,timeout=50)
 text=log.read_text(encoding='utf8',errors='replace');states=re.findall(r'\.bKeybindMenuOpen = (True|False)',text)
 assert result.returncode==0 and not re.search(r'Fatal error|Assertion failed',text)
 assert states==['True','False','False'],states
 report={'passed':3,'checks':['packaged binding page opens','continuing clears binding page','reopening pause remains at root'],'states':states,'scope':'actual packaged console/API lifecycle; raw Escape route checked separately in PIE'}
 (out/'packaged-keybind-menu.json').write_text(json.dumps(report,indent=2),encoding='utf8');print(json.dumps(report))
finally:
 if old is None:settings.unlink(missing_ok=True)
 else:settings.write_bytes(old)
