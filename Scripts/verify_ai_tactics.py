"""Verify the same tactical entry points used by AI, with controlled observations."""
import asyncio
import json
from pathlib import Path
import unreal

w=unreal.EditorLevelLibrary.get_game_world()
actors=unreal.GameplayStatics.get_all_actors_of_class
mode=actors(w,unreal.WarlockMode)[0]
mode.restart_match(False);mode.start_round();unreal.GameplayStatics.set_game_paused(w,True)
ps=list(actors(w,unreal.WarlockPawn))
bot=next(p for p in ps if p.get_editor_property('player_state').get_editor_property('bot'))
target=next(p for p in ps if p!=bot and not p.get_editor_property('player_state').get_editor_property('bot'))
ai=bot.get_controller();checks=[]
def get(o,k):return o.get_editor_property(k)
def put(o,k,v):o.set_editor_property(k,v)
def check(name,value):
    assert value,name
    checks.append(name)
def reset(p,pos):p.reset_for_round(unreal.Vector(*pos));put(p,'protection',0)
def prepare(distance):
    for p in ps:reset(p,(3000,3000,0))
    reset(bot,(0,0,0));reset(target,(distance,0,0));put(get(bot,'player_state'),'levels',[1,1,1,1,1,1,1,1])
    state=get(bot,'player_state')
    put(state,'families',list(range(8)));put(state,'defense_choice',0)
    for key in ['life_mastery','range_mastery','time_mastery']:put(state,key,0)
    put(state,'items',[0]*8)
    ai.reset_perception()
    # Protection prevents random attack choices while observation finishes.
    put(bot,'protection',4)
    ai.think();first=get(ai,'reaction_remaining')
    for _ in range(4):ai.think()
    put(bot,'protection',0)
    return first
def steps(n):
    for _ in range(n):bot.step_simulation(.03);ai.update_tactics(.03)

first_reaction=prepare(280);gold=get(get(bot,'player_state'),'gold')
check('AI still waits at least 0.45 seconds on initial observation',.449<=first_reaction<=.751)
check('feint starts the real cancellable Smite',ai.try_feint() and get(bot,'pending_cast')==1)
steps(14)
check('feint cancels before Smite release',get(bot,'pending_cast')==-1 and get(bot,'last_cast_slot')==255 and get(ai,'tactic')==0)
check('feint spends neither health nor cooldown',get(bot,'health')==100 and get(bot,'cooldowns')[1]==0 and get(target,'health')==100)
check('feints cannot be spammed continuously',get(ai,'feint_cooldown')>8 and not ai.try_feint())
prepare(450)
check('Blink Smite starts with a legal Blink windup',ai.try_blink_smite() and get(bot,'pending_cast')==3)
steps(3)
check('combo lands short of last observed target',abs(bot.get_actor_location().x-315)<.1 and get(ai,'tactic')==3 and get(bot,'cooldowns')[3]>15)
steps(3)
check('combo has a visible post-Blink interval',get(bot,'pending_cast')==-1)
steps(5)
check('combo follows with the normal 0.7-second Smite',get(bot,'pending_cast')==1 and get(bot,'cast_remaining')>.5)
steps(25)
check('combo uses normal area damage and self cost',abs(get(target,'health')-90)<.003 and abs(get(bot,'health')-90)<.003)
check('combo preserves Blink and Smite cooldown rules',get(bot,'cooldowns')[3]>14 and get(bot,'cooldowns')[1]>2.8)
prepare(450);ai.try_blink_smite();steps(10);reset(target,(900,0,0));steps(34)
check('committed combo can miss a target that dodges',get(target,'health')==100 and abs(get(bot,'health')-90)<.003)
prepare(450)
target.set_actor_location(unreal.Vector(1700,0,40),False,False)
ai.reset_perception();put(bot,'protection',4)
for _ in range(5):ai.think()
put(bot,'protection',0)
check('AI rejects combos outside legal setup distance',not ai.try_blink_smite())
prepare(450);ai.try_blink_smite();bot.cancel_cast();ai.update_tactics(.03)
check('interrupted Blink does not fire the follow-up',get(ai,'tactic')==0 and get(bot,'pending_cast')==-1 and get(bot,'cooldowns')[1]==0)
check('tactics do not spend shop gold',get(get(bot,'player_state'),'gold')==gold)
mode.restart_match(False)
check('new match/round clears tactical intent',get(bot,'pending_cast')==-1)
unreal.GameplayStatics.set_game_paused(w,False)
report={'passed':len(checks),'checks':checks,'scope':'controlled same-path tactical plans; natural playability is separate'}
Path(unreal.Paths.project_saved_dir(),'Validation','ai-tactics.json').write_text(json.dumps(report,indent=2),encoding='utf8')
report
