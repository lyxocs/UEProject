"""UE editor Python: import derived character and project-authored arena meshes."""
import math
import random
from pathlib import Path
import unreal

ROOT='/Game/WarlockArena'
source=Path(unreal.Paths.project_dir())/'AssetsSource'
E=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
created=[]
def import_file(file,destination):
    task=unreal.AssetImportTask();task.set_editor_properties({'filename':str(file),'destination_path':destination,'automated':True,'save':True,'replace_existing':False})
    tools.import_asset_tasks([task]);created.extend(str(x) for x in task.get_editor_property('imported_object_paths'))

if not E.does_asset_exist(ROOT+'/Characters/RefinedWarlock/Warlock/SkeletalMeshes/Warlock'):
    import_file(source/'Warlock/Refined/Warlock.glb',ROOT+'/Characters/RefinedWarlock')

def surface(name,rings,sides=64,rough=False):
    destination=ROOT+'/Environment'
    if E.does_asset_exist(destination+'/'+name):return
    vertices=[];faces=[];rng=random.Random(8192)
    phase=[rng.uniform(0,math.tau) for _ in range(5)]
    for radius,z in rings:
        for j in range(sides):
            a=math.tau*j/sides
            r=radius
            if rough:
                r*=.91+.045*math.sin(a*3+phase[0])+ .025*math.sin(a*7+phase[1]+z*.025)+.018*math.sin(a*11+phase[2])+ .012*math.sin(z*.7+a*3)
            vertices.append((r*math.cos(a),r*math.sin(a),z))
    for k in range(len(rings)-1):
        for j in range(sides):
            a=k*sides+j;b=k*sides+(j+1)%sides;c=b+sides;d=a+sides
            faces.extend([(a,b,c),(a,c,d)])
    bottom=len(vertices);vertices.append((0,0,rings[0][1]));top=len(vertices);vertices.append((0,0,rings[-1][1]))
    for j in range(sides):
        faces.append((bottom,(j+1)%sides,j));faces.append((top,(len(rings)-1)*sides+j,(len(rings)-1)*sides+(j+1)%sides))
    path=source/'Environment'/f'{name}.obj';path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text('\n'.join(['# Original WarlockArena mesh, cm and Z up','s 1']+['v %.6f %.6f %.6f'%p for p in vertices]+['f %d %d %d'%tuple(i+1 for i in f) for f in faces]),encoding='utf8')
    import_file(path,destination)
    mesh=E.load_asset(destination+'/'+name);mesh.set_material(0,E.load_asset(ROOT+'/Materials/M_Stone'))

# Same maximum radius/height as the previous collision-free primitive visuals.
surface('SM_ArenaPlatform',[(48,-50),(49,-47),(50,-43),(50,-29),(49.7,-25),(49.5,-23),(50,-20),(50,32),(49.6,36),(49.6,39),(50,43),(50,50)],256)
surface('SM_ArenaPillar',[(47,-50),(50,-47),(50,-40),(44,-36),(43,-28),(44,-18),(42,-8),(44,2),(43,12),(44,22),(43,33),(48,38),(50,41),(50,46),(47,50)],48,True)
surface('SM_BasaltFormation_Detailed',[(50*(1-(i/20)**2*.92),-50+i*5) for i in range(21)],48,True)

actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rock=E.load_asset(ROOT+'/Environment/SM_BasaltFormation_Detailed')
for actor in actors.get_all_level_actors():
    if actor.get_actor_label().startswith('Obsidian formation'):actor.static_mesh_component.set_static_mesh(rock)
E.save_directory(ROOT,only_if_is_dirty=True)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
{'created':created,'derived_character':ROOT+'/Characters/RefinedWarlock','environment':['SM_ArenaPlatform','SM_ArenaPillar','SM_BasaltFormation_Detailed']}
