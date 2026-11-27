"""Actual packaged four-seat expert fight, model loading and reaction telemetry."""
import json,re,subprocess,time
from pathlib import Path
root=Path(__file__).resolve().parents[1];package=root/'Saved/Builds/Windows';exe=package/'WarlockArena/Binaries/Win64/WarlockArena.exe'
settings=package/'WarlockArena/Saved/Config/Windows/GameUserSettings.ini';original=settings.read_bytes() if settings.exists() else None
out=root/'Saved/Validation/expert-packaged';out.mkdir(exist_ok=True);prefix=out/'state';log=out/'client.log';png=out/'battle.png'
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
commands=f'TogglePause,SetAIDifficulty 6,SetParticipantCount 3,SetParticipantCount 4,TogglePause,SetGraphicsPreset 1,BeginTestRound,CaptureScene {png.as_posix()} 7'
proc=subprocess.Popen([str(exe),'/Game/WarlockArena/Maps/Arena','-NoSound','-NoSplash','-RenderOffscreen','-unattended','-windowed','-ResX=1600','-ResY=900',f'-WarlockNetTest={prefix}','-WarlockNetTestPeriod=0.5',f'-abslog={log}',f'-ExecCmds={commands}'],cwd=package,startupinfo=startup)
start=time.time();samples=[]
try:
 while time.time()-start<35:
  assert proc.poll() is None,'Packaged client exited early'
  try:
   if prefix.with_suffix('.json').stat().st_mtime<start:time.sleep(.1);continue
   s=json.loads(prefix.with_suffix('.json').read_text(encoding='utf8'));bots=[p for p in s['roster'] if p['bot']]
   if s['difficulty']==6 and s['expert_model_loaded'] and s['phase']=='FIGHT' and len(bots)==3 and all(p.get('expert_decisions',0)>3 for p in bots):
    assert all(p['expert_minimum_reaction']>=.18 for p in bots)
    samples.append({'round':s['round'],'decisions':[p['expert_decisions'] for p in bots],'minimum_reactions':[p['expert_minimum_reaction'] for p in bots]})
    if len(samples)>=12 and png.exists() and png.stat().st_mtime>start:break
  except (OSError,json.JSONDecodeError,KeyError):pass
  time.sleep(.5)
 else:raise AssertionError('Model/difficulty/brain telemetry did not become ready')
 text=log.read_text(encoding='utf8',errors='replace');assert not re.search(r'Fatal error|Assertion failed|Default Material will be used',text)
 report={'passed':4,'checks':['sixth tier in actual packaged four-seat fight','new model loaded from staged Content','all three expert brains make decisions','observed reaction minimum never below 180ms'],'samples':samples,'scope':'same-PC rendered process; not physical input or a frame-rate benchmark'}
 (out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf8');print(json.dumps(report))
finally:
 if proc.poll() is None:proc.terminate();proc.wait(timeout=15)
 if original is None:settings.unlink(missing_ok=True)
 else:settings.write_bytes(original)
