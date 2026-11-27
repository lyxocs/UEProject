"""Default eight-skill anticipation, actual S input, and evaluated staff lift."""
import asyncio
import json
import math
from pathlib import Path
import unreal

w=unreal.EditorLevelLibrary.get_game_world();assert w and 'UEDPIE_' in w.get_path_name()
actors=unreal.GameplayStatics.get_all_actors_of_class
mode=actors(w,unreal.WarlockMode)[0];c=unreal.GameplayStatics.get_player_controller(w,0)
mode.restart_match(False);c.add_mixed_player();await asyncio.sleep(.1);mode.start_round()
view=next(v for v in unreal.ObjectIterator(unreal.WarlockViewport) if not v.get_name().startswith('Default__'))
p=next(a for a in actors(w,unreal.WarlockPawn) if a.get_editor_property('player_state')==c.get_editor_property('player_state'));state=p.get_editor_property('player_state');avatar=p.get_editor_property('avatar')
checks=[];lifts={};times=[.36,.7,.42,.09,.45,.12,.33,.39]
def get(o,k):return o.get_editor_property(k)
def put(o,k,v):o.set_editor_property(k,v)
def check(name,value):
    assert value,name
    checks.append(name)
def reset():
    unreal.GameplayStatics.set_game_paused(w,True)
    for a in actors(w,unreal.WarlockProjectile)+actors(w,unreal.WarlockSpellFX):a.destroy_actor()
    for i,a in enumerate(actors(w,unreal.WarlockPawn)):
        a.reset_for_round(unreal.Vector(0,0,0) if a==p else unreal.Vector(3000+i*500,3000,0));put(a,'protection',0 if a==p else 1000)
    put(state,'levels',[1]*8);put(state,'items',[0]*8)
    for k in ['life_mastery','range_mastery','time_mastery']:put(state,k,0)
    put(c,'shop_open',False);put(c,'help_open',False)
async def advance(t):
    unreal.GameplayStatics.set_game_paused(w,False);await asyncio.sleep(t);unreal.GameplayStatics.set_game_paused(w,True)
async def stop():
    unreal.GameplayStatics.set_game_paused(w,False);view.inject_local_key('S',True);await asyncio.sleep(.035);view.inject_local_key('S',False);unreal.GameplayStatics.set_game_paused(w,True)

for slot,duration in enumerate(times):
    reset();gold=get(state,'gold');pos=p.get_actor_location()
    check(f'{slot}: starts a real anticipation',p.cast(slot) and get(p,'pending_cast')==slot and abs(get(p,'cast_duration')-duration)<.001)
    for _ in range(int(duration/.03*.45)):p.step_simulation(.03)
    check(f'{slot}: no effect or cost before release',get(p,'health')==100 and not any(get(p,'cooldowns')) and get(p,'shield_remaining')==0 and p.get_actor_location()==pos and not actors(w,unreal.WarlockProjectile))
    await stop()
    check(f'{slot}: raw S cancels pending cast',get(p,'pending_cast')==-1 and get(p,'last_cast_slot')==255)
    for _ in range(40):p.step_simulation(.03);mode.simulate_projectiles(.03)
    check(f'{slot}: cancelled spell never appears later',get(p,'health')==100 and get(p,'shield_remaining')==0 and not any(get(p,'cooldowns')) and not actors(w,unreal.WarlockProjectile) and not any(get(e,'style')!=7 for e in actors(w,unreal.WarlockSpellFX)))
    check(f'{slot}: cancellation preserves economy',get(state,'gold')==gold)
    reset();await advance(.4);idle=avatar.get_socket_location('16_Staff').z
    # Slow only the pose inspection so an overloaded editor still evaluates
    # enough skeletal frames; cancellation/input tests above use normal time.
    speed=.1
    unreal.GameplayStatics.set_global_time_dilation(w,speed)
    p.cast(slot);await advance(duration*(.5 if duration<.15 else .75)/speed)
    unreal.GameplayStatics.set_global_time_dilation(w,1)
    check(f'{slot}: raised pose remains before effect',get(p,'pending_cast')==slot)
    lift=avatar.get_socket_location('16_Staff').z-idle;lifts[str(slot)]=round(lift,3)
    check(f'{slot}: evaluated quick or full staff gesture is visible',lift>(5 if slot in [3,5] else 20))
    p.cancel_cast();await advance(.6)
    check(f'{slot}: cancellation fades back to idle',get(avatar.get_anim_instance(),'casting_blend')<.05)

reset();p.cast(0);old=next(e for e in actors(w,unreal.WarlockSpellFX) if get(e,'style')==7);serial=get(old,'source_cast_serial');p.cancel_cast();p.cast(0)
check('same-skill recast gets a new cue generation',get(p,'cast_serial')!=serial and len(actors(w,unreal.WarlockSpellFX))==2)
await advance(.22)
check('old anticipation cue expires during immediate recast',old not in list(actors(w,unreal.WarlockSpellFX)))
p.cancel_cast();reset();p.cast(0)
for _ in range(12):p.step_simulation(.03)
check('release occurs on the exact 0.36 fixed-step boundary',get(p,'pending_cast')==-1 and len(actors(w,unreal.WarlockProjectile))==1)
await stop()
check('S after release does not undo an existing fireball',len(actors(w,unreal.WarlockProjectile))==1 and get(p,'cooldowns')[0]>0)
mode.restart_match(False);unreal.GameplayStatics.set_game_paused(w,False)
report={'passed':len(checks),'checks':checks,'staff_lift_cm':lifts,'windups':times,'scope':'all eight currently implemented basic skills; chain detachment is an immediate utility, not a fresh cast'}
Path(unreal.Paths.project_saved_dir(),'Validation','all-skill-anticipation.json').write_text(json.dumps(report,indent=2),encoding='utf8')
report
