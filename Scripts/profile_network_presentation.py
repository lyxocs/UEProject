"""A/B actual DS snapshots vs client display/camera at 4K; one rendered + one NullRHI peer.

Use an isolated server, never a room with human players. Writes opt-in Development
commands only. Reports packet-step holds separately from GPU/CPU frame time.
"""
import argparse,csv,json,math,statistics,subprocess,time,re
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--address',required=True);p.add_argument('--client-exe',type=Path,required=True);p.add_argument('--smooth',type=int,choices=[0,1],default=1);p.add_argument('--impaired',action='store_true');p.add_argument('--width',type=int,default=3840);p.add_argument('--height',type=int,default=2160);a=p.parse_args()
root=Path(__file__).resolve().parents[1];exe=a.client_exe.resolve();package=exe.parents[3]
out=root/'Saved/Validation'/f'network-presentation-{a.smooth}-{"jitter" if a.impaired else "normal"}';out.mkdir(parents=True,exist_ok=True)
settings=exe.parents[2]/'Saved/Config/Windows/GameUserSettings.ini';old=settings.read_bytes() if settings.exists() else None
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
processes=[];ids=[0,0];states=[{},{}];start=time.time()
def state(i):
 try:states[i]=json.loads((out/f'p{i}.json').read_text(encoding='utf-8-sig'))
 except (OSError,json.JSONDecodeError):pass
 return states[i]
def wait(predicate,timeout=40):
 until=time.monotonic()+timeout
 while time.monotonic()<until:
  if predicate():return
  assert all(x.poll() is None for x in processes),'client exited'
  time.sleep(.1)
 raise TimeoutError(str(out))
def cmd(i,action,**values):
 ids[i]+=1;(out/f'p{i}.command.json').write_text(json.dumps(dict(id=ids[i],action=action,**values)),encoding='utf8')
 wait(lambda:state(i).get('command',0)>=ids[i])
