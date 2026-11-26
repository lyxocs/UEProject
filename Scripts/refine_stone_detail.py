"""UE editor Python: update owned shader nodes without deleting rooted expressions."""
import unreal

E=unreal.EditorAssetLibrary
mat=E.load_asset('/Game/WarlockArena/Materials/M_Stone')
old='float n=.5+.22*sin(P.x*.13+sin(P.y*.07)*2)+.13*sin(P.y*.29+P.x*.21)+.08*sin(P.x*1.9)*sin(P.y*2.3);'
new='''
float2 np=float2(P.x+P.z*.37,P.y+P.z*.21)*.12;float ns=0,amp=.65;
for(int oct=0;oct<2;oct++){
float2 ip=floor(np),f=frac(np);f=f*f*(3-2*f);
float4 h=frac(sin(float4(dot(ip,float2(127.1,311.7)),dot(ip+float2(1,0),float2(127.1,311.7)),dot(ip+float2(0,1),float2(127.1,311.7)),dot(ip+1,float2(127.1,311.7))))*43758.5453);
ns+=lerp(lerp(h.x,h.y,f.x),lerp(h.z,h.w,f.x),f.y)*amp;np=np*2.03+17.1;amp*=.5;}
float n=.2+ns;
'''
changed=0
if E.get_metadata_tag(mat,'WarlockStoneDetail')!='mottled-v1':
    for node in unreal.ObjectIterator(unreal.MaterialExpressionCustom):
        if not node.get_path_name().startswith(mat.get_path_name()):continue
        code=node.get_editor_property('code')
        if old in code:
            node.set_editor_property('code',code.replace(old,new));changed+=1
    assert changed==3, f'Expected three owned stone nodes, got {changed}'
    E.set_metadata_tag(mat,'WarlockStoneDetail','mottled-v1')
    E.save_directory('/Game/WarlockArena/Materials',only_if_is_dirty=True)
normal_fixed=0
if E.get_metadata_tag(mat,'WarlockNormalOrientation')!='mesh-v1':
    normal=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionVertexNormalWS)
    for node in unreal.ObjectIterator(unreal.MaterialExpressionCustom):
        if not node.get_path_name().startswith(mat.get_path_name()):continue
        code=node.get_editor_property('code')
        if 'float3 q=P+float3(0,0,h)' not in code:continue
        entry=unreal.CustomInput();entry.set_editor_property('input_name','N')
        inputs=list(node.get_editor_property('inputs'));inputs.append(entry);node.set_editor_property('inputs',inputs)
        code=code.replace('float3 q=P+float3(0,0,h)','float3 q=P+normalize(N)*h').replace('return normal.z<0?-normal:normal;','return dot(normal,N)<0?-normal:normal;')
        node.set_editor_property('code',code)
        assert unreal.MaterialEditingLibrary.connect_material_expressions(normal,'',node,'N')
        normal_fixed+=1
    assert normal_fixed==1,normal_fixed
    E.set_metadata_tag(mat,'WarlockNormalOrientation','mesh-v1')
    E.save_directory('/Game/WarlockArena/Materials',only_if_is_dirty=True)
{'stone_nodes_updated':changed,'normals_fixed':normal_fixed,'detail':'non-repeating mineral detail; stable normals on both floor and vertical sides'}
