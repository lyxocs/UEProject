"""Project-authored shaders and derived source-model polish, via UE asset APIs."""
from pathlib import Path
import unreal

E=unreal.EditorAssetLibrary
M=unreal.MaterialEditingLibrary
assets=unreal.AssetToolsHelpers.get_asset_tools()
ROOT='/Game/WarlockArena'
changed=[]

def node(mat,cls,**values):
    n=M.create_material_expression(mat,cls)
    if values:n.set_editor_properties(values)
    return n

def custom(mat,code,inputs,scalar=False):
    entries=[]
    for name in inputs:
        e=unreal.CustomInput();e.set_editor_property('input_name',name);entries.append(e)
    n=node(mat,unreal.MaterialExpressionCustom,inputs=entries,code=code,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1 if scalar else unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    for name,src in inputs.items():assert M.connect_material_expressions(src,'',n,name)
    return n

def sprite(name,code,ghost=False):
    path=ROOT+'/Materials/'+name
    if E.does_asset_exist(path):return unreal.load_asset(path)
    mat=assets.create_asset(name,ROOT+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    mat.set_editor_properties({'blend_mode':unreal.BlendMode.BLEND_TRANSLUCENT if ghost else unreal.BlendMode.BLEND_ADDITIVE,'shading_model':unreal.MaterialShadingModel.MSM_UNLIT,'two_sided':True})
    inputs={'UV':node(mat,unreal.MaterialExpressionTextureCoordinate),'T':node(mat,unreal.MaterialExpressionTime),'Tint':node(mat,unreal.MaterialExpressionVectorParameter,parameter_name='Tint',default_value=unreal.LinearColor(1,.25,.02,1)),'Opacity':node(mat,unreal.MaterialExpressionScalarParameter,parameter_name='Opacity',default_value=1),'Progress':node(mat,unreal.MaterialExpressionScalarParameter,parameter_name='Progress',default_value=0),'Seed':node(mat,unreal.MaterialExpressionScalarParameter,parameter_name='Seed',default_value=0)}
    if ghost:inputs['F']=node(mat,unreal.MaterialExpressionFresnel)
    color=custom(mat,code,inputs)
    M.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity=custom(mat,'return Opacity*(.035+.45*F);',{'Opacity':inputs['Opacity'],'F':inputs['F']},True) if ghost else node(mat,unreal.MaterialExpressionConstant,r=1)
    M.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
    if ghost:M.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    E.save_loaded_asset(mat);changed.append(path)
    return mat

sprite('M_FlameTongue','''
float2 p=(UV-.5)*2; float a=Seed+T*.85; p=mul(float2x2(cos(a),-sin(a),sin(a),cos(a)),p);
float y=(p.y+1)*.5;float curl=.18*sin(p.y*8-T*10+Seed)+.075*sin(p.y*19-T*17+Seed*3);
float width=lerp(.62,.025,saturate(y));float plume=pow(saturate(1-abs(p.x+curl)/width),1.5);
plume*=smoothstep(0,.17,y)*(1-smoothstep(.82,1,y));
float veins=.64+.22*sin(p.y*13-T*13+sin(p.x*9+Seed))+.14*sin(p.y*27-T*21);
float heat=pow(plume,5)*(1-y)*.6;
return (Tint.rgb*plume*max(.25,veins)*1.4+float3(1,.55,.12)*heat)*Opacity;
''')
sprite('M_ArcaneSprite','''float2 p=(UV-.5)*2;float r=length(p);float core=pow(saturate(1-r),3);float ray=pow(saturate(1-abs(p.x)*7),4)*pow(saturate(1-abs(p.y)),2);return Tint.rgb*(core*.65+ray*.8)*Opacity;''')
sprite('M_ArcaneRing','''
float2 p=(UV-.5)*2;float r=length(p);float a=atan2(p.y,p.x);
float ring=exp(-abs(r-.89)*140)+.45*exp(-abs(r-.76)*110);
float glyph=pow(saturate(cos(a*24+T*.8)),20)*exp(-abs(r-.82)*65);
float crack=pow(saturate(cos(a*13+sin(a*7)*2)),50)*smoothstep(.3,.75,r)*(1-smoothstep(.8,.92,r));
return Tint.rgb*(ring*.9+glyph*.65+crack*.22)*Opacity;
''')
sprite('M_BlinkEcho','''return Tint.rgb*(.45+F*1.25)*(.7+.3*sin(UV.y*180+T*15));''',True)

# Replace the opaque-looking uniform bubble with a thin edge and moving facets.
shield=unreal.load_asset(ROOT+'/Materials/M_Shield')
if E.get_metadata_tag(shield,'WarlockFXVersion')!='2':
    sources={}
    for n in unreal.ObjectIterator(unreal.MaterialExpressionCustom):
        if n.get_outer()==shield:
            old=n
            break
    f=node(shield,unreal.MaterialExpressionFresnel)
    tint=node(shield,unreal.MaterialExpressionVectorParameter,parameter_name='Tint',default_value=unreal.LinearColor(.1,.45,.7,1))
    uv=node(shield,unreal.MaterialExpressionTextureCoordinate)
    t=node(shield,unreal.MaterialExpressionTime)
    glow=custom(shield,'float facets=pow(saturate(sin(UV.x*100+T)*sin(UV.y*80-T)),18);return Tint.rgb*(.12+F*1.4+facets*.2);',{'UV':uv,'T':t,'F':f,'Tint':tint})
    alpha=custom(shield,'return .008+pow(F,1.6)*.24;',{'F':f},True)
    M.connect_material_property(glow,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);M.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
    E.set_metadata_tag(shield,'WarlockFXVersion','2');E.save_loaded_asset(shield);changed.append(shield.get_path_name())

# Recover textile roles and rich, restrained colors. Keep original art assets.
for name in ['M_SorcererCloth_Natural','M_SorcererMetal_Natural','M_SorcererSkin_Natural','M_SorcererGlow_Natural']:
    mat=unreal.load_asset(ROOT+'/Characters/Materials/'+name)
    if E.get_metadata_tag(mat,'WarlockFinish')=='polished-v2':continue
    for n in unreal.ObjectIterator(unreal.MaterialExpressionCustom):
        if n.get_outer()!=mat:continue
        if 'Cloth' in name:n.set_editor_property('code','''float fold=.8+.12*sin(UV.x*20+sin(UV.y*9)*.3);float weave=sin(UV.x*650)*sin(UV.y*650)*.012;float hem=1-smoothstep(.009,.018,min(frac(UV.x),1-frac(UV.x)));float brocade=pow(saturate(sin(UV.x*70)*cos(UV.y*55)),10)*.035;return Tint.rgb*(fold+weave+brocade)+float3(.2,.13,.055)*hem*.35;''')
        elif 'Metal' in name:n.set_editor_property('code','return Tint.rgb*(.75+.08*sin(UV.x*190)*sin(UV.y*140));')
        elif 'Skin' in name:n.set_editor_property('code','return Tint.rgb*(.92+.05*sin(UV.y*30));')
        else:n.set_editor_property('code','return Tint.rgb*.5;')
    if 'Cloth' in name:
        uv=node(mat,unreal.MaterialExpressionTextureCoordinate)
        normal=custom(mat,'return normalize(float3(.075*sin(UV.x*35)*sin(UV.y*11),.03*cos(UV.y*50),1));',{'UV':uv})
        M.connect_material_property(normal,'',unreal.MaterialProperty.MP_NORMAL)
    E.set_metadata_tag(mat,'WarlockFinish','polished-v2');E.save_loaded_asset(mat);changed.append(mat.get_path_name())

palette=unreal.load_asset(ROOT+'/Characters/DA_WarlockAppearances')
if E.get_metadata_tag(palette,'WarlockFinish')!='polished-v2':
    looks=[]
    for name,cloth,metal,glow in [('Crimson',(.18,.022,.04),(.23,.145,.065),(.55,.22,.04)),('Moon',(.035,.065,.14),(.23,.26,.3),(.12,.35,.65)),('Grove',(.024,.105,.065),(.16,.2,.12),(.12,.45,.25))]:
        look=unreal.WarlockLook();look.set_editor_properties({'name':name,'cloth':unreal.LinearColor(*cloth,1),'metal':unreal.LinearColor(*metal,1),'glow':unreal.LinearColor(*glow,1)});looks.append(look)
    palette.set_editor_properties({'looks':looks,'skin_tone':unreal.LinearColor(.46,.3,.23,1)})
    E.set_metadata_tag(palette,'WarlockFinish','polished-v2');E.save_loaded_asset(palette);changed.append(palette.get_path_name())

model=ROOT+'/Characters/PolishedWarlock/Warlock/SkeletalMeshes/Warlock'
if not E.does_asset_exist(model):
    task=unreal.AssetImportTask();task.set_editor_properties({'filename':str(Path(unreal.Paths.project_dir())/'AssetsSource/Warlock/Polished/Warlock.glb'),'destination_path':ROOT+'/Characters/PolishedWarlock','automated':True,'save':True,'replace_existing':False})
    assets.import_asset_tasks([task]);changed.extend(str(p) for p in task.get_editor_property('imported_object_paths'))
{'changed':changed,'model_exists':E.does_asset_exist(model)}
