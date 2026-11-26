"""Effect lifecycle and gameplay separation on the default visual preset."""
import asyncio
import json
from pathlib import Path
import unreal

w=unreal.EditorLevelLibrary.get_game_world()
assert w and 'UEDPIE_' in w.get_path_name()
actors=unreal.GameplayStatics.get_all_actors_of_class
mode=actors(w,unreal.WarlockMode)[0]
c=unreal.GameplayStatics.get_player_controller(w,0)
ps=list(actors(w,unreal.WarlockPawn))
p=next(a for a in ps if a.get_editor_property('player_state')==c.get_editor_property('player_state'))
target=next(a for a in ps if a!=p)
unreal.GameplayStatics.set_game_paused(w,True)
mode.restart_match(False);mode.start_round()
s=p.get_editor_property('player_state');s.set_editor_property('levels',[1]*8)
checks=[]
def get(o,k):return o.get_editor_property(k)
def check(name,value):
    assert value,name
    checks.append(name)
def reset(a,xyz):
    a.reset_for_round(unreal.Vector(*xyz));a.set_editor_property('protection',0)
def step(n):
    for _ in range(n):p.step_simulation(.03)
def effects(style):return [a for a in actors(w,unreal.WarlockSpellFX) if get(a,'style')==style]
for a in ps:reset(a,(3000,3000,0))
reset(p,(-300,0,0));reset(target,(100,0,0))
gold=get(s,'gold');points=get(s,'points')
p.cast(0)
check('anticipation creates skill-colored effect event',any(get(e,'slot')==0 and get(e,'source')==p for e in effects(7)))
step(6)
shot=actors(w,unreal.WarlockProjectile)[0]
check('solid fireball sphere is hidden',not get(shot,'mesh').is_visible())
check('fireball has six live flames plus bounded particle trail',len(get(shot,'fire_particles'))==36)
reset(target,(100,54,0));mode.simulate_projectiles(.8)
check('larger visuals retain original near-edge collision',abs(get(target,'health')-93)<.003)
reset(p,(-300,0,0));reset(target,(100,56,0));p.cast(0);step(6);mode.simulate_projectiles(.8)
check('visual flame halo does not enlarge hit detection',abs(get(target,'health')-100)<.003)
for a in actors(w,unreal.WarlockProjectile):a.destroy_actor()
reset(p,(0,0,0));reset(target,(120,0,0));p.cast(1);step(24)
check('Smite produces shockwave ring and radial particles',len(effects(0))==1 and len(get(effects(0)[0],'particles'))==48)
check('Smite damage still resolves independently of particles',abs(get(target,'health')-90)<.003)
reset(p,(-300,0,0));reset(target,(300,0,0));p.cast(2);step(9)
check('lightning creates a branched arc rather than repeated discs',len(effects(1))==1 and len(get(effects(1)[0],'beams'))==36)
check('lightning damage remains seven at level one',abs(get(target,'health')-93)<.003)
reset(p,(-500,0,0));p.cast(3);step(3)
check('Blink creates three posed silhouettes',len(effects(2))==1 and len(get(effects(2)[0],'echoes'))==3)
check('Blink still moves to legal target',abs(p.get_actor_location().x)<.01)
reset(p,(-300,0,0));p.cast(4);step(10)
check('spray begins only after its windup',len(actors(w,unreal.WarlockProjectile))==0)
step(4)
check('spray uses smaller flame-particle projectiles',any(len(get(a,'fire_particles'))==18 for a in actors(w,unreal.WarlockProjectile)))
for a in actors(w,unreal.WarlockProjectile):a.destroy_actor()
reset(p,(-300,0,0));p.cast(5);step(15)
check('meteor marks impact area and descends with flame particles',len(effects(4))==1 and len(get(effects(4)[0],'particles'))==24)
reset(p,(-300,0,0));p.cast(6);step(6)
check('reflection keeps its duration and adds an arcane activation wave',get(p,'shield_remaining')>2.7 and len(effects(5))>0)
reset(p,(-300,0,0));reset(target,(300,0,0));p.cast(7);step(8)
mode.simulate_projectiles(.8)
unreal.GameplayStatics.set_game_paused(w,False);await asyncio.sleep(.06);unreal.GameplayStatics.set_game_paused(w,True)
check('attached chain renders a persistent energy tether',len(effects(6))==1 and len(get(effects(6)[0],'beams'))==20)
check('visual events do not charge gold or award points',get(s,'gold')==gold and get(s,'points')==points)
mode.restart_match(False)
check('restart removes every transient effect event',len(actors(w,unreal.WarlockSpellFX))==0)
avatar=get(p,'avatar')
check('polished source-derived mesh is active','/PolishedWarlock/' in avatar.get_skeletal_mesh_asset().get_path_name())
check('original 51-bone animation rig remains intact',avatar.get_num_bones()==51)
check('main BloodElf cloak now uses cloth material',any('PBR_04_' in str(m.get_editor_property('material_slot_name')) and 'Cloth' in avatar.get_material(i).get_base_material().get_name() for i,m in enumerate(avatar.get_skeletal_mesh_asset().get_editor_property('materials'))))
unreal.GameplayStatics.set_game_paused(w,False)
report={'passed':len(checks),'checks':checks,'visual_acceptance':'separate actual rendered package captures','physical_gamepads':'not exercised'}
Path(unreal.Paths.project_saved_dir(),'Validation','spell-polish.json').write_text(json.dumps(report,indent=2),encoding='utf8')
report
