"""Owned, derivative-filtered soft guide. One inexpensive local-only mesh per player."""
import unreal
E=unreal.EditorAssetLibrary;M=unreal.MaterialEditingLibrary
path='/Game/WarlockArena/Materials/M_AimGuide'
if not E.does_asset_exist(path):
    mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_AimGuide','/Game/WarlockArena/Materials',unreal.Material,unreal.MaterialFactoryNew())
    mat.set_editor_properties({'blend_mode':unreal.BlendMode.BLEND_TRANSLUCENT,'shading_model':unreal.MaterialShadingModel.MSM_UNLIT,'two_sided':True})
    def node(cls,**props):
        n=M.create_material_expression(mat,cls)
        if props:n.set_editor_properties(props)
        return n
    sources={'UV':node(unreal.MaterialExpressionTextureCoordinate),'Shape':node(unreal.MaterialExpressionScalarParameter,parameter_name='Shape',default_value=0),'Opacity':node(unreal.MaterialExpressionScalarParameter,parameter_name='Opacity',default_value=1),'Tint':node(unreal.MaterialExpressionVectorParameter,parameter_name='Tint',default_value=unreal.LinearColor(.36,.63,.68,1))}
    common='''float x=UV.x,y=abs(UV.y-.5)*2;float aa=max(fwidth(y),.015);float fill=0,rim=0;
if(Shape<.5){float tip=1-smoothstep(.84,1,x);float w=lerp(.80,.05,saturate((x-.82)/.18));fill=(1-smoothstep(w-aa,w+aa,y));rim=exp(-abs(y-w)*55)+exp(-y*85)*.35;rim*=1-smoothstep(.98,1,x);}
else if(Shape<1.5){float w=max(.01,x*.95);fill=(1-smoothstep(w-aa,w+aa,y));rim=exp(-abs(y-w)*65)+exp(-abs(x-.98)*120)*fill;}
else{float r=length((UV-.5)*2);aa=max(fwidth(r),.007);fill=1-smoothstep(.96-aa,.96+aa,r);rim=exp(-abs(r-.94)*90);float a=atan2(UV.y-.5,UV.x-.5);rim*=.72+.28*pow(saturate(cos(a*8)),8);}
float edge=smoothstep(0,.025,x)*(1-smoothstep(.99,1,x));
'''
    for prop,code,typ in [(unreal.MaterialProperty.MP_EMISSIVE_COLOR,'return Tint.rgb;',unreal.CustomMaterialOutputType.CMOT_FLOAT3),(unreal.MaterialProperty.MP_OPACITY,'return saturate(fill*.055+rim*.42)*Opacity*edge;',unreal.CustomMaterialOutputType.CMOT_FLOAT1)]:
        c=node(unreal.MaterialExpressionCustom,output_type=typ);inputs=[]
        for name in sources:
            i=unreal.CustomInput();i.set_editor_property('input_name',name);inputs.append(i)
        c.set_editor_properties({'inputs':inputs,'code':common+code})
        for name,source in sources.items():assert M.connect_material_expressions(source,'',c,name)
        assert M.connect_material_property(c,'',prop)
    M.recompile_material(mat);assert E.save_loaded_asset(mat)
{'guide':path}
