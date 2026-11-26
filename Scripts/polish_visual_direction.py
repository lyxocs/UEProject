"""Versioned UE material/lighting pass. No terrain or collision changes.

Only replace project-authored custom shader code; preserve assets and references.
Run against the Arena level through official editor Python/MCP.
"""
import unreal

E=unreal.EditorAssetLibrary;M=unreal.MaterialEditingLibrary
VERSION='natural-arena-v4'
LAVA_VERSION='natural-lava-v5'
def node(mat,cls,**values):
    n=M.create_material_expression(mat,cls)
    if values:n.set_editor_properties(values)
    return n
def parameter(mat,name,value):
    return node(mat,unreal.MaterialExpressionVectorParameter,parameter_name=name,default_value=unreal.LinearColor(*value))
def input_to(n,name,src):
    inputs=list(n.get_editor_property('inputs'))
    if not any(str(i.get_editor_property('input_name'))==name for i in inputs):
        item=unreal.CustomInput();item.set_editor_property('input_name',name);inputs.append(item);n.set_editor_property('inputs',inputs)
    assert M.connect_material_expressions(src,'',n,name)
def owned(mat):return [n for n in unreal.ObjectIterator(unreal.MaterialExpressionCustom) if n.get_path_name().startswith(mat.get_path_name())]

stone=unreal.load_asset('/Game/WarlockArena/Materials/M_Stone')
common='''
float2 st=(P.xy+P.z*float2(.37,.21))/98+float2(sin(P.y*.007),cos(P.x*.006))*.09;
float2 cell=floor(st),f=frac(st);float d0=4,d1=4,id=0;
for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++){
float2 c=cell+float2(x,y);float2 h=frac(sin(float2(dot(c,float2(127.1,311.7)),dot(c,float2(269.5,183.3))))*43758.5453);
float d=length(float2(x,y)+h*.8+.1-f);if(d<d0){d1=d0;d0=d;id=h.x;}else d1=min(d1,d);}
float edge=d1-d0;
float2 np=(P.xy+P.z*float2(.37,.21))*.085;float ns=0,amp=.65;
for(int oct=0;oct<2;oct++){
float2 ip=floor(np),ff=frac(np);ff=ff*ff*(3-2*ff);
float4 h=frac(sin(float4(dot(ip,float2(127.1,311.7)),dot(ip+float2(1,0),float2(127.1,311.7)),dot(ip+float2(0,1),float2(127.1,311.7)),dot(ip+1,float2(127.1,311.7))))*43758.5453);
ns+=lerp(lerp(h.x,h.y,ff.x),lerp(h.z,h.w,ff.x),ff.y)*amp;np=np*2.03+17.1;amp*=.5;}
float mineral=.2+ns;
float joint=smoothstep(.004,.017,edge);
'''
if E.get_metadata_tag(stone,'WarlockArtDirection')!=VERSION:
    tint=parameter(stone,'ThemeTint',(1,1,1,1));count=0
    for n in owned(stone):
        code=n.get_editor_property('code')
        if 'return Tint.rgb*' in code:
            input_to(n,'ThemeTint',tint)
            n.set_editor_property('code',common+'''
float r=length(P.xy);float engraving=exp(-abs(r-925)*.28)*.22+exp(-abs(r-900)*.35)*.10;
float marks=pow(saturate(cos(atan2(P.y,P.x)*48)),32)*exp(-abs(r-910)*.16)*.18;
float grain=.70+id*.12+mineral*.24;
return Tint.rgb*ThemeTint.rgb*grain*lerp(.58,1,joint)+float3(.11,.085,.052)*(engraving+marks);
''');count+=1
        elif 'return clamp(' in code:
            n.set_editor_property('code',common+'return clamp(.78+mineral*.12-id*.08,.68,.94);');count+=1
        elif 'cross(ddy(' in code:
            n.set_editor_property('code',common+'''float h=joint*.28+mineral*.12;
float3 displaced=P+normalize(N)*h;float3 normal=normalize(cross(ddy(displaced),ddx(displaced)));return dot(normal,N)<0?-normal:normal;''');count+=1
    assert count==3,count
    E.set_metadata_tag(stone,'WarlockArtDirection',VERSION);E.save_loaded_asset(stone)

