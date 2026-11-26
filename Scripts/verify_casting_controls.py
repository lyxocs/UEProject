"""Default feel preset: anticipation, cancellation, perception, viewport routing, pause."""
import asyncio
import json
from pathlib import Path
import unreal

w=unreal.EditorLevelLibrary.get_game_world()
assert w and 'UEDPIE_' in w.get_path_name()
actors=unreal.GameplayStatics.get_all_actors_of_class
mode=actors(w,unreal.WarlockMode)[0];match=actors(w,unreal.WarlockMatchState)[0]
c1=unreal.GameplayStatics.get_player_controller(w,0)
c1.add_keyboard_mouse_player()
await asyncio.sleep(.1)
c2=unreal.GameplayStatics.get_player_controller(w,1)
ps=list(actors(w,unreal.WarlockPawn))
p1=next(p for p in ps if p.get_editor_property('player_state')==c1.get_editor_property('player_state'))
p2=next(p for p in ps if p.get_editor_property('player_state')==c2.get_editor_property('player_state'))
view=next(v for v in unreal.ObjectIterator(unreal.WarlockViewport) if not v.get_name().startswith("Default__"))
checks=[]
def get(o,k):return o.get_editor_property(k)
def put(o,k,v):o.set_editor_property(k,v)
def near(a,b,t=.003):return abs(a-b)<t
def check(name,value):
    assert value,name
    checks.append(name)
def reset(p,xyz):
    p.reset_for_round(unreal.Vector(*xyz));put(p,'protection',0)
def step(p,n):
    for _ in range(n):p.step_simulation(.03)
def clean():
    for a in actors(w,unreal.WarlockProjectile):a.destroy_actor()
async def tick(t=.1):
    unreal.GameplayStatics.set_game_paused(w,False);await asyncio.sleep(t);unreal.GameplayStatics.set_game_paused(w,True)
async def key(name,t=.08):
    view.inject_local_key(name,True);await tick(t);view.inject_local_key(name,False)

unreal.GameplayStatics.set_game_paused(w,True)
mode.restart_match(False);mode.start_round()
for p in ps:reset(p,(3000,3000,0))
reset(p1,(-100,0,0));reset(p2,(100,0,0))
check('custom viewport is active',isinstance(view,unreal.WarlockViewport))
check('keyboard and mouse are separate local roles',get(c1,'local_control')==1 and get(c2,'local_control')==2)
check('Smite begins anticipation',p1.cast(1) and get(p1,'pending_cast')==1)
step(p1,20)
check('Smite has no damage cost or cooldown at 0.60s',near(get(p1,'health'),100) and near(get(p2,'health'),100) and get(p1,'cooldowns')[1]==0)
p1.cancel_cast();step(p1,10)
check('cancel before release leaves no delayed effect or cooldown',get(p1,'pending_cast')==-1 and near(get(p1,'health'),100) and near(get(p2,'health'),100) and get(p1,'cooldowns')[1]==0)
p1.cast(1);step(p1,23)
check('Smite has not released at 0.69s',get(p1,'pending_cast')==1 and near(get(p2,'health'),100))
step(p1,1)
check('Smite releases once on first fixed step after 0.70s',get(p1,'pending_cast')==-1 and near(get(p1,'health'),90) and near(get(p2,'health'),90) and near(get(p2,'knockback'),10) and near(get(p1,'cooldowns')[1],3))
step(p1,3)
check('Smite does not repeat its damage',near(get(p2,'health'),90))
reset(p1,(0,0,0));reset(p2,(0,900,0));clean()
p1.cast(0)
check('one pending cast blocks overlapping casts',not p1.cast(1) and len(actors(w,unreal.WarlockProjectile))==0)
p1.cancel_cast();check('cancel clears animation intent',get(p1,'last_cast_slot')==255)
p1.cast(0);step(p1,12)
check('fireball releases after visible 0.36s windup',len(actors(w,unreal.WarlockProjectile))==1)
shot=actors(w,unreal.WarlockProjectile)[0]
check('fireball creates its bounded flame and ember pool',len(get(shot,'fire_particles'))==36)
for _ in range(10):mode.simulate_projectiles(.03)
check('default fireball travels at 600 units per second',near(shot.get_actor_location().x,180,.05))
for _ in range(45):mode.simulate_projectiles(.03)
check('slower preset retains approximately original 1000 range',970<shot.get_actor_location().x<=1000)
mode.simulate_projectiles(.03)
check('slower projectile expires at its extended lifetime',len(actors(w,unreal.WarlockProjectile))==0)
reset(p1,(0,0,0));p1.cast(0);p1.hit(p2,1000,unreal.Vector(1,0,0),0);step(p1,10)
check('death cancels pending spell',not get(p1,'alive') and get(p1,'pending_cast')==-1 and len(actors(w,unreal.WarlockProjectile))==0)

# Human blink must reset the bot's observation delay before any new counter-cast.
reset(p1,(100,0,0));reset(p2,(3000,3000,0))
bot=next(p for p in ps if get(get(p,'player_state'),'bot'))
reset(bot,(0,0,0));ai=bot.get_controller();ai.reset_perception();serial=get(bot,'cast_serial')
ai.think()
check('bot starts with 0.45-0.75 second observation delay',.449<get(ai,'reaction_remaining')<.751 and get(bot,'cast_serial')==serial)
ai.think();ai.think()
check('bot cannot counter within its first 0.44 seconds',get(bot,'cast_serial')==serial)
levels=list(get(get(p1,'player_state'),'levels'));levels[3]=1;put(get(p1,'player_state'),'levels',levels)
p1.cast(3);step(p1,10);serial=get(bot,'cast_serial');ai.think()
check('observed Blink restarts reaction delay without instant counter',get(ai,'reaction_remaining')>=.449 and get(bot,'cast_serial')==serial)
for _ in range(6):ai.think()
check('AI resumes decisions after reaction delay',get(ai,'observed_target_position')==p1.get_actor_location())