def console(command):cmd(0,'profile',commands=command)
try:
 if old is not None:
  fixture=re.sub(r'(?m)^FrameRateLimit=.*$', 'FrameRateLimit=120.000000',old.decode('utf-8-sig'));settings.write_text(fixture,encoding='utf8')
 for i in range(2):
  (out/f'p{i}.json').write_text('{}',encoding='utf8');(out/f'p{i}.command.json').write_text('{"id":0,"action":"noop"}',encoding='utf8')
  args=[str(exe),'/Game/WarlockArena/Maps/Arena','-NoSplash','-NoSound','-unattended',f'-WarlockNetTest={out/f"p{i}"}','-WarlockNetTestPeriod=0.5',f'-abslog={out/f"p{i}.log"}','-nullrhi' if i else '-RenderOffscreen','-windowed',f'-ResX={a.width}',f'-ResY={a.height}','-ExecCmds=SetGraphicsPreset 3,r.VSync 0,t.MaxFPS 120,r.GPUCsvStatsEnabled 1']
  processes.append(subprocess.Popen(args,cwd=package,startupinfo=startup))
  wait(lambda i=i:'self' in state(i),70);cmd(i,'join',address=a.address,name=f'Perf{i}')
 wait(lambda:all(state(i).get('phase')=='LOBBY' and state(i).get('net_mode')==3 for i in range(2)))
 host=0 if state(0)['host']==state(0)['self'] else 1
 cmd(host,'room',value=4,difficulty=3)
 for i in range(2):cmd(i,'ready')
 wait(lambda:state(0).get('phase')=='SHOP')
 cmd(0,'buy',value=3)
 console(f'warlock.NetworkSmoothing {a.smooth}')
 console('t.MaxFPS 120');console('r.VSync 0')
 if a.impaired:
  for command in ['Net PktIncomingLagMin=40','Net PktIncomingLagMax=80','Net PktIncomingLoss=2','Net PktLag=60','Net PktLagVariance=20','Net PktLoss=2']:console(command)
 cmd(0,'click',x=-600,y=-300);time.sleep(7.5)
 console('csvprofile start');time.sleep(.5)
 # Long straight paths, safely inside the arena and away from the pillars.
 for x in [600,-600,600]:cmd(0,'click',x=x,y=-300);time.sleep(4.5)
 # Fireballs still move and push without damage in preparation.
 cmd(0,'stop');cmd(0,'cast',value=0,x=600,y=300);time.sleep(3)
 console('csvprofile stop');time.sleep(2)
 cmd(0,'cast',value=3,x=300,y=0);time.sleep(.5);console('getall WarlockPawn CastDuration')
 def blink_verified():
  log=(out/'p0.log').read_text(encoding='utf8',errors='replace')
  return any(abs(float(v)-.03)<.0001 for v in re.findall(r'\.CastDuration = ([0-9.]+)',log))
 wait(blink_verified,5) # Wait for the asynchronous UE log writer as well.
 files=[f for f in (exe.parents[2]/'Saved/Profiling/CSV').glob('*.csv') if f.stat().st_mtime>start];assert files
 source=max(files,key=lambda f:f.stat().st_mtime);(out/'profile.csv').write_bytes(source.read_bytes())
 csv.field_size_limit(16*1024*1024)
 rows=list(csv.reader((out/'profile.csv').open(newline='')));header=next(r for r in reversed(rows) if r and r[0]=='EVENTS');data=[]
 for row in rows:
  if not row or row[0]=='EVENTS':continue
  try:d={k:float(v or 0) for k,v in zip(header[1:],row[1:])}
  except ValueError:continue
  if d.get('FrameTime',0)>0 and 'WarlockAuthX' in d:data.append(d)
 assert len(data)>300
 def stat(values):
  s=sorted(values);n=len(s)
  return dict(mean=statistics.mean(s),p95=s[math.ceil(n*.95)-1],p99=s[math.ceil(n*.99)-1])
 def movement(prefix):
  speeds=[]
  for before,after in zip(data,data[1:]):
   # Restrict to the middle of horizontal paths, excluding starts/stops/turns.
   if not(-400<after['WarlockAuthX']<350 and abs(after['WarlockAuthY']+300)<25):continue
   dx=after[prefix+'X']-before[prefix+'X'];dy=after[prefix+'Y']-before[prefix+'Y']
   speeds.append(math.hypot(dx,dy)*1000/after['FrameTime'])
  assert len(speeds)>100
  return dict(frames=len(speeds),held_fraction=sum(v<.01 for v in speeds)/len(speeds),speed_cv=statistics.pstdev(speeds)/max(.001,statistics.mean(speeds)),speed=stat(speeds))
 projectile=[]
 for before,after in zip(data,data[1:]):
  if after.get('WarlockProjectileId',0)>0 and before.get('WarlockProjectileId')==after['WarlockProjectileId']:
   projectile.append(math.hypot(after.get('WarlockProjectileDisplayX',0)-before.get('WarlockProjectileDisplayX',0),after.get('WarlockProjectileDisplayY',0)-before.get('WarlockProjectileDisplayY',0)))
 report=dict(smoothing=a.smooth,impaired=a.impaired,output=[a.width,a.height],frames=len(data),ms={key:stat([d.get(key,0) for d in data]) for key in ['FrameTime','GameThreadTime','GPUTime']},motion={prefix:movement(prefix) for prefix in ['WarlockAuth','WarlockDisplay','WarlockCamera']},ping_ms=stat([d.get('WarlockPingMs',0) for d in data]),projectile_frames=len(projectile),projectile_held_fraction=sum(x<.001 for x in projectile)/len(projectile) if projectile else None,scope='4 seats, preparation movement and fireball; one rendered client plus independent NullRHI peer; not human Internet play or combat FPS certification')
 report['network']={key:stat([d.get(key,0) for d in data]) for key in header if key.startswith('Replication/') or key in ['Exclusive/GameThread/NetworkIncoming','Exclusive/GameThread/NetworkOutgoing']}
 report['server_blink_duration_verified']=.03
 (out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf8');print(json.dumps({'report':str(out/'report.json'),'frame_ms':report['ms']['FrameTime'],'motion':{k:{'held_fraction':v['held_fraction'],'speed_cv':v['speed_cv']} for k,v in report['motion'].items()},'projectile_held_fraction':report['projectile_held_fraction']}),flush=True)
finally:
 for i in range(len(processes)):
  ids[i]+=1;(out/f'p{i}.command.json').write_text(json.dumps({'id':ids[i],'action':'leave'}),encoding='utf8')
 if processes:time.sleep(1)
 for process in processes:
  if process.poll() is None:process.terminate();process.wait(timeout=15)
 if old is None:settings.unlink(missing_ok=True)
 else:settings.write_bytes(old)
