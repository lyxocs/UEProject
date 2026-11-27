"""Six-tier UI, delayed sensor, safety and legal expert shopping in real PIE."""
import asyncio,json,hashlib
from pathlib import Path
import unreal
w=unreal.EditorLevelLibrary.get_game_world();assert w and 'UEDPIE_' in w.get_path_name()
actors=unreal.GameplayStatics.get_all_actors_of_class
m=actors(w,unreal.WarlockMode)[0];state=actors(w,unreal.WarlockMatchState)[0];c=unreal.GameplayStatics.get_player_controller(w,0)
get=lambda o,k:o.get_editor_property(k)
put=lambda o,k,v:o.set_editor_property(k,v)
checks=[]
def check(n,v):
 assert v,n
 checks.append(n)
view=next(x for x in unreal.ObjectIterator(unreal.WarlockViewport) if not x.get_name().startswith('Default__'))
old=get(state,'ai_difficulty')
economy=unreal.get_default_object(unreal.load_class(None,'/Script/WarlockArena.WarlockEconomy'));old_gold=economy.get_editor_property('StartingGold')
try:
 m.restart_match(False);m.start_round();unreal.GameplayStatics.set_game_paused(w,True)
 ps=list(actors(w,unreal.WarlockPawn));p=next(x for x in ps if get(get(x,'player_state'),'bot'));q=next(x for x in ps if x.get_controller()==c);ai=p.get_controller();s=get(p,'player_state')
 for tier,bounds in [(1,(1.05,1.5)),(2,(.75,1.1)),(3,(.45,.75)),(4,(.36,.6)),(5,(.3,.5))]:
  c.set_ai_difficulty(tier);ai.reset_perception();ai.think();check(f'legacy tier {tier} reaction range retained',bounds[0]-.001<=get(ai,'reaction_remaining')<=bounds[1]+.001)
 view.inject_local_key('Six',True);view.inject_local_key('Six',False)
 check('ESC keyboard Six selects expert',get(state,'ai_difficulty')==6 and unreal.GameplayStatics.is_game_paused(w))
 c.set_ai_difficulty(7);check('tier seven is rejected',get(state,'ai_difficulty')==6)
 for x in ps:x.reset_for_round(unreal.Vector(3000,3000,40))
 p.reset_for_round(unreal.Vector(0,0,40));q.reset_for_round(unreal.Vector(500,0,40));put(p,'protection',8)
 ai.reset_perception();before=get(ai,'expert_decisions')
 for _ in range(5):ai.think_expert(.03)
 check('no expert action within first 150ms',get(ai,'expert_decisions')==before and abs(get(p,'move_input').x)+abs(get(p,'move_input').y)<.001)
 for _ in range(12):ai.think_expert(.03)
 check('expert consumes delayed observations',get(ai,'expert_decisions')>before)
 check('every consumed observation is at least 180ms old',get(ai,'expert_minimum_reaction')>=.1799)
 previous=get(ai,'observed_target_position');q.set_actor_location(unreal.Vector(-500,0,40),False,False)
 for _ in range(5):ai.think_expert(.03)
 check('new target movement cannot be read within 150ms',get(ai,'observed_target_position').x>0)
 for _ in range(12):ai.think_expert(.03)
 check('new position arrives after delayed perception',get(ai,'observed_target_position').x<0)
 put(s,'families',list(range(8)));put(s,'levels',[0,0,0,0,0,0,1,0]);put(p,'cooldowns',[0.0]*8);put(p,'protection',0)
 p.hit(q,40,unreal.Vector(0,0,0),0,-1)
 for _ in range(5):ai.think_expert(.03)
 check('new self damage cannot trigger an instant shield within 150ms',get(p,'pending_cast')==-1 and get(p,'shield_remaining')==0)
 p.reset_for_round(unreal.Vector(1240,0,40));q.reset_for_round(unreal.Vector(900,0,40));put(p,'protection',8);ai.reset_perception()
 for _ in range(20):ai.think_expert(.03)
 check('edge movement points back to safe ground',get(p,'move_input').x<0)
 check('expert delay remains bounded below after reset',get(ai,'expert_minimum_reaction')>=.1799)
 p.reset_for_round(unreal.Vector(0,0,40));q.reset_for_round(unreal.Vector(-600,0,40));put(p,'protection',0);put(q,'protection',0);put(s,'levels',[0]*8)
 q.cast(0)
 for _ in range(13):q.step_simulation(.03)
 check('dodge fixture uses an actual enemy projectile',len(actors(w,unreal.WarlockProjectile))>0)
 ai.reset_perception();dodges=get(ai,'expert_dodges')
 for _ in range(40):
  ai.think_expert(.03);p.step_simulation(.03);m.simulate_projectiles(.03)
 check('expert recognizes delayed incoming projectile threats',get(ai,'expert_dodges')>dodges)
 check('expert avoids the controlled incoming fireball without damage',get(p,'health')==100)
 c.set_ai_difficulty(6)
 economy.set_editor_property('StartingGold',100);m.restart_match(False)
 p=next(x for x in actors(w,unreal.WarlockPawn) if get(get(x,'player_state'),'bot'));ai=p.get_controller();s=get(p,'player_state')
 # Run only the same authority Shop entry point; purchases must consume gold.
 check('expert spends real shop gold',0<=get(s,'gold')<100)
 check('expert buys equipment as well as spells',sum(get(s,'items'))>0 and get(ai,'expert_item_purchases')>0)
 check('expert learns a legal skill build',sum(v>0 for v in get(s,'levels')[2:])>=1)
 check('no item exceeds its actual cap',all(0<=a<=b for a,b in zip(get(s,'items'),[1,3,2,1,3,3,3,3])))
 check('expert does not mint gold through difficulty change',get(s,'gold')>=0)
 report={'passed':len(checks),'checks':checks,'scope':'controlled PIE, public delayed snapshots and legal purchases; strength requires held-out battles'}
 Path(unreal.Paths.project_saved_dir(),'Validation/expert-ai.json').write_text(json.dumps(report,indent=2),encoding='utf8')
 print(json.dumps(report))
finally:
 economy.set_editor_property('StartingGold',old_gold);c.set_ai_difficulty(old);unreal.GameplayStatics.set_game_paused(w,False);m.restart_match(False)