# Real viewport key path, rather than directly calling gameplay actions.
mode.restart_match(False);c1.add_keyboard_mouse_player();mode.start_round()
for c in [c1,c2]:put(c,'shop_open',False);put(c,'help_open',False)
before1=p1.get_actor_location();before2=p2.get_actor_location()
await key('Right',.2);await tick(.04)
check('viewport keyboard moves only P1',p1.get_actor_location().x>before1.x+10 and (p2.get_actor_location()-before2).length()<.1)
view.inject_local_key('G',True);await tick(.06);view.inject_local_key('G',False)
check('keyboard spell starts only on P1',get(p1,'pending_cast')==0 and get(p2,'pending_cast')==-1)
await key('S',.04)
check('S cancels P1 anticipation without starting cooldown',get(p1,'pending_cast')==-1 and get(p1,'cooldowns')[0]==0)
# Pause now deliberately consumes UI clicks; battle clicks must arrive unpaused.
unreal.GameplayStatics.set_game_paused(w,False)
view.inject_local_key('LeftMouseButton',True);await asyncio.sleep(.06);view.inject_local_key('LeftMouseButton',False)
unreal.GameplayStatics.set_game_paused(w,True)
check('viewport mouse starts only P2 spell',get(p2,'pending_cast')==0 and get(p1,'pending_cast')==-1)
await key('MiddleMouseButton',.04)
check('middle button cancels only P2',get(p2,'pending_cast')==-1 and get(p2,'cooldowns')[0]==0)
mode.restart_match(False);c1.add_local_player()
check('two pads retain one controller per local player',get(c1,'local_control')==get(c2,'local_control')==3 and c1.local_input()!=c2.local_input())
check('right stick is not mapped to aiming',not any(str(m.get_editor_property('key'))=='Gamepad_Right2D' for m in get(next(m for m in unreal.ObjectIterator(unreal.InputMappingContext) if m.get_outer()==c1),'mappings')))
mode.start_round();before1=p1.get_actor_location();before2=p2.get_actor_location()
def pad_move(dt):
    for c,axis in [(c1,(1,0,0)),(c2,(0,1,0))]:
        action=next(a for a in get(c,'actions') if a.get_name()=='Move')
        c.local_input().inject_input_vector_for_action(action,unreal.Vector(*axis),[],[])
    return True
handle=unreal.register_ticker_callback(pad_move)
try:await tick(.2)
finally:unreal.unregister_ticker_callback(handle)
await tick(.04)
check('two pad sticks move their own characters independently',p1.get_actor_location().x>before1.x+15 and p2.get_actor_location().y<before2.y-15 and abs(p1.get_actor_location().y-before1.y)<.1 and abs(p2.get_actor_location().x-before2.x)<.1)
for c in [c1,c2]:
    action=next(a for a in get(c,'actions') if a.get_name()=='Cast0')
    c.local_input().inject_input_vector_for_action(action,unreal.Vector(1,0,0),[],[])
await tick(.05)
check('pad skills start along each retained facing',get(p1,'pending_cast')==get(p2,'pending_cast')==0 and abs(p1.get_actor_rotation().yaw)<1 and abs(p2.get_actor_rotation().yaw+90)<1)
mode.restart_match(False)
c1.add_keyboard_mouse_player()
unreal.GameplayStatics.set_game_paused(w,False)
view.inject_local_key('Escape',True);view.inject_local_key('Escape',False)
remaining=get(match,'remaining');await asyncio.sleep(.2)
check('Escape pauses the world and shop clock',unreal.GameplayStatics.is_game_paused(w) and near(get(match,'remaining'),remaining))
view.inject_local_key('Escape',True);view.inject_local_key('Escape',False);await asyncio.sleep(.15)
check('Escape resumes the same match and clock',not unreal.GameplayStatics.is_game_paused(w) and get(match,'remaining')<remaining)
unreal.GameplayStatics.set_game_paused(w,True);mode.start_round();reset(p1,(0,0,0));p1.cast(1)
unreal.GameplayStatics.set_game_paused(w,False)
view.inject_local_key('Escape',True);view.inject_local_key('Escape',False)
remaining=get(p1,'cast_remaining');await asyncio.sleep(.2)
check('pause freezes anticipation too',near(get(p1,'cast_remaining'),remaining) and get(p1,'cooldowns')[1]==0)
view.inject_local_key('Escape',True);view.inject_local_key('Escape',False)
await asyncio.sleep(.8)
check('resumed anticipation completes normally',get(p1,'pending_cast')==-1 and get(p1,'cooldowns')[1]>2)
mode.restart_match(False)
report={'passed':len(checks),'checks':checks,'preset':'default windups, fireball 0.60, spray/chain 0.78','physical_gamepads':'not exercised','effects':'separate rendered review'}
Path(unreal.Paths.project_saved_dir(),'Validation','casting-controls.json').write_text(json.dumps(report,indent=2),encoding='utf8')
report
