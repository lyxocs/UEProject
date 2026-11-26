"""Ready flow and raw viewport device routing, not subsystem-only injection."""
import asyncio
import json
from pathlib import Path
import unreal

w=unreal.EditorLevelLibrary.get_game_world()
assert w and 'UEDPIE_' in w.get_path_name()
actors=unreal.GameplayStatics.get_all_actors_of_class
mode=actors(w,unreal.WarlockMode)[0];match=actors(w,unreal.WarlockMatchState)[0]
c1=unreal.GameplayStatics.get_player_controller(w,0)
mode.restart_match(False);c1.add_mixed_player();await asyncio.sleep(.15)
c2=unreal.GameplayStatics.get_player_controller(w,1)
view=next(v for v in unreal.ObjectIterator(unreal.WarlockViewport) if not v.get_name().startswith('Default__'))
ps=list(actors(w,unreal.WarlockPawn))
p1=next(p for p in ps if p.get_editor_property('player_state')==c1.get_editor_property('player_state'))
p2=next(p for p in ps if p.get_editor_property('player_state')==c2.get_editor_property('player_state'))
s1=p1.get_editor_property('player_state');s2=p2.get_editor_property('player_state')
checks=[];path=Path(unreal.Paths.project_saved_dir(),'Validation','ready-routing.json')
def get(o,k):return o.get_editor_property(k)
def put(o,k,v):o.set_editor_property(k,v)
def check(name,value):
    if not value:
        path.write_text(json.dumps({'passed':len(checks),'checks':checks,'failed':name},indent=2),encoding='utf8')
        raise AssertionError(name)
    checks.append(name)
def clear_ui():
    for c in [c1,c2]:put(c,'shop_open',False);put(c,'help_open',False)
async def key(name,t=.06):
    view.inject_local_key(name,True);await asyncio.sleep(t);view.inject_local_key(name,False);await asyncio.sleep(.03)
async def axes(values,seconds=.2,keyboard=False):
    def hold(dt):
        for pad,x,y in values:view.inject_pad_axis('Gamepad_LeftX',pad,x);view.inject_pad_axis('Gamepad_LeftY',pad,y)
        return True
    if keyboard:view.inject_local_key('Right',True)
    handle=unreal.register_ticker_callback(hold)
    try:await asyncio.sleep(seconds)
    finally:
        unreal.unregister_ticker_callback(handle)
        if keyboard:view.inject_local_key('Right',False)
        for pad,x,y in values:view.inject_pad_axis('Gamepad_LeftX',pad,0);view.inject_pad_axis('Gamepad_LeftY',pad,0)
    await asyncio.sleep(.05)

unreal.GameplayStatics.set_game_paused(w,False);clear_ui();mode.restart_match(False)
check('both human seats initially unready',not get(s1,'ready') and not get(s2,'ready'))
check('every AI seat is ready immediately',all(get(get(p,'player_state'),'ready') for p in ps if get(get(p,'player_state'),'bot')))
c1.toggle_ready();check('one ready player waits for the other',get(s1,'ready') and not get(s2,'ready') and get(match,'phase')=='SHOP')
c1.toggle_ready();check('ready can be cancelled',not get(s1,'ready'))
c1.toggle_ready();mode.buy(p1,8)
check('a successful build change cancels human readiness',not get(s1,'ready') and get(s1,'life_mastery')==1)
c1.toggle_ready();c2.toggle_ready()
check('last ready immediately starts one round',get(match,'phase')=='FIGHT' and get(match,'round')==1)
c1.toggle_ready();c2.toggle_ready();mode.start_round()
check('duplicate ready or start cannot advance round twice',get(match,'round')==1)
for p in ps:put(p,'protection',0)
for p in ps:
    if p!=p1:p.hit(p1,1000,unreal.Vector(1,0,0),0)
await asyncio.sleep(.12);put(match,'remaining',.01);await asyncio.sleep(.12)
check('next shop resets human readiness and keeps AI ready',get(match,'phase')=='SHOP' and not get(s1,'ready') and not get(s2,'ready') and all(get(get(p,'player_state'),'ready') for p in ps if get(get(p,'player_state'),'bot')))
put(match,'remaining',.01);await asyncio.sleep(.12)
check('existing preparation timeout still starts combat',get(match,'phase')=='FIGHT')

