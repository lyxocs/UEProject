"""Create the original procedural fire-sprite material; do not overwrite edits."""
import unreal

path='/Game/WarlockArena/Materials/M_FireSprite'
M=unreal.MaterialEditingLibrary
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_FireSprite','/Game/WarlockArena/Materials',unreal.Material,unreal.MaterialFactoryNew())
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided',True)
    uv=M.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate)
    time=M.create_material_expression(mat,unreal.MaterialExpressionTime)
    tint=M.create_material_expression(mat,unreal.MaterialExpressionVectorParameter)
    tint.set_editor_property('parameter_name','Tint');tint.set_editor_property('default_value',unreal.LinearColor(1,.22,.01,1))
    opacity=M.create_material_expression(mat,unreal.MaterialExpressionScalarParameter)
    opacity.set_editor_property('parameter_name','Opacity');opacity.set_editor_property('default_value',1)
    glow=M.create_material_expression(mat,unreal.MaterialExpressionCustom)
    glow.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    glow.set_editor_property('code','''
float2 p=(UV-.5)*2; float a=T*5; float2 r=float2(p.x*cos(a)-p.y*sin(a),p.x*sin(a)+p.y*cos(a));
float swirl=sin(r.x*7+T*11)*sin(r.y*6-T*9)*.13;
float edge=saturate(1-length(p)+swirl)*saturate((1-length(p)) * 3);
float heat=pow(saturate(1-length(p)*1.65),3);
return (Tint.rgb*edge*2.7+float3(1,.68,.16)*heat*3.5)*Opacity;
''')
    inputs=[]
    for name in ['UV','T','Tint','Opacity']:
        entry=unreal.CustomInput();entry.set_editor_property('input_name',name);inputs.append(entry)
    glow.set_editor_property('inputs',inputs)
    for node,name in [(uv,'UV'),(time,'T'),(tint,'Tint'),(opacity,'Opacity')]:assert M.connect_material_expressions(node,'',glow,name)
    assert M.connect_material_property(glow,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert M.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
unreal.EditorAssetLibrary.save_loaded_asset(unreal.load_asset(path))
{'material':path,'exists':unreal.EditorAssetLibrary.does_asset_exist(path)}
