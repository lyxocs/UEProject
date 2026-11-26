"""Pause-menu difficulty routing and use of the shipped learned policy in PIE."""
import asyncio
import json
from pathlib import Path
import unreal

w=unreal.EditorLevelLibrary.get_game_world()
assert w and 'UEDPIE_' in w.get_path_name()
actors=unreal.GameplayStatics.get_all_actors_of_class
mode=actors(w,unreal.WarlockMode)[0];match=actors(w,unreal.WarlockMatchState)[0]
c=unreal.GameplayStatics.get_player_controller(w,0)
view=next(v for v in unreal.ObjectIterator(unreal.WarlockViewport) if not v.get_name().startswith('Default__'))
checks=[]
def get(o,k):return o.get_editor_property(k)
def put(o,k,v):o.set_editor_property(k,v)
def check(name,value):
    assert value,name
    checks.append(name)
async def key(name):
    view.inject_local_key(name,True);await asyncio.sleep(.025);view.inject_local_key(name,False);await asyncio.sleep(.025)

old=get(match,'ai_difficulty')
try:
    mode.restart_match(False);c.add_mixed_player();await asyncio.sleep(.1);mode.start_round()
    ps=list(actors(w,unreal.WarlockPawn));bot=next(p for p in ps if get(get(p,'player_state'),'bot'));ai=bot.get_controller()
    before=get(match,'ai_difficulty');c.set_ai_difficulty(1)
    check('difficulty cannot change outside pause',get(match,'ai_difficulty')==before)
    c.toggle_pause();await asyncio.sleep(.04)
    snapshot=[(get(p,'health'),get(get(p,'player_state'),'gold'),list(get(p,'cooldowns'))) for p in ps]
    for tier,name in enumerate(['One','Two','Three','Four','Five'],1):
        await key(name)
        check(f'paused raw keyboard selects tier {tier}',get(match,'ai_difficulty')==tier)
    await key('One')
    view.inject_pad_key('Gamepad_DPad_Left',0,True);view.inject_pad_key('Gamepad_DPad_Left',0,False)
    check('paused pad D-pad adjusts difficulty',get(match,'ai_difficulty')==2)
    c.set_ai_difficulty(0);c.set_ai_difficulty(6)
    check('invalid tiers are rejected',get(match,'ai_difficulty')==2)
    check('difficulty does not grant stats, gold or cooldowns',snapshot==[(get(p,'health'),get(get(p,'player_state'),'gold'),list(get(p,'cooldowns'))) for p in ps])
    check('world remains paused after difficulty selection',unreal.GameplayStatics.is_game_paused(w))
    casts=[get(p,'pending_cast') for p in ps]
    await key('LeftMouseButton')
    check('paused UI click does not queue a combat spell',unreal.GameplayStatics.is_game_paused(w) and casts==[get(p,'pending_cast') for p in ps])
    c.set_ai_difficulty(3);ai.reset_perception();ai.think()
    check('standard reaction remains in existing range',.449<=get(ai,'reaction_remaining')<=.751)
    c.set_ai_difficulty(1);ai.reset_perception();ai.think()
    check('entry reaction is deliberately slower',1.049<=get(ai,'reaction_remaining')<=1.501)
    c.set_ai_difficulty(5);ai.reset_perception();ai.think()
    check('master still has a nonzero reaction window',.299<=get(ai,'reaction_remaining')<=.501)
    check('trained policy is actually loaded',unreal.WarlockController.is_trained_ai_ready())
    before=get(ai,'policy_decisions')
    for i,p in enumerate(ps):
        p.reset_for_round(unreal.Vector(i*400,0,40));put(p,'protection',0)
    ai.reset_perception()
    # Fixed control steps exercise runtime inference even while world is paused.
    for _ in range(90):
        for p in ps:p.step_simulation(.03)
        ai.think();ai.update_tactics(.03)
    check('AI executes learned policy at runtime',get(ai,'policy_decisions')>before)
    check('reported policy action is inside legal action space',0<=get(ai,'last_policy_action')<=10)
finally:
    c.set_ai_difficulty(old)
    unreal.GameplayStatics.set_game_paused(w,False)
    mode.restart_match(False)
report={'passed':len(checks),'checks':checks,'physical_gamepads':'software viewport events only'}
Path(unreal.Paths.project_saved_dir(),'Validation','ai-difficulty.json').write_text(json.dumps(report,indent=2),encoding='utf8')
report