mode.restart_match(False);c1.add_mixed_player();clear_ui();await asyncio.sleep(.08)
check('mixed mode explicitly isolates keyboard mouse and pad',get(c1,'local_control')==4 and get(c2,'local_control')==3)
await key('T')
check('keyboard skill T does not accidentally ready the player',not get(s1,'ready') and not get(s2,'ready'))
view.inject_pad_key('Gamepad_FaceButton_Top',0,True);await asyncio.sleep(.04);view.inject_pad_key('Gamepad_FaceButton_Top',0,False)
check('pad slot zero readies only P2 in mixed mode',get(s2,'ready') and not get(s1,'ready') and get(match,'phase')=='SHOP')
await key('SpaceBar')
check('keyboard space completes readiness without casting spray',get(match,'phase')=='FIGHT' and get(p1,'cooldowns')[4]==get(p2,'cooldowns')[4]==0)
before1=p1.get_actor_location();before2=p2.get_actor_location()
await axes([(0,0,.8)],keyboard=True)
check('keyboard and first pad simultaneously move different characters',p1.get_actor_location().x>before1.x+8 and abs(p1.get_actor_location().y-before1.y)<.1 and p2.get_actor_location().y<before2.y-8 and abs(p2.get_actor_location().x-before2.x)<.1)
view.inject_pad_key('Gamepad_RightTrigger',0,True);await asyncio.sleep(.05);view.inject_pad_key('Gamepad_RightTrigger',0,False)
check('first pad attack reaches only P2',get(p2,'pending_cast')==0 and get(p1,'pending_cast')==-1)
p2.cancel_cast();await key('G',.04)
check('keyboard attack reaches only P1',get(p1,'pending_cast')==0 and get(p2,'pending_cast')==-1)
p1.cancel_cast();before1=p1.get_actor_location();before2=p2.get_actor_location();await axes([(1,.8,0)],.15)
check('unassigned second pad cannot hijack mixed-mode P1',(p1.get_actor_location()-before1).length()<.1 and (p2.get_actor_location()-before2).length()<.1)

mode.restart_match(False);c1.add_local_player();clear_ui();await asyncio.sleep(.08);mode.start_round()
before1=p1.get_actor_location();before2=p2.get_actor_location();await axes([(0,.8,0),(1,0,.8)],keyboard=True)
check('two raw pad slots move separate characters',p1.get_actor_location().x>before1.x+8 and p2.get_actor_location().y<before2.y-8 and abs(p1.get_actor_location().y-before1.y)<.1 and abs(p2.get_actor_location().x-before2.x)<.1)
before1=p1.get_actor_location();before2=p2.get_actor_location();await key('Right',.15)
check('keyboard movement is blocked in two-pad mode',(p1.get_actor_location()-before1).length()<.1 and (p2.get_actor_location()-before2).length()<.1)
for pad,p,other in [(0,p1,p2),(1,p2,p1)]:
    view.inject_pad_key('Gamepad_RightTrigger',pad,True);await asyncio.sleep(.05);view.inject_pad_key('Gamepad_RightTrigger',pad,False)
    check(f'pad {pad+1} attack stays with its assigned character',get(p,'pending_cast')==0 and get(other,'pending_cast')==-1);p.cancel_cast()
view.inject_pad_axis('Gamepad_LeftX',1,.8);await asyncio.sleep(.04)
mode.restart_match(False);c1.add_mixed_player();await asyncio.sleep(.08);mode.start_round();before1=p1.get_actor_location();before2=p2.get_actor_location();await asyncio.sleep(.15)
check('switching modes flushes held input and cached motion',(p1.get_actor_location()-before1).length()<.1 and (p2.get_actor_location()-before2).length()<.1)
mode.restart_match(False);c1.add_keyboard_mouse_player();await asyncio.sleep(.08);mode.start_round();before1=p1.get_actor_location();before2=p2.get_actor_location();await axes([(0,.8,.8),(1,.8,.8)])
check('pads cannot control either keyboard/mouse-only seat',(p1.get_actor_location()-before1).length()<.1 and (p2.get_actor_location()-before2).length()<.1)
check('mode switches preserve four seats and two humans',len(actors(w,unreal.WarlockPawn))==4 and unreal.GameplayStatics.get_player_controller(w,1)==c2)
mode.restart_match(False)
report={'passed':len(checks),'checks':checks,'path':'raw viewport keyboard and numbered gamepad key/axis events','physical_gamepads':'software events; real hardware not exercised'}
path.write_text(json.dumps(report,indent=2),encoding='utf8')
report
