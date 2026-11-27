"""One DS and two independent packaged clients; owned RPCs and replicated observations."""
import argparse,json,math,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--address',default='127.0.0.1:17777');p.add_argument('--local-server',action='store_true');p.add_argument('--server-exe',type=Path);p.add_argument('--headless',action='store_true');args=p.parse_args()
exe=ROOT/'Saved/Builds/Windows/WarlockArena/Binaries/Win64/WarlockArena.exe'
out=ROOT/'Saved/Validation'/('network-'+time.strftime('%Y%m%d-%H%M%S'));out.mkdir(parents=True)
processes=[];checks=[];prefixes=[out/'client1',out/'client2'];ids=[0,0];snapshots=[{},{}]
settings=ROOT/'Saved/Builds/Windows/WarlockArena/Saved/Config/Windows/GameUserSettings.ini';original=settings.read_bytes() if settings.exists() else None
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
def launch(extra):
 proc=subprocess.Popen([str(exe),*extra,'-NoSplash','-unattended','-NoSound'],cwd=exe.parents[3],startupinfo=startup);processes.append(proc);return proc
def state(i):
 try:snapshots[i]=json.loads(prefixes[i].with_suffix('.json').read_text(encoding='utf-8-sig'))
 except (OSError,json.JSONDecodeError):pass
 return snapshots[i]
def wait(label,predicate,timeout=35):
 start=time.monotonic()
 while time.monotonic()-start<timeout:
  if predicate():checks.append(label);print('PASS '+label,flush=True);return
  for proc in processes:
   if proc.poll() is not None:raise RuntimeError(f'Process exited {proc.returncode}: {label}; {out}')
  time.sleep(.1)
 raise AssertionError(f'{label}; states={[state(0),state(1)]}; logs={out}')
def cmd(i,action,value=0,**kwargs):
 ids[i]+=1;prefixes[i].with_suffix('.command.json').write_text(json.dumps(dict(id=ids[i],action=action,value=value,**kwargs)),encoding='utf8');wait(f'client{i+1} command {action}',lambda:state(i).get('command',0)>=ids[i])
def roster(i):return {x['id']:x for x in state(i).get('roster',[])}
def own(i):return roster(i).get(state(i).get('self'),{})
def capture(i,label):
 if args.headless:return
 path=out/(label+f'-p{i+1}.png');cmd(i,'capture',path=path.as_posix());wait('screenshot '+label,lambda:path.exists())
