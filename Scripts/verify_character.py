"""Editor MCP: verify real imported skeleton, poses and cosmetic state in PIE."""
import asyncio
import json
from pathlib import Path
import unreal

world=unreal.EditorLevelLibrary.get_game_world()
assert world and 'UEDPIE_' in world.get_path_name()
c=unreal.GameplayStatics.get_player_controller(world,0)
mode=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.WarlockMode)[0]
mode.restart_match(False);c.add_mixed_player();c.set_editor_property('shop_open',False);c.set_editor_property('help_open',False)
view=next(v for v in unreal.ObjectIterator(unreal.WarlockViewport) if not v.get_name().startswith('Default__'))
s=c.get_editor_property('player_state')
p=next(p for p in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.WarlockPawn) if p.get_editor_property('player_state')==s)
avatar=p.get_editor_property('avatar')
checks=[]
def check(name,value):
    assert value,name
    checks.append(name)
def pose(bone):
    transform=avatar.get_socket_transform(bone,unreal.RelativeTransformSpace.RTS_COMPONENT)
    rotation=transform.get_editor_property('rotation');translation=transform.get_editor_property('translation')
    return [rotation.get_editor_property(k) for k in ['x','y','z','w']]+[translation.get_editor_property(k) for k in ['x','y','z']]
check('source skeleton has 51 bones',avatar.get_num_bones()==51)
check('refined garment mesh is active','/PolishedWarlock/' in avatar.get_skeletal_mesh_asset().get_path_name())
idle_path=avatar.get_skeletal_mesh_asset().get_path_name().rsplit('/',1)[0]+'/WarlockStand.WarlockStand'
check('imported animation and refined mesh use the same skeleton',unreal.load_asset(idle_path).get_editor_property('skeleton')==avatar.get_skeletal_mesh_asset().get_editor_property('skeleton'))
names=[str(avatar.get_bone_name(i)) for i in range(avatar.get_num_bones())]
check('chest and head attachment names resolve','36_Bone_Chest' in names and '37_Bone_Head' in names)
check('native animation instance is active','WarlockAnimation' in avatar.get_anim_instance().get_class().get_name())
unreal.GameplayStatics.set_game_paused(world,False)
view.inject_local_key('Right',True)
await asyncio.sleep(.1)
a=pose('45_Bone_Leg1_L')
await asyncio.sleep(.27)
b=pose('45_Bone_Leg1_L')
check('walk changes evaluated bone pose',max(abs(x-y) for x,y in zip(a,b))>.001)
view.inject_local_key('Right',False)
await asyncio.sleep(.3)
a=pose('40_Bone_Hand_R')
p.set_editor_property('last_cast_slot',0)
p.set_editor_property('cast_serial',p.get_editor_property('cast_serial')+1)
await asyncio.sleep(.35)
b=pose('40_Bone_Hand_R')
check('cast changes evaluated upper-body pose',max(abs(x-y) for x,y in zip(a,b))>.001)
before=(s.get_editor_property('gold'),list(s.get_editor_property('levels')),list(s.get_editor_property('items')))
looks=[]
for i in range(3):
    s.set_editor_property('appearance',i)
    await asyncio.sleep(.08)
    looks.append(avatar.get_material(0).get_path_name())
check('three appearances allocate distinct materials',len(set(looks))==3)
check('appearance leaves gold and build intact',before==(s.get_editor_property('gold'),list(s.get_editor_property('levels')),list(s.get_editor_property('items'))))
report={'checks':checks,'passed':len(checks),'bones':names,'visual_review':'separate rendered package screenshots'}
Path(unreal.Paths.project_saved_dir(),'Validation','character.json').write_text(json.dumps(report,indent=2),encoding='utf8')
report
