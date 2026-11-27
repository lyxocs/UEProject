"""Verify a fresh player ZIP, then run its split-screen executable without a development PATH."""
import argparse,ctypes,hashlib,json,os,re,subprocess,time,zipfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--version',required=True);a=p.parse_args()
assert re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*',a.version)
root=Path(__file__).resolve().parents[1];bundle=root/'Saved/Distribution'/f'WarlockArena-Win64-{a.version}.zip'
target=root/'Saved/Distribution'/('Smoke '+a.version)
if not target.exists():
 target.mkdir()
 with zipfile.ZipFile(bundle) as z:
  assert all(not Path(n).is_absolute() and '..' not in Path(n).parts for n in z.namelist())
  z.extractall(target)
package=target/'WarlockArena';manifest=json.loads((package/'BUILD-MANIFEST.json').read_text(encoding='utf8'))
assert manifest['source_commit']==subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()
for name,info in manifest['files'].items():
 file=package/name;assert file.stat().st_size==info['size']
 with file.open('rb') as f:assert hashlib.file_digest(f,'sha256').hexdigest()==info['sha256'],name
exe=package/'WarlockArena/Binaries/Win64/WarlockArena.exe';out=root/'Saved/Validation'/('distribution-'+a.version);out.mkdir(exist_ok=True)
png=out/'split-screen.png';log=out/'client.log'
buffer=ctypes.create_unicode_buffer(260);assert ctypes.windll.kernel32.GetWindowsDirectoryW(buffer,260)
env=os.environ.copy();env.update(SystemRoot=buffer.value,WINDIR=buffer.value,PATH=str(Path(buffer.value)/'System32')+';'+buffer.value)
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
start=time.time();proc=subprocess.Popen([str(exe),'/Game/WarlockArena/Maps/Arena?LocalPlayers=2?Input=Mixed','-NoSound','-NoSplash','-RenderOffscreen','-unattended','-windowed','-ResX=1600','-ResY=900',f'-abslog={log}',f'-ExecCmds=TogglePause,SetParticipantCount 8,TogglePause,SetGraphicsPreset 1,BeginTestRound,CaptureScene {png.as_posix()} 8'],cwd=package,env=env,startupinfo=startup)
try:
 while time.time()-start<55:
  assert proc.poll() is None,'Client exited early'
  if png.exists() and png.stat().st_mtime>start:break
  time.sleep(.25)
 else:raise TimeoutError(str(log))
finally:
 if proc.poll() is None:proc.terminate();proc.wait(timeout=10)
text=log.read_text(encoding='utf8',errors='replace')
assert not re.search(r'Fatal error|Assertion failed|Failed to compile Material|Default Material will be used',text)
assert 'Warlock: roster=8 humans=2' in text
report={'source_commit':manifest['source_commit'],'files_verified':len(manifest['files']),'startup':'actual extracted executable, split-screen, restricted PATH without development tools','fatal_or_material_fallback':False,'limitation':'same PC with installed Microsoft runtimes; not a clean-PC or physical-controller test'}
(out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf8');print(json.dumps(report,indent=2))