lava=unreal.load_asset('/Game/WarlockArena/Materials/M_Lava')
if E.get_metadata_tag(lava,'WarlockArtDirection')!=LAVA_VERSION:
    core=parameter(lava,'LavaCore',(.52,.13,.018,1));bank=parameter(lava,'LavaBank',(.24,.035,.004,1));count=0
    for n in owned(lava):
        if 'float heat=' not in n.get_editor_property('code'):continue
        input_to(n,'LavaCore',core);input_to(n,'LavaBank',bank)
        n.set_editor_property('code','''
float2 uv=P.xy/170;
uv+=float2(sin(P.y*.006+T*.045),cos(P.x*.005-T*.035))*.48;
uv+=float2(sin(P.y*.041+sin(P.x*.019)*2),cos(P.x*.037+P.y*.013))*.22;
float2 cell=floor(uv),f=frac(uv);float d0=4,d1=4,id=0;
for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++){
float2 c=cell+float2(x,y);float2 h=frac(sin(float2(dot(c,float2(127.1,311.7)),dot(c,float2(269.5,183.3))))*43758.5453);
float d=length(float2(x,y)+h*.8+.1-f);if(d<d0){d1=d0;d0=d;id=h.x;}else d1=min(d1,d);}
float edge=d1-d0;
float2 np=uv*3.1;float grain=0,amp=.62;
for(int oct=0;oct<3;oct++){
float2 ip=floor(np),ff=frac(np);ff=ff*ff*(3-2*ff);
float4 h=frac(sin(float4(dot(ip,float2(127.1,311.7)),dot(ip+float2(1,0),float2(127.1,311.7)),dot(ip+float2(0,1),float2(127.1,311.7)),dot(ip+1,float2(127.1,311.7))))*43758.5453);
grain+=lerp(lerp(h.x,h.y,ff.x),lerp(h.z,h.w,ff.x),ff.y)*amp;np=np*2.03+17.1;amp*=.5;}
float current=.5+.5*sin(P.x*.007+P.y*.005+sin(P.y*.012+T*.08)*2+T*.11);
float heat=1-smoothstep(.003,lerp(.018,.060,grain),edge);
float cooling=smoothstep(.24,.68,grain+current*.1);
float pools=smoothstep(.55,.79,grain+current*.09);
float pockets=pow(saturate(sin(P.x*.015+T*.07)*sin(P.y*.019-T*.04)),8)*.23;
heat=max(heat*.65,pools*.55);
float3 crust=float3(.018,.014,.013)*(.65+grain*.45);
return crust+LavaBank.rgb*(heat*(.18+.7*cooling)+exp(-edge*24)*.10)
      +LavaCore.rgb*(pow(heat,3)*cooling*(.55+.3*current)+pockets+pools*.07);
''');count+=1
    assert count==1,count
    E.set_metadata_tag(lava,'WarlockArtDirection',LAVA_VERSION);E.save_loaded_asset(lava)

