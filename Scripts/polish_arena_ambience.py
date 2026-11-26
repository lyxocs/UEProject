"""UE Python: replace only the owned lava shader, without deleting expressions.

Idempotent versioned visual update; does not alter terrain or gameplay bounds.
The existing Time input drives the current and cooling crust.
"""
import unreal

asset=unreal.EditorAssetLibrary.load_asset('/Game/WarlockArena/Materials/M_Lava')
assert asset
version='broken-crust-v1'
changed=0
if unreal.EditorAssetLibrary.get_metadata_tag(asset,'WarlockAmbience')!=version:
    for node in unreal.ObjectIterator(unreal.MaterialExpressionCustom):
        if node.get_outer()!=asset or 'float heat=' not in node.get_editor_property('code'):
            continue
        node.set_editor_property('code','''
// Slow broad current, locally warped to break the polygonal cell edges.
float2 uv=P.xy/190;
uv+=float2(sin(P.y*.006+T*.045),cos(P.x*.005-T*.035))*.42;
uv+=float2(sin(P.y*.047+sin(P.x*.021)*2),cos(P.x*.039+P.y*.013))*.12;
float2 cell=floor(uv),f=frac(uv);float d0=4,d1=4,id=0;
for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++){
float2 c=cell+float2(x,y);
float2 h=frac(sin(float2(dot(c,float2(127.1,311.7)),dot(c,float2(269.5,183.3))))*43758.5453);
float d=length(float2(x,y)+h*.8+.1-f);
if(d<d0){d1=d0;d0=d;id=h.x;}else d1=min(d1,d);}
float edge=d1-d0;
// Layered value noise supplies uneven crust and varying hot seam width.
float2 np=P.xy*.018;float grain=0,amp=.6;
for(int oct=0;oct<3;oct++){
float2 ip=floor(np),ff=frac(np);ff=ff*ff*(3-2*ff);
float4 h=frac(sin(float4(dot(ip,float2(127.1,311.7)),dot(ip+float2(1,0),float2(127.1,311.7)),dot(ip+float2(0,1),float2(127.1,311.7)),dot(ip+1,float2(127.1,311.7))))*43758.5453);
grain+=lerp(lerp(h.x,h.y,ff.x),lerp(h.z,h.w,ff.x),ff.y)*amp;np=np*2.03+17.1;amp*=.5;}
float current=.5+.5*sin(P.x*.008+P.y*.005+sin(P.y*.012+T*.08)*2+T*.11);
float width=lerp(.009,.043,saturate(grain));
float heat=1-smoothstep(width*.12,width,edge);
float cooling=smoothstep(.24,.62,grain+current*.13);
float core=pow(heat,4)*cooling;
float bank=exp(-edge*32)*(.12+grain*.2);
float3 crust=float3(.012,.009,.008)*(.55+grain*.5);
return crust+float3(.30,.028,.0025)*(heat*(.2+.8*cooling)+bank)
     +float3(.62,.16,.018)*core*(.65+.35*current);
''')
        changed+=1
    assert changed==1,changed
    unreal.EditorAssetLibrary.set_metadata_tag(asset,'WarlockAmbience',version)
    # NullRHI saves the graph; the normal cook performs shader compilation.
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
{'updated_lava_nodes':changed,'version':version}
