"""UE editor Python: import missing source character and create owned PBR/ornament assets."""
from pathlib import Path
import math
import unreal

ROOT = '/Game/WarlockArena'
SOURCE = Path(unreal.Paths.project_dir()) / 'AssetsSource' / 'Warlock'
assets = unreal.AssetToolsHelpers.get_asset_tools()
M = unreal.MaterialEditingLibrary
created = []


def import_file(filename, destination):
    task = unreal.AssetImportTask()
    task.set_editor_properties({'filename': str(filename), 'destination_path': destination, 'automated': True, 'save': True, 'replace_existing': False})
    assets.import_asset_tasks([task])
    created.extend(str(p) for p in task.get_editor_property('imported_object_paths'))


model = ROOT + '/Characters/SourceWarlock/Warlock/SkeletalMeshes/Warlock'
if not unreal.EditorAssetLibrary.does_asset_exist(model):
    import_file(SOURCE/'Warlock.glb', ROOT+'/Characters/SourceWarlock')


def master(name, metal, rough, glow=False):
    path = ROOT+'/Characters/Materials/'+name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        mat=unreal.EditorAssetLibrary.load_asset(path)
        if metal>.5:M.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_NANITE)
        return
    else:mat = assets.create_asset(name, ROOT+'/Characters/Materials', unreal.Material, unreal.MaterialFactoryNew())
    M.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    if metal>.5:M.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_NANITE)
    if glow: mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    tint = M.create_material_expression(mat, unreal.MaterialExpressionVectorParameter)
    tint.set_editor_properties({'parameter_name':'Tint','default_value':unreal.LinearColor(.15,.07,.035,1)})
    uv = M.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate)
    node = M.create_material_expression(mat, unreal.MaterialExpressionCustom)
    entries=[]
    for field in ['UV','Tint']:
        e=unreal.CustomInput();e.set_editor_property('input_name',field);entries.append(e)
    node.set_editor_properties({'inputs':entries,'output_type':unreal.CustomMaterialOutputType.CMOT_FLOAT3})
    node.set_editor_property('code', 'return Tint.rgb*.8;' if glow else (
        'return Tint.rgb*(.98+.015*sin(UV.x*70)*sin(UV.y*90));' if 'Skin' in name else
        'float weave=sin(UV.x*700)*sin(UV.y*700);float brocade=pow(saturate(sin(UV.x*120)*sin(UV.y*90)),12);return Tint.rgb*(.91+.02*weave+.05*brocade);' if metal<.5 else
        'float scratch=sin(UV.x*300+sin(UV.y*40))*sin(UV.y*130);return Tint.rgb*(.94+.04*scratch);'))
    M.connect_material_expressions(uv,'',node,'UV');M.connect_material_expressions(tint,'',node,'Tint')
    M.connect_material_property(node,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR if glow else unreal.MaterialProperty.MP_BASE_COLOR)
    for value,prop in [(rough,unreal.MaterialProperty.MP_ROUGHNESS),(metal,unreal.MaterialProperty.MP_METALLIC)]:
        c=M.create_material_expression(mat,unreal.MaterialExpressionConstant);c.set_editor_property('r',value);M.connect_material_property(c,'',prop)
    created.append(path)
    unreal.EditorAssetLibrary.set_metadata_tag(mat,'WarlockFinish','natural-v1')


master('M_SorcererCloth_Natural',0,.83)
master('M_SorcererMetal_Natural',.65,.48)
master('M_SorcererSkin_Natural',0,.68)
master('M_SorcererGlow_Natural',0,.3,True)

# Data asset contains only art parameters and remains editable after creation.
palette_path=ROOT+'/Characters/DA_WarlockAppearances'
if not unreal.EditorAssetLibrary.does_asset_exist(palette_path):
    factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.WarlockAppearanceSet)
    palette=assets.create_asset('DA_WarlockAppearances',ROOT+'/Characters',unreal.WarlockAppearanceSet,factory)
    looks=[]
    for name,cloth,metal,glow in [
        ('Crimson',(.09,.036,.032),(.26,.19,.11),(.55,.16,.025)),
        ('Moon',(.035,.055,.08),(.26,.29,.31),(.09,.25,.4)),
        ('Grove',(.035,.065,.045),(.16,.19,.12),(.12,.32,.15))]:
        look=unreal.WarlockLook();look.set_editor_properties({'name':name,'cloth':unreal.LinearColor(*cloth,1),'metal':unreal.LinearColor(*metal,1),'glow':unreal.LinearColor(*glow,1)});looks.append(look)
    palette.set_editor_property('looks',looks);created.append(palette_path)

