"""Packaged-client 4K benchmark; preserves generated player settings.

Default is four combatants, high preset. Native comparison:
 --extra "r.DynamicRes.OperationMode 0,r.ScreenPercentage 100"
CSV exit code 777003 is Unreal's normal -ExitAfterCsvProfiling result.
"""
import argparse,csv,json,math,statistics,subprocess,time,shutil,re
from pathlib import Path
p=argparse.ArgumentParser()
for name,default in [('preset',3),('frames',3600),('width',3840),('height',2160),('seats',4),('warmup',360)]:p.add_argument('--'+name,type=int,default=default)
p.add_argument('--label',default='4k-four-high');p.add_argument('--extra',default='');p.add_argument('--rich',action='store_true',help='500 starting gold, legal shop RPCs, auto-cast local player');p.add_argument('--observe',action='store_true',help='enable file-based state observation; adds synchronous I/O overhead');a=p.parse_args()
root=Path(__file__).resolve().parents[1];package=root/'Saved/Builds/Windows';exe=package/'WarlockArena/Binaries/Win64/WarlockArena.exe'
out=root/'Saved/Validation'/('client-perf-'+a.label);out.mkdir(parents=True,exist_ok=True)
settings=package/'WarlockArena/Saved/Config/Windows/GameUserSettings.ini';original=settings.read_bytes() if settings.exists() else None
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
commands=f'TogglePause,SetParticipantCount {a.seats},TogglePause,SetGraphicsPreset {a.preset},r.VSync 0,t.MaxFPS 0,r.GPUCsvStatsEnabled 1,BeginTestRound,CaptureScene {(out/"screen.png").as_posix()} 2'
if a.extra:commands+=','+a.extra
if a.rich:commands=commands.replace(',BeginTestRound','')
start=time.time();proc=None;samples=[]
command_id=0;buy_index=0;cast_index=0;next_cast=0
plan=[109,3,113,116,118,121,8,9,10,0]*2
def send(action,value=0,x=400,y=0):
 global command_id
 command_id+=1
 (out/'state.command.json').write_text(json.dumps({'id':command_id,'action':action,'value':value,'x':x,'y':y}),encoding='utf8')
try:
 # Uncapping t.MaxFPS does not change the saved frame-budget target. Pin the
 # transient fixture to 120 as well, then restore the exact original bytes.
 if original is not None:
  fixture=original.decode('utf-8-sig');fixture=re.sub(r'(?m)^FrameRateLimit=.*$', 'FrameRateLimit=120.000000',fixture);settings.write_text(fixture,encoding='utf8')
 options=['-ini:Game:[/Script/WarlockArena.WarlockEconomy]:StartingGold=500'] if a.rich else []
 if a.rich or a.observe:options += [f'-WarlockNetTest={(out/"state").as_posix()}','-WarlockNetTestPeriod=0.5']
 proc=subprocess.Popen([str(exe),'/Game/WarlockArena/Maps/Arena','-NoSound','-RenderOffscreen','-unattended','-NoSplash','-windowed',f'-ResX={a.width}',f'-ResY={a.height}',f'-csvCaptureFrames={a.frames}','-csvCompression=0','-ExitAfterCsvProfiling',f'-abslog={out/"client.log"}',f'-ExecCmds={commands}',*options],cwd=package,startupinfo=startup)
 while proc.poll() is None:
  if time.time()-start>240:raise TimeoutError('profiling exceeded 240 seconds')
  try:
   state=json.loads((out/'state.json').read_text(encoding='utf8'));samples.append({'elapsed':time.time()-start,'phase':state['phase'],'round':state['round'],'seats':state['seats'],'alive':sum(x['alive'] for x in state['roster'])})
   if a.rich and state.get('command',0)>=command_id:
    if state['phase']=='SHOP':
     if buy_index<len(plan):send('buy',plan[buy_index]);buy_index+=1
     else:send('ready')
    elif state['phase']=='FIGHT' and time.time()>=next_cast:
     others=[x for x in state['roster'] if x['id']!=state.get('self') and x['alive']]
     if others:send('cast',[0,2,4,5,7,6,3,1][cast_index%8],others[0]['x'],others[0]['y']);cast_index+=1;next_cast=time.time()+1
  except (OSError,json.JSONDecodeError,KeyError):pass
  time.sleep(.25)
 log=(out/'client.log').read_text(encoding='utf8',errors='replace')
 assert proc.returncode in (0,777003) and 'Capture Ended' in log and 'Fatal error' not in log,proc.returncode
 files=[x for x in (package/'WarlockArena/Saved/Profiling/CSV').glob('*.csv') if x.stat().st_mtime>start];assert files
 shutil.copy2(max(files,key=lambda x:x.stat().st_mtime),out/'profile.csv')
 csv.field_size_limit(16*1024*1024)
 with (out/'profile.csv').open(newline='') as f:rows=list(csv.reader(f))
 header=next(r for r in reversed(rows) if r and r[0]=='EVENTS');initial=rows[0]
 assert header[:len(initial)]==initial,'CSV columns were reordered, not just appended'
 data=[]
 for row in rows:
  if not row or row[0]=='EVENTS' or len(row)>len(header):continue
  try:d={k:float(v or 0) for k,v in zip(header[1:],row[1:])}
  except ValueError:continue
  if d.get('FrameTime',0)>0:data.append(d)
 data=data[a.warmup:];combat_filtered=any('WarlockCombatPhase' in d for d in data)
 if combat_filtered:data=[d for d in data if d.get('WarlockCombatPhase')==1]
 assert len(data)>=300
 def stat(key):
  vs=sorted(x.get(key,0) for x in data);n=len(vs)
  return {'mean':statistics.mean(vs),'median':statistics.median(vs),'p95':vs[math.ceil(n*.95)-1],'p99':vs[math.ceil(n*.99)-1],'min':vs[0],'max':vs[-1]}
 keys=['FrameTime','GameThreadTime','RenderThreadTime','RHIThreadTime','GPUTime']
 summary={'frames':len(data),'warmup_frames':a.warmup,'output':[a.width,a.height],'seats':a.seats,'preset':a.preset,'extra':a.extra,'ms':{k:stat(k) for k in keys},'under_8_333_ms':sum(x['FrameTime']<=1000/120 for x in data)/len(data),'dynamic_resolution_percent':{k:stat(k) for k in header if 'DynamicResolutionPercentage' in k or 'WarlockRenderPercentage' in k},'gpu_ms':sorted([(k,stat(k)['mean']) for k in header if k.startswith('GPU/')],key=lambda x:-x[1])[:20],'scope':'one packaged client with AI; not four remote human clients or native-4K certification','phase_samples':samples}
 summary.update({'combat_filtered':combat_filtered,'rich_fixture':a.rich,'file_observer_enabled':a.rich or a.observe,'scene':{k:stat(k) for k in ['WarlockSeats','WarlockAlive'] if k in header}})
 if 'WarlockAlive' in header:summary['all_combatants_alive_frames']=sum(d.get('WarlockAlive')==a.seats for d in data)
 (out/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf8')
 print(json.dumps({'output':str(out),'frames':len(data),'frame_ms':summary['ms']['FrameTime'],'gpu_ms':summary['ms']['GPUTime'],'dynamic_resolution_percent':summary['dynamic_resolution_percent'],'under_8_333_ms':summary['under_8_333_ms']}),flush=True)
finally:
 if proc and proc.poll() is None:proc.terminate();proc.wait(timeout=15)
 if original is None:settings.unlink(missing_ok=True)
 else:settings.write_bytes(original)