path='/Game/WarlockArena/Materials/M_ArenaBoundary'
if not E.does_asset_exist(path):
    mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_ArenaBoundary','/Game/WarlockArena/Materials',unreal.Material,unreal.MaterialFactoryNew())
    mat.set_editor_properties({'blend_mode':unreal.BlendMode.BLEND_ADDITIVE,'shading_model':unreal.MaterialShadingModel.MSM_UNLIT,'two_sided':True})
    uv=node(mat,unreal.MaterialExpressionTextureCoordinate);t=node(mat,unreal.MaterialExpressionTime);color=parameter(mat,'Tint',(.6,.4,.2,1))
    c=node(mat,unreal.MaterialExpressionCustom,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    for name,source in {'UV':uv,'T':t,'Tint':color}.items():input_to(c,name,source)
    c.set_editor_property('code','''float2 p=(UV-.5)*2;float r=length(p),a=atan2(p.y,p.x);float aa=max(fwidth(r),.0015);float rim=1-smoothstep(aa,aa*2.2,abs(r-.995));float ticks=pow(saturate(cos(a*64)),24)*exp(-abs(r-.978)*260);return Tint.rgb*(rim*.55+ticks*.24)*(.86+.06*sin(T*1.6));''')
    M.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity=node(mat,unreal.MaterialExpressionConstant,r=1);M.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
    E.save_loaded_asset(mat)

for name,code in {
    'M_ShockwaveRing':'''float2 p=(UV-.5)*2;float r=length(p),a=atan2(p.y,p.x);float shell=exp(-abs(r-.94)*105);float wake=exp(-abs(r-.81)*36)*.24;float fragments=pow(saturate(sin(a*37+sin(a*9)*2)),14)*exp(-abs(r-.89)*55)*.32;return Tint.rgb*(shell+wake+fragments)*Opacity;''',
    'M_MeteorWarning':'''float2 p=(UV-.5)*2;float r=length(p),a=atan2(p.y,p.x);float edge=exp(-abs(r-.98)*150);float inner=exp(-abs(r-.78)*95)*.35;float marks=pow(saturate(cos(a*12)),22)*exp(-abs(r-.89)*50);float spokes=pow(saturate(cos(a*6+T*.45)),45)*smoothstep(.30,.7,r)*(1-smoothstep(.76,.88,r));return Tint.rgb*(edge*.65+inner+marks*.4+spokes*.18)*Opacity*(.83+.10*sin(T*10));'''
}.items():
    path='/Game/WarlockArena/Materials/'+name
    if E.does_asset_exist(path):continue
    mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,'/Game/WarlockArena/Materials',unreal.Material,unreal.MaterialFactoryNew())
    mat.set_editor_properties({'blend_mode':unreal.BlendMode.BLEND_ADDITIVE,'shading_model':unreal.MaterialShadingModel.MSM_UNLIT,'two_sided':True})
    c=node(mat,unreal.MaterialExpressionCustom,code=code,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    for key,source in {'UV':node(mat,unreal.MaterialExpressionTextureCoordinate),'T':node(mat,unreal.MaterialExpressionTime),'Tint':parameter(mat,'Tint',(1,.5,.1,1)),'Opacity':node(mat,unreal.MaterialExpressionScalarParameter,parameter_name='Opacity',default_value=1)}.items():input_to(c,key,source)
    M.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity=node(mat,unreal.MaterialExpressionConstant,r=1);M.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
    E.save_loaded_asset(mat)

for name in ['M_SorcererCloth_Natural','M_SorcererMetal_Natural','M_SorcererSkin_Natural']:
    mat=unreal.load_asset('/Game/WarlockArena/Characters/Materials/'+name)
    if E.get_metadata_tag(mat,'WarlockHitFinish')==VERSION:continue
    flash=node(mat,unreal.MaterialExpressionScalarParameter,parameter_name='HitFlash',default_value=0)
    c=node(mat,unreal.MaterialExpressionCustom,code='return float3(.42,.28,.14)*Flash;',output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3);input_to(c,'Flash',flash)
    M.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    E.set_metadata_tag(mat,'WarlockHitFinish',VERSION);E.save_loaded_asset(mat)

palette=unreal.load_asset('/Game/WarlockArena/Characters/DA_WarlockAppearances')
looks=list(palette.get_editor_property('looks'))
for i,(cloth,metal,glow) in enumerate([
    ((.18,.046,.036,1),(.31,.215,.115,1),(.48,.13,.023,1)),
    ((.042,.095,.17,1),(.27,.32,.36,1),(.075,.23,.38,1)),
    ((.052,.13,.073,1),(.23,.255,.145,1),(.13,.31,.14,1))]):
    looks[i].set_editor_properties({'cloth':unreal.LinearColor(*cloth),'metal':unreal.LinearColor(*metal),'glow':unreal.LinearColor(*glow)})
palette.set_editor_property('looks',looks);E.save_loaded_asset(palette)

sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for actor in sub.get_all_level_actors():
    label=actor.get_actor_label()
    if label=='Cold moon - key light':actor.light_component.set_editor_properties({'intensity':2.6,'light_color':unreal.Color(235,240,250,255),'light_source_angle':2.5})
    if label=='Ambient fill':actor.light_component.set_editor_properties({'intensity':.95,'lower_hemisphere_color':unreal.LinearColor(.10,.11,.13,1)})
    if label=='Arena image finish':
        settings=actor.get_editor_property('settings')
        for key,value in {'bloom_intensity':.18,'vignette_intensity':.12,'motion_blur_amount':0.}.items():settings.set_editor_property('override_'+key,True);settings.set_editor_property(key,value)
        actor.set_editor_property('settings',settings)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print('Updated arena stone, lava, boundary, wardrobe, hit flash and neutral lighting: '+VERSION)