rune_path=ROOT+'/Characters/Materials/M_SorcererRune'
if not unreal.EditorAssetLibrary.does_asset_exist(rune_path):
    rune=assets.create_asset('M_SorcererRune',ROOT+'/Characters/Materials',unreal.Material,unreal.MaterialFactoryNew())
    rune.set_editor_properties({'blend_mode':unreal.BlendMode.BLEND_MASKED,'shading_model':unreal.MaterialShadingModel.MSM_UNLIT})
    p=M.create_material_expression(rune,unreal.MaterialExpressionWorldPosition)
    o=M.create_material_expression(rune,unreal.MaterialExpressionObjectPositionWS)
    tint=M.create_material_expression(rune,unreal.MaterialExpressionVectorParameter);tint.set_editor_property('parameter_name','Tint')
    M.connect_material_property(tint,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    node=M.create_material_expression(rune,unreal.MaterialExpressionCustom)
    entries=[]
    for field in ['P','O']:
        e=unreal.CustomInput();e.set_editor_property('input_name',field);entries.append(e)
    node.set_editor_properties({'inputs':entries,'output_type':unreal.CustomMaterialOutputType.CMOT_FLOAT1,'code':'float2 v=P.xy-O.xy;float r=length(v);float ring=1-smoothstep(.5,1.2,abs(r-33));float glyph=(1-smoothstep(.4,1.2,abs(r-27)))*pow(saturate(cos(atan2(v.y,v.x)*12)),12);return max(ring,glyph);'})
    M.connect_material_expressions(p,'',node,'P');M.connect_material_expressions(o,'',node,'O');M.connect_material_property(node,'',unreal.MaterialProperty.MP_OPACITY_MASK)
    created.append(rune_path)


def ornament(style):
    """Original, small head ornaments; kept separate from the source skeleton."""
    verts=[];faces=[]
    def tube(points, radii, sides=10):
        start=len(verts)
        for (x,y,z),r in zip(points,radii):
            for j in range(sides):
                a=j*math.tau/sides;verts.append((x+r*math.cos(a),y+r*math.sin(a),z))
        for i in range(len(points)-1):
            for j in range(sides):
                a=start+i*sides+j;b=start+i*sides+(j+1)%sides;c=b+sides;d=a+sides
                faces.extend([(a,b,c),(a,c,d)])
    # Crown ring, radius ~13 map units, follows the head bone.
    n=48
    for r,z in [(13,0),(13,2.2),(11.5,0),(11.5,2.2)]:
        for j in range(n):
            a=j*math.tau/n;verts.append((r*math.cos(a),r*math.sin(a),z))
    for j in range(n):
        k=(j+1)%n
        for a,b,c,d in [(j,k,n+k,n+j),(2*n+j,3*n+j,3*n+k,2*n+k),(n+j,n+k,3*n+k,3*n+j)]:faces.extend([(a,b,c),(a,c,d)])
    if style=='Crimson':
        for y in [-9,0,9]:tube([(11,y,2),(12,y,8),(10,y,14 if y==0 else 10)],[2,1.4,.08])
    elif style=='Moon':
        for side in [-1,1]:
            tube([(0,side*12,2),(-2,side*15,9),(-5,side*14,17),(-6,side*9,22)],[2.1,1.7,1,.08])
    else:
        for side in [-1,1]:tube([(-3,side*11,2),(-8,side*16,8),(-10,side*20,15),(-7,side*21,20)],[2.6,1.8,1,.08])
    text=['# Owned WarlockArena ornamental mesh; Z up, centimeters','s 1']
    text+=['v %.6f %.6f %.6f'%v for v in verts]
    text+=['f %d %d %d'%tuple(x+1 for x in f) for f in faces]
    filename=SOURCE/f'SM_{style}Crown.obj';filename.write_text('\n'.join(text),encoding='utf8')
    dest=ROOT+'/Characters/Ornaments'
    if not unreal.EditorAssetLibrary.does_asset_exist(dest+'/SM_'+style+'Crown'): import_file(filename,dest)


for style in ['Crimson','Moon','Grove']: ornament(style)
unreal.EditorAssetLibrary.save_directory(ROOT+'/Characters',only_if_is_dirty=True)
{'created':created,'source_model':model,'variants':['Crimson','Moon','Grove']}
