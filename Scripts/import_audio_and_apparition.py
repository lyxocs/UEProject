"""Idempotent UE import of original sound beds and authored apparition material."""
from pathlib import Path
import json
import unreal
E=unreal.EditorAssetLibrary;M=unreal.MaterialEditingLibrary
root=Path(unreal.Paths.project_dir());manifest=json.loads((root/'AssetsSource/Audio/audio-manifest.json').read_text())
for item in manifest['assets']:
    path='/Game/WarlockArena/Audio/'+item['name']
    if not E.does_asset_exist(path):
        task=unreal.AssetImportTask();task.set_editor_properties({'filename':str(root/'AssetsSource/Audio'/(item['name']+'.wav')),'destination_path':'/Game/WarlockArena/Audio','automated':True,'save':True,'replace_existing':False})
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    sound=unreal.load_asset(path);assert isinstance(sound,unreal.SoundWave),path
    sound.set_editor_property('looping',item['loop']);E.save_loaded_asset(sound)
path='/Game/WarlockArena/Materials/M_ApparitionPortal'
if not E.does_asset_exist(path):
    mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_ApparitionPortal','/Game/WarlockArena/Materials',unreal.Material,unreal.MaterialFactoryNew())
    mat.set_editor_properties({'blend_mode':unreal.BlendMode.BLEND_TRANSLUCENT,'shading_model':unreal.MaterialShadingModel.MSM_UNLIT,'two_sided':True})
    def node(cls,**values):
        n=M.create_material_expression(mat,cls)
        if values:n.set_editor_properties(values)
        return n
    uv=node(unreal.MaterialExpressionTextureCoordinate);time=node(unreal.MaterialExpressionTime)
    opacity=node(unreal.MaterialExpressionScalarParameter,parameter_name='Opacity',default_value=1)
    tint=node(unreal.MaterialExpressionVectorParameter,parameter_name='Tint',default_value=unreal.LinearColor(.1,.08,.16,1))
    common='float2 p=(UV-.5)*2;float r=length(p);float a=atan2(p.y,p.x);float curl=sin(a*7-r*19+T*13)*.035+sin(a*13+r*8-T*8)*.018;float edge=1-smoothstep(.66,.99,r+curl);'
    for output,code in [(unreal.MaterialProperty.MP_EMISSIVE_COLOR,'float rim=exp(-abs(r-.77-curl)*32);return Tint.rgb*(rim*.45+edge*.018);'),(unreal.MaterialProperty.MP_OPACITY,'return edge*.94*Opacity;')]:
        c=node(unreal.MaterialExpressionCustom,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3 if output==unreal.MaterialProperty.MP_EMISSIVE_COLOR else unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        inputs=[]
        for name in ['UV','T','Opacity','Tint']:
            x=unreal.CustomInput();x.set_editor_property('input_name',name);inputs.append(x)
        c.set_editor_properties({'inputs':inputs,'code':common+code})
        for name,source in [('UV',uv),('T',time),('Opacity',opacity),('Tint',tint)]:assert M.connect_material_expressions(source,'',c,name)
        assert M.connect_material_property(c,'',output)
    M.recompile_material(mat);assert E.save_loaded_asset(mat)
{'sounds':len(manifest['assets']),'portal':path}