try:
 if args.local_server:
  if not args.server_exe:raise ValueError('--local-server requires an actual Server binary or UnrealEditor --server-exe')
  serverargs=[str(args.server_exe)]+([str(ROOT/'WarlockArena.uproject')] if args.server_exe.stem.startswith('UnrealEditor') else [])+['/Game/WarlockArena/Maps/Arena','-server','-nullrhi','-NoSound','-unattended','-port=17777','-Participants=2',f'-abslog={out / "server.log"}']
  processes.append(subprocess.Popen(serverargs,cwd=ROOT,startupinfo=startup));time.sleep(3)
 for i in range(2):
  launch(['/Game/WarlockArena/Maps/Arena',f'-WarlockNetTest={prefixes[i]}',f'-abslog={prefixes[i]}.log','-windowed','-ResX=1280','-ResY=720','-nullrhi' if args.headless else '-RenderOffscreen','-ExecCmds=SetGraphicsPreset 1,r.VSync 0,t.MaxFPS 45,r.Streaming.PoolSize 256,r.ScreenPercentage 85']);
 for i in range(2):
  wait(f'client{i+1} offline startup before joining',lambda i=i:'self' in state(i),70)
  if i==0 and not args.headless:
   cmd(i,'menu');capture(i,'connection-panel');cmd(i,'closemenu')
  cmd(i,'join',address=args.address,name=f'NetTest-P{i+1}')
 wait('two clients in dedicated lobby',lambda:all(state(i).get('phase')=='LOBBY' and state(i).get('net_mode')==3 and len([x for x in roster(i).values() if not x['bot']])==2 for i in range(2)),70)
 a,b=state(0)['self'],state(1)['self'];assert a!=b
 host=0 if state(0)['host']==a else 1;guest=1-host
 wait('unique owned identities and same roster',lambda:set(roster(0))==set(roster(1)) and len(roster(0))>=2)
 wait('chosen player names replicate',lambda:all(roster(i)[a]['name']=='NetTest-P1' and roster(i)[b]['name']=='NetTest-P2' for i in range(2)))
 cmd(guest,'room',value=3);time.sleep(.5);assert state(guest)['seats']!=3;checks.append('guest cannot change room rules')
 cmd(host,'room',value=3);wait('host creates three seats with one AI',lambda:all(len(roster(i))==3 and sum(x['bot'] for x in roster(i).values())==1 for i in range(2)))
 cmd(host,'room',value=8);wait('eight seats include six distinct AI identities',lambda:all(state(i)['seats']==8 and len(roster(i))==8 and sum(x['bot'] for x in roster(i).values())==6 for i in range(2)))
 cmd(host,'room',value=2);wait('host restores two seats',lambda:all(state(i)['seats']==2 and len(roster(i))==2 for i in range(2)))
 capture(0,'lobby');capture(1,'lobby')
 cmd(0,'ready');time.sleep(.5);assert state(1)['phase']=='LOBBY';checks.append('one ready cannot start room')
 cmd(1,'ready');wait('all ready enters preparation',lambda:all(state(i).get('phase')=='SHOP' for i in range(2)))
 assert all(own(i)['gold']==20 for i in range(2));checks.append('independent initial economy')
 cmd(0,'buy',value=3);cmd(1,'buy',value=5)
 wait('both purchases replicate to both clients',lambda:all(roster(i)[a]['levels'][3]==1 and roster(i)[b]['levels'][5]==1 and roster(i)[a]['gold']<20 and roster(i)[b]['gold']<20 for i in range(2)))
 gold=own(0)['gold'];cmd(0,'buy',value=99);time.sleep(.4);assert own(0)['gold']==gold;checks.append('invalid product does not deduct gold')
 for i,y in [(0,-170),(1,170)]:cmd(i,'click',x=0,y=y)
 wait('independent authoritative movement',lambda:all(abs(own(i)['x'])<35 and abs(own(i)['y']-y)<35 for i,y in [(0,-170),(1,170)]),15)
 wait('remote view receives owned pawn position',lambda:all(math.dist((roster(0)[k]['x'],roster(0)[k]['y']),(roster(1)[k]['x'],roster(1)[k]['y']))<30 for k in [a,b]))
 cmd(0,'cast',value=0,x=own(1)['x'],y=own(1)['y']);wait('preparation hit pushes without damage',lambda:own(1).get('knockback',0)>0 and own(1)['health']==100,8)
 cmd(0,'ready');cmd(1,'ready');wait('both ready starts first round',lambda:all(state(i)['phase']=='FIGHT' and state(i)['round']==1 for i in range(2)))
 cmd(0,'pause');assert state(0)['menu'] and not state(0)['world_paused'] and not state(1)['menu'];checks.append('ESC local menu does not pause server or peer')
 cmd(0,'pause');time.sleep(4.3)
 for i,y in [(0,-170),(1,170)]:cmd(i,'click',x=0,y=y)
 wait('combat positioning after protection',lambda:all(abs(own(i)['x'])<35 and abs(own(i)['y']-y)<35 for i,y in [(0,-170),(1,170)]),15)
 before=own(0)['cast'];cmd(0,'aim',value=0,x=own(1)['x'],y=own(1)['y']);time.sleep(.35);assert own(0)['cast']==before and state(0)['aiming']==0;checks.append('held guide does not cast remotely')
 capture(0,'aim');cmd(0,'release',value=0);wait('released fireball damages and knocks back',lambda:own(1)['health']<100 and own(1)['knockback']>0,8)
 wait('both clients agree on combat health',lambda:abs(roster(0)[b]['health']-roster(1)[b]['health'])<.01)
 capture(0,'combat');capture(1,'combat')
 # Explicit S cancels normal windup before release; same authoritative RPC as input.
 cmd(0,'cast',value=1,x=0,y=0);wait('smite pending on both peers',lambda:all(roster(i)[a]['pending']==1 for i in range(2)),3)
 serial=own(0)['cast'];cmd(0,'stop');time.sleep(.9);assert own(0)['cast']==serial+1 and own(0)['pending']==-1;checks.append('S cancels server windup')
 # Walking deliberately into lava exercises shared death, scores and transition.
 cmd(1,'click',x=0,y=2300);wait('lava death and result replicated',lambda:all(state(i)['phase']=='RESULT' and not roster(i)[b]['alive'] for i in range(2)),30)
 wait('round economy returns to preparation',lambda:all(state(i)['phase']=='SHOP' for i in range(2)),10)
 cmd(host,'back');wait('host returns everyone to lobby',lambda:all(state(i)['phase']=='LOBBY' for i in range(2)))
 # Server remains alive and promotes the remaining human after the host leaves.
 cmd(host,'leave');wait('host migration after disconnect',lambda:state(guest).get('phase')=='LOBBY' and state(guest).get('host')==state(guest).get('self') and sum(not x['bot'] for x in roster(guest).values())==1,15)
 cmd(host,'join',address=args.address,name='Rejoined');wait('fresh reconnect fills second human seat',lambda:all(state(i).get('phase')=='LOBBY' and sum(not x['bot'] for x in roster(i).values())==2 for i in range(2)),35)
 report=dict(address=args.address,checks=checks,passed=len(checks),rendered=not args.headless,logs=str(out),limitation='software-generated owned RPC actions; not physical gamepad or WAN latency acceptance')
 (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');print(json.dumps(report,ensure_ascii=False),flush=True)
finally:
 for proc in reversed(processes):
  if proc.poll() is None:proc.terminate();proc.wait(timeout=15)
 if original is None:settings.unlink(missing_ok=True)
 else:settings.write_bytes(original)
