"""Filter subpixel procedural detail through UE; preserve geometry and material wiring."""
import json
from pathlib import Path
import unreal

E=unreal.EditorAssetLibrary
M=unreal.MaterialEditingLibrary
VERSION='screen-filter-v1'
report=[]
codes={
    'M_SorcererCloth_Natural':'''float2 footprint=fwidth(UV);
float weaveFade=1-smoothstep(.6,2.4,max(footprint.x*700,footprint.y*700));
float brocadeFade=1-smoothstep(.35,1.2,max(footprint.x*120,footprint.y*90));
float weave=sin(UV.x*700)*sin(UV.y*700)*weaveFade;
float brocade=pow(saturate(sin(UV.x*120)*sin(UV.y*90)),12)*brocadeFade;
return Tint.rgb*(.91+.02*weave+.05*brocade);''',
    'M_SorcererMetal_Natural':'''float2 footprint=fwidth(UV);
float fade=1-smoothstep(.6,2.4,max(footprint.x*300,footprint.y*130));
float scratch=sin(UV.x*300+sin(UV.y*40))*sin(UV.y*130)*fade;
return Tint.rgb*(.94+.04*scratch);''',
    'M_SorcererSkin_Natural':'''float2 footprint=fwidth(UV);
float fade=1-smoothstep(.6,2.4,max(footprint.x*70,footprint.y*90));
return Tint.rgb*(.98+.015*sin(UV.x*70)*sin(UV.y*90)*fade);'''
}
for name,code in codes.items():
    mat=unreal.load_asset('/Game/WarlockArena/Characters/Materials/'+name)
    assert mat and mat.get_editor_property('blend_mode')==unreal.BlendMode.BLEND_OPAQUE,name
    nodes=[n for n in unreal.ObjectIterator(unreal.MaterialExpressionCustom)
           if n.get_path_name().startswith(mat.get_path_name()) and 'return Tint.rgb*(' in n.get_editor_property('code')]
    assert len(nodes)==1,(name,len(nodes))
    changed=nodes[0].get_editor_property('code')!=code
    if changed:
        nodes[0].set_editor_property('code',code)
        E.set_metadata_tag(mat,'WarlockClarity',VERSION)
        M.recompile_material(mat)
        assert E.save_loaded_asset(mat)
    report.append({'material':mat.get_path_name(),'changed':changed,'opaque':True,'code':nodes[0].get_editor_property('code')})

stone=unreal.load_asset('/Game/WarlockArena/Materials/M_Stone')
nodes=[n for n in unreal.ObjectIterator(unreal.MaterialExpressionCustom)
       if n.get_path_name().startswith(stone.get_path_name()) and 'return Tint.rgb*ThemeTint.rgb*' in n.get_editor_property('code')]
assert len(nodes)==1,len(nodes)
before=nodes[0].get_editor_property('code')
# Only filter base color. The normal node already uses derivatives of displacement;
# feeding fwidth into that node would introduce nested derivatives.
after=before.replace('float joint=smoothstep(.004,.017,edge);',
    'float jointAA=max(fwidth(edge),.002);float joint=smoothstep(.004-jointAA*.5,.017+jointAA*.5,edge);')
assert 'float jointAA=' in after
if after!=before:
    nodes[0].set_editor_property('code',after)
    E.set_metadata_tag(stone,'WarlockClarity',VERSION)
    M.recompile_material(stone)
    assert E.save_loaded_asset(stone)
report.append({'material':stone.get_path_name(),'changed':after!=before,'base_color_only':True})
path=Path(unreal.Paths.project_saved_dir(),'Validation','material-clarity.json')
path.write_text(json.dumps({'version':VERSION,'materials':report},indent=2),encoding='utf8')
{'materials':len(report),'changed':sum(x['changed'] for x in report),'report':str(path)}
