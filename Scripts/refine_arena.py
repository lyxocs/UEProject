"""UE editor Python: versioned scene refinement, without touching gameplay bounds."""
import math
import random
from pathlib import Path
import unreal

ROOT='/Game/WarlockArena'
E=unreal.EditorAssetLibrary
M=unreal.MaterialEditingLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
VERSION='5'
changed=[]

def constant(mat,value,prop):
    node=M.create_material_expression(mat,unreal.MaterialExpressionConstant)
    node.set_editor_property('r',value)
    M.connect_material_property(node,'',prop)

def custom(mat,code,inputs,output=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
    node=M.create_material_expression(mat,unreal.MaterialExpressionCustom)
    entries=[]
    for name,source in inputs.items():
        item=unreal.CustomInput();item.set_editor_property('input_name',name);entries.append(item)
    node.set_editor_properties({'inputs':entries,'code':code,'output_type':output})
    for name,source in inputs.items():M.connect_material_expressions(source,'',node,name)
    return node

def refine(name,lava=False):
    mat=E.load_asset(ROOT+'/Materials/'+name)
    if not lava:M.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_NANITE)
    if E.get_metadata_tag(mat,'WarlockRefinement')==VERSION:return
    M.delete_all_material_expressions(mat)
    p=M.create_material_expression(mat,unreal.MaterialExpressionWorldPosition)
    t=M.create_material_expression(mat,unreal.MaterialExpressionTime)
    tint=M.create_material_expression(mat,unreal.MaterialExpressionVectorParameter)
    tint.set_editor_properties({'parameter_name':'Tint','default_value':unreal.LinearColor(.18,.17,.155,1)})
    noise='''
float2 uv=P.xy/160;float2 cell=floor(uv),f=frac(uv);float d0=4,d1=4;float id=0;
for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++){
float2 g=float2(x,y),c=cell+g;
float2 h=frac(sin(float2(dot(c,float2(127.1,311.7)),dot(c,float2(269.5,183.3))))*43758.5453);
float d=length(g+h*.8+.1-f);if(d<d0){d1=d0;d0=d;id=h.x;}else d1=min(d1,d);}
float edge=d1-d0;
float n=.5+.22*sin(P.x*.13+sin(P.y*.07)*2)+.13*sin(P.y*.29+P.x*.21)+.08*sin(P.x*1.9)*sin(P.y*2.3);
'''
    if lava:
        lava_noise=noise.replace('float2 uv=P.xy/160;', 'float2 uv=P.xy/190;uv+=float2(sin(P.y*.012+T*.13),cos(P.x*.009+T*.09))*.38;')
        code=lava_noise+'''
float flow=.5+.5*sin(P.x*.011+P.y*.007+sin(P.y*.016+T*.12)*2+T*.16);
float heat=1-smoothstep(.008,.045,edge);
float3 crust=float3(.018,.014,.012)*(.7+n*.35);
return crust+float3(.95,.09,.003)*heat*(.8+.2*flow)+float3(.12,.006,.001)*pow(flow,24);
'''
        node=custom(mat,code,{'P':p,'T':t,'Tint':tint})
        M.connect_material_property(node,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        constant(mat,.85,unreal.MaterialProperty.MP_ROUGHNESS)
    else:
        code=noise+'''
float joint=smoothstep(.006,.024,edge);
float r=length(P.xy);float ring=1-smoothstep(1.3,3.5,abs(r-925));
float marks=pow(saturate(cos(atan2(P.y,P.x)*32)),40)*(1-smoothstep(8,20,abs(r-900)));
return Tint.rgb*(.65+id*.4+n*.28)*lerp(.24,1,joint)+float3(.19,.12,.04)*(ring+marks);
'''
        node=custom(mat,code,{'P':p,'T':t,'Tint':tint})
        M.connect_material_property(node,'',unreal.MaterialProperty.MP_BASE_COLOR)
        rough=custom(mat,noise+'return clamp(.84+n*.07-id*.13,.65,.95);',{'P':p},unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        M.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
        # World-space geometric bump on the broad arena surface, with bounded height.
        normal=custom(mat,noise+'''float h=smoothstep(.006,.024,edge)*1.5+n*.3;
float3 q=P+float3(0,0,h);float3 normal=normalize(cross(ddy(q),ddx(q)));
return normal.z<0?-normal:normal;''',{'P':p})
        mat.set_editor_property('tangent_space_normal',False)
        M.connect_material_property(normal,'',unreal.MaterialProperty.MP_NORMAL)
        constant(mat,.04,unreal.MaterialProperty.MP_METALLIC)
    E.set_metadata_tag(mat,'WarlockRefinement',VERSION)
    changed.append(mat.get_path_name())

refine('M_Stone');refine('M_Lava',True)

# One owned irregular basalt formation replaces the editor's perfect cone props.
rock_path=ROOT+'/Environment/SM_BasaltFormation_Detailed' if E.does_asset_exist(ROOT+'/Environment/SM_BasaltFormation_Detailed') else ROOT+'/Environment/SM_BasaltFormation'
if not E.does_asset_exist(rock_path):
    rng=random.Random(8124);verts=[];faces=[];sides=16
    radii=[1,.96,.82,.72,.48,.26,.04]
    offsets=[(rng.uniform(-.15,.15),rng.uniform(-.15,.15)) for _ in radii]
    for k,r in enumerate(radii):
        for j in range(sides):
            a=j*math.tau/sides;rr=r*rng.uniform(.8,1.2)
            verts.append(((math.cos(a)*rr+offsets[k][0])*50,(math.sin(a)*rr+offsets[k][1])*50,k/6*100-50))
    for k in range(len(radii)-1):
        for j in range(sides):
            a=k*sides+j;b=k*sides+(j+1)%sides;c=b+sides;d=a+sides
            faces.extend([(a,b,c),(a,c,d)])
    source=Path(unreal.Paths.project_dir(),'AssetsSource','Environment','SM_BasaltFormation.obj');source.parent.mkdir(parents=True,exist_ok=True)
    source.write_text('\n'.join(['# Original WarlockArena basalt geometry','s 1']+['v %.6f %.6f %.6f'%v for v in verts]+['f %d %d %d'%tuple(i+1 for i in f) for f in faces]),encoding='utf8')
    task=unreal.AssetImportTask();task.set_editor_properties({'filename':str(source),'destination_path':ROOT+'/Environment','automated':True,'save':True,'replace_existing':False})
    tools.import_asset_tasks([task])
rock=E.load_asset(rock_path)
rock.set_material(0,E.load_asset(ROOT+'/Materials/M_Stone'))
all_actors=sub.get_all_level_actors()
for actor in all_actors:
    label=actor.get_actor_label()
    if label.startswith('Obsidian formation'):
        actor.static_mesh_component.set_static_mesh(rock)
        index=int(label.rsplit(' ',1)[1]);actor.set_actor_rotation(unreal.Rotator(pitch=0,yaw=index*31,roll=0),False)
    if label=='Cold moon - key light':
        actor.set_actor_rotation(unreal.Rotator(pitch=-55,yaw=145,roll=0),False)
        actor.light_component.set_editor_properties({'intensity':2.4,'light_color':unreal.Color(r=255,g=238,b=218,a=255),'light_source_angle':2.0,'contact_shadow_length':.15})
    if label.startswith('Lava glow'):
        actor.light_component.set_editor_properties({'intensity':35.,'light_color':unreal.Color(r=255,g=100,b=24,a=255),'cast_shadows':False})
    if label=='Ambient fill':actor.light_component.set_editor_properties({'intensity':.65,'lower_hemisphere_color':unreal.LinearColor(.07,.065,.06,1)})
    if label=='Volcanic atmosphere':actor.component.set_editor_property('fog_density',.008)
volume=next((a for a in all_actors if a.get_actor_label()=='Arena image finish'),None)
if not volume:
    volume=sub.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector(0,0,0),unreal.Rotator())
    volume.set_actor_label('Arena image finish')
volume.set_editor_property('unbound',True)
settings=volume.get_editor_property('settings')
for key,value in {'bloom_intensity':.25,'vignette_intensity':.18,'scene_fringe_intensity':0.,'film_grain_intensity':0.,'motion_blur_amount':0.}.items():
    settings.set_editor_property('override_'+key,True);settings.set_editor_property(key,value)
volume.set_editor_property('settings',settings)
E.save_directory(ROOT,only_if_is_dirty=True)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
{'changed':changed,'rock':rock_path,'lighting':'warm neutral key, restrained bloom, no motion blur'}
