"""Authority-confirmed reward events, real amounts, streaks and lifecycle."""
import asyncio,json
from pathlib import Path
import unreal
w=unreal.EditorLevelLibrary.get_game_world();assert w and 'UEDPIE_' in w.get_path_name()
actors=unreal.GameplayStatics.get_all_actors_of_class
mode=actors(w,unreal.WarlockMode)[0];match=actors(w,unreal.WarlockMatchState)[0]
get=lambda o,k:o.get_editor_property(k)
put=lambda o,k,v:o.set_editor_property(k,v)
checks=[];path=Path(unreal.Paths.project_saved_dir())/'Validation/rewards.json'
def check(name,value):
    if not value:path.write_text(json.dumps({'passed':len(checks),'failed':name}),encoding='utf8');raise AssertionError(name)
    checks.append(name)
unreal.GameplayStatics.set_game_paused(w,True)
try:
    mode.restart_match(False);mode.start_round();ps=sorted(actors(w,unreal.WarlockPawn),key=lambda x:get(get(x,'player_state'),'bot'));p=ps[0];s=get(p,'player_state')
    for i,x in enumerate(ps):x.reset_for_round(unreal.Vector(i*220,0,0));put(x,'protection',0)
    ps[1].hit(p,1000,unreal.Vector(1,0,0),0,-1)
    rewards=list(actors(w,unreal.WarlockRewardFX));check('first blood produces one confirmed event',len(rewards)==1 and get(rewards[0],'kind')==3)
    fx=rewards[0];check('event identifies correct killer and real score',get(fx,'source')==p and get(fx,'points')==2)
    check('zero map-default bounty remains zero in floating reward',get(fx,'gold')==0 and get(s,'gold')==20)
    check('visual rune cannot collide or cause damage',get(fx,'ring').get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION)
    ps[1].hit(p,1000,unreal.Vector(1,0,0),0,-1);check('duplicate lethal hit cannot create another reward',len(actors(w,unreal.WarlockRewardFX))==1)
    ps[2].hit(p,1000,unreal.Vector(1,0,0),0,-1)
    second=next(x for x in actors(w,unreal.WarlockRewardFX) if x!=fx)
    check('second confirmed kill carries double-kill streak',get(second,'kind')==0 and get(second,'streak')==2)
    ps[3].hit(p,1000,unreal.Vector(1,0,0),0,-1)
    unreal.GameplayStatics.set_game_paused(w,False);await asyncio.sleep(.12);unreal.GameplayStatics.set_game_paused(w,True)
    wins=[x for x in actors(w,unreal.WarlockRewardFX) if get(x,'kind')==2]
    check('round victory event follows authoritative round result',get(match,'phase')=='RESULT' and len(wins)==1 and get(wins[0],'source')==p)
    check('victory popup shows actual reward rather than an invented gold amount',get(wins[0],'gold')==0)
    mode.restart_match(False);check('restart removes all pending celebration actors',len(actors(w,unreal.WarlockRewardFX))==0)
    check('new round clears first-blood state',not get(match,'first_blood'))
    path.write_text(json.dumps({'passed':len(checks),'checks':checks,'scope':'PIE event, collision and amount validation; auditory/visual taste needs play feedback'},indent=2),encoding='utf8')
    print(json.dumps({'passed':len(checks),'report':str(path)}))
finally:mode.restart_match(False);unreal.GameplayStatics.set_game_paused(w,False)
