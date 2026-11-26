"""Read-only classic MDX -> glTF skin/animation conversion for the supplied map.

No JASS is executed. Original model remains analysis evidence; generated glTF is
an internal comparison asset. Missing base-game textures are NOT fabricated:
named PBR substitute materials are used and recorded in the report.
"""
import bisect
import hashlib
import json
import math
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'Analysis/Warlock1.00b CN/Models/WarlockModel.mdx'
OUT = ROOT / 'AssetsSource/Warlock'


def u32(b, p=0): return struct.unpack_from('<I', b, p)[0]
def vec(b, p, n=3): return list(struct.unpack_from('<'+'f'*n, b, p))
def name(b): return b.split(b'\0')[0].decode('utf8', errors='replace')
def coords(v): return [v[0]/100, v[2]/100, -v[1]/100]
def quaternion(v): return [v[0], v[2], -v[1], v[3]]
def normal(v): return [v[0], v[2], -v[1]]


def track(b, p, dims):
    count, kind, glob = struct.unpack_from('<III', b, p+4)
    p += 16
    keys = []
    for _ in range(count):
        frame = u32(b, p); p += 4
        value = vec(b, p, dims); p += dims*4
        incoming = outgoing = None
        if kind > 1:
            incoming = vec(b, p, dims); p += dims*4
            outgoing = vec(b, p, dims); p += dims*4
        keys.append((frame, value, incoming, outgoing))
    return {'kind': kind, 'global': glob, 'keys': keys}, p


def slerp(a, b, t):
    dot = sum(x*y for x,y in zip(a,b))
    if dot < 0: b = [-x for x in b]; dot = -dot
    dot = min(1, max(-1, dot))
    if dot > .9995: r = [(1-t)*x+t*y for x,y in zip(a,b)]
    else:
        angle = math.acos(dot); s = math.sin(angle)
        r = [(math.sin((1-t)*angle)*x+math.sin(t*angle)*y)/s for x,y in zip(a,b)]
    length = math.sqrt(sum(x*x for x in r)) or 1
    return [x/length for x in r]


def evaluate(tr, frame, start, end, default, rotation=False):
    if not tr: return default
    if tr['global'] != 0xffffffff:
        frame %= GLOBALS[tr['global']]; keys = tr['keys']
    else: keys = [k for k in tr['keys'] if start <= k[0] <= end]
    if not keys: return default
    i = bisect.bisect_right([k[0] for k in keys], frame)-1
    if i < 0: return keys[0][1]
    if i >= len(keys)-1 or tr['kind'] == 0: return keys[i][1]
    a,b = keys[i:i+2]; t = min(1,max(0,(frame-a[0])/(b[0]-a[0])))
    if rotation:
        if tr['kind'] == 1: return slerp(a[1],b[1],t)
        return slerp(slerp(a[1],b[1],t),slerp(a[3],b[2],t),2*t*(1-t))
    if tr['kind'] == 1: return [(1-t)*x+t*y for x,y in zip(a[1],b[1])]
    if tr['kind'] == 2:
        return [(2*t**3-3*t*t+1)*x+(-2*t**3+3*t*t)*y+(t**3-2*t*t+t)*ta+(t**3-t*t)*tb for x,y,ta,tb in zip(a[1],b[1],a[3],b[2])]
    return [(1-t)**3*x+3*(1-t)**2*t*ta+3*(1-t)*t*t*tb+t**3*y for x,y,ta,tb in zip(a[1],b[1],a[3],b[2])]


raw = SOURCE.read_bytes()
assert raw[:4] == b'MDLX'
chunks = {}; p = 4
while p < len(raw):
    tag = raw[p:p+4].decode(); size = u32(raw,p+4)
    chunks[tag] = raw[p+8:p+8+size]; p += 8+size
assert u32(chunks['VERS']) == 800, 'Only the supplied classic MDX version is supported'
GLOBALS = list(struct.unpack('<'+'I'*(len(chunks['GLBS'])//4),chunks['GLBS']))
sequences = []
for p in range(0,len(chunks['SEQS']),132):
    b=chunks['SEQS']; sequences.append({'name':name(b[p:p+80]),'start':u32(b,p+80),'end':u32(b,p+84)})
pivots = [vec(chunks['PIVT'],p) for p in range(0,len(chunks['PIVT']),12)]
nodes = {}
for typ in ['BONE','HELP']:
    b=chunks[typ]; p=0
    while p<len(b):
        size=u32(b,p); ident,parent,flags=struct.unpack_from('<IiI',b,p+84)
        node={'name':name(b[p+4:p+84]),'parent':parent,'flags':flags,'tracks':{}}
        q=p+96
        while q<p+size:
            tag=b[q:q+4].decode(); assert tag in ['KGTR','KGRT','KGSC'],tag
            tr,q=track(b,q,4 if tag=='KGRT' else 3);node['tracks'][tag]=tr
        nodes[ident]=node;p+=size+(8 if typ=='BONE' else 0)
textures=[]
b=chunks['TEXS']
for p in range(0,len(b),268):textures.append({'replaceable':u32(b,p),'path':name(b[p+4:p+264])})
materials=[];b=chunks['MTLS'];p=0
while p<len(b):
    size=u32(b,p);assert b[p+12:p+16]==b'LAYS'
    q=p+20; layers=[]
    for _ in range(u32(b,p+16)):
        z=u32(b,q);layers.append({'filter':u32(b,q+4),'flags':u32(b,q+8),'texture':u32(b,q+12),'alpha':vec(b,q+24,1)[0]});q+=z
    materials.append(layers);p+=size
geo_anim={};b=chunks['GEOA'];p=0
while p<len(b):
    size=u32(b,p);alpha=vec(b,p+4,1)[0];ident=u32(b,p+24);q=p+28;tr=None
    while q<p+size:
        tag=b[q:q+4];a,q=track(b,q,1 if tag==b'KGAO' else 3)
        if tag==b'KGAO':tr=a
    geo_anim[ident]=(alpha,tr);p+=size
geos=[];b=chunks['GEOS'];p=0
while p<len(b):
    size=u32(b,p);q=p+4;g={}
    for tag,dims,fmt in [('VRTX',3,'f'),('NRMS',3,'f'),('PTYP',1,'I'),('PCNT',1,'I'),('PVTX',1,'H'),('GNDX',1,'B'),('MTGC',1,'I'),('MATS',1,'I')]:
        assert b[q:q+4].decode()==tag,(q,b[q:q+4],tag)
        count=u32(b,q+4);q+=8;n=count*dims
        data=list(struct.unpack_from('<'+fmt*n,b,q));q+=struct.calcsize(fmt)*n
        g[tag]=[data[i:i+dims] for i in range(0,n,dims)] if dims>1 else data
    g['material']=u32(b,q);q+=12+28
    count=u32(b,q);q+=4+28*count
    assert b[q:q+4]==b'UVAS';uvcount=u32(b,q+4);q+=8
    for i in range(uvcount):
        assert b[q:q+4]==b'UVBS';count=u32(b,q+4);q+=8
        uv=[vec(b,q+j*8,2) for j in range(count)];q+=count*8
        if i==0:g['uv']=uv
    assert q==p+size,(q,p+size)
    geos.append(g);p+=size

doc={'asset':{'version':'2.0','generator':'WarlockArena source-map comparison converter'},'scene':0,'scenes':[{'nodes':[]}], 'nodes':[], 'meshes':[], 'skins':[], 'materials':[], 'buffers':[], 'bufferViews':[], 'accessors':[], 'animations':[]}
binary=bytearray()


def accessor(values,typ,component=5126,bounds=False):
    dims={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}[typ]
    if dims==1: values=[[v] for v in values]
    while len(binary)%4:binary.append(0)
    offset=len(binary);fmt={5126:'f',5123:'H',5125:'I'}[component]
    binary.extend(struct.pack('<'+fmt*(len(values)*dims),*[x for row in values for x in row]))
    view=len(doc['bufferViews']);doc['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':len(binary)-offset})
    a={'bufferView':view,'componentType':component,'count':len(values),'type':typ}
    if bounds:a.update(min=[min(v[i] for v in values) for i in range(dims)],max=[max(v[i] for v in values) for i in range(dims)])
    doc['accessors'].append(a);return len(doc['accessors'])-1


ids=sorted(nodes);index={id:i for i,id in enumerate(ids)}
for id in ids:
    n=nodes[id];par=n['parent'];base=[pivots[id][i]-(pivots[par][i] if par>=0 else 0) for i in range(3)]
    out={'name':f"{id:02d}_{n['name']}",'translation':coords(base)}
    children=[index[i] for i in ids if nodes[i]['parent']==id]
    if children:out['children']=children
    doc['nodes'].append(out)
    if par<0:doc['scenes'][0]['nodes'].append(index[id])
bind=[]
for id in ids:
    x,y,z=coords(pivots[id]);bind.append([1,0,0,0,0,1,0,0,0,0,1,0,-x,-y,-z,1])
doc['skins'].append({'name':'Warlock_Source_Skeleton','joints':list(range(len(ids))),'inverseBindMatrices':accessor(bind,'MAT4'),'skeleton':index[35]})
report={'source':str(SOURCE.relative_to(ROOT)),'sha256':hashlib.sha256(raw).hexdigest(),'clips':sequences,'bones':len(ids),'geosets':[], 'texture_status':'Base Warcraft III textures absent from supplied MPQ; substituted PBR colors, not original textures','limitations':['Original low-poly geometry; not an HD remake','Billboard/particle geosets and geosets hidden in Stand/Walk excluded','Texture/visibility animation is not transferred','Tracks baked at 30fps; do-not-inherit node flags require further review if present']}
for i,g in enumerate(geos):
    alpha,tr=geo_anim.get(i,(1,None));visible=max(evaluate(tr,2250,1500,3000,[alpha])[0],evaluate(tr,10000,9667,10600,[alpha])[0])
    layers=materials[g['material']];layer=layers[0];tex=textures[layer['texture']]
    info={'index':i,'vertices':len(g['VRTX']),'material':g['material'],'texture':tex,'alpha':visible,'imported':False}
    report['geosets'].append(info)
    if len(g['PVTX'])<3 or visible<.1 or layer['filter']>=3:continue
    path=tex['path'].lower();color=[.095,.045,.035,1];metal=0;rough=.82
    if tex['replaceable']==1:color=[.3,.065,.045,1]
    elif any(x in path for x in ['armorpads','copper','hero_bloodelf']):color=[.31,.2,.075,1];metal=.7;rough=.4
    elif 'sylvanus' in path:color=[.42,.29,.23,1];rough=.65
    elif 'black32' in path:color=[.012,.015,.018,1]
    elif any(x in path for x in ['glow','eyes','water','cloud']):color=[.04,.28,.35,1];metal=.15;rough=.3
    mat={'name':f"PBR_{i:02d}_{Path(tex['path'].replace(chr(92),'/')).stem or 'TeamCloth'}",'pbrMetallicRoughness':{'baseColorFactor':color,'metallicFactor':metal,'roughnessFactor':rough},'doubleSided':True,'extras':{'missing_original_texture':tex['path']}}
    material_id=len(doc['materials']);doc['materials'].append(mat)
    groups=[];offset=0
    for count in g['MTGC']:groups.append(g['MATS'][offset:offset+count]);offset+=count
    joints=[];weights=[];joints2=[];weights2=[]
    for group in g['GNDX']:
        influences=groups[group];assert len(influences)<=8,influences
        assert all(id in index for id in influences),influences
        ji=[index[id] for id in influences]+[0]*(8-len(influences));we=[1/len(influences)]*len(influences)+[0]*(8-len(influences))
        joints.append(ji[:4]);weights.append(we[:4]);joints2.append(ji[4:]);weights2.append(we[4:])
    attributes={'POSITION':accessor([coords(v) for v in g['VRTX']],'VEC3',bounds=True),'NORMAL':accessor([normal(v) for v in g['NRMS']],'VEC3'),'TEXCOORD_0':accessor(g['uv'],'VEC2'),'JOINTS_0':accessor(joints,'VEC4',5123),'WEIGHTS_0':accessor(weights,'VEC4')}
    if any(sum(w)>0 for w in weights2):attributes.update(JOINTS_1=accessor(joints2,'VEC4',5123),WEIGHTS_1=accessor(weights2,'VEC4'))
    # Combine all visible geosets into one skeletal mesh, retaining material slots.
    if not doc['meshes']:doc['meshes'].append({'name':'Warlock_Source','primitives':[]})
    doc['meshes'][0]['primitives'].append({'attributes':attributes,'indices':accessor(g['PVTX'],'SCALAR',5123),'material':material_id})
    info['imported']=True
mesh_node=len(doc['nodes']);doc['nodes'].append({'name':'Warlock','mesh':0,'skin':0});doc['scenes'][0]['nodes'].append(mesh_node)
for seq in sequences:
    if seq['name'] not in ['Stand','Walk','Attack','Spell','Death','Attack Slam']:continue
    anim={'name':seq['name'].replace(' ','_'),'samplers':[],'channels':[]}
    duration=(seq['end']-seq['start'])/1000;steps=max(1,math.ceil(duration*30));times=[duration*i/steps for i in range(steps+1)]
    time_accessor=accessor(times,'SCALAR',bounds=True)
    for id in ids:
        n=nodes[id];par=n['parent'];base=[pivots[id][i]-(pivots[par][i] if par>=0 else 0) for i in range(3)]
        for tag,path,default in [('KGTR','translation',[0,0,0]),('KGRT','rotation',[0,0,0,1]),('KGSC','scale',[1,1,1])]:
            tr=n['tracks'].get(tag)
            if not tr:continue
            values=[]
            for t in times:
                v=evaluate(tr,seq['start']+t*1000,seq['start'],seq['end'],default,tag=='KGRT')
                if tag=='KGTR':v=coords([v[i]+base[i] for i in range(3)])
                elif tag=='KGRT':v=quaternion(v)
                else:v=[v[0],v[2],v[1]]
                values.append(v)
            sampler=len(anim['samplers']);anim['samplers'].append({'input':time_accessor,'output':accessor(values,'VEC4' if tag=='KGRT' else 'VEC3'),'interpolation':'LINEAR'})
            anim['channels'].append({'sampler':sampler,'target':{'node':index[id],'path':path}})
    doc['animations'].append(anim)
doc['buffers'].append({'byteLength':len(binary)})
js=json.dumps(doc,separators=(',',':')).encode()
js+=b' '*((-len(js))%4);binary+=b'\0'*((-len(binary))%4)
glb=struct.pack('<4sII',b'glTF',2,12+8+len(js)+8+len(binary))+struct.pack('<I4s',len(js),b'JSON')+js+struct.pack('<I4s',len(binary),b'BIN\0')+binary
OUT.mkdir(parents=True,exist_ok=True)
(OUT/'Warlock.glb').write_bytes(glb)
(OUT/'conversion.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps({'output':str(OUT/'Warlock.glb'),'bytes':len(glb),'bones':len(ids),'visible_geosets':sum(x['imported'] for x in report['geosets']),'clips':[x['name'] for x in doc['animations']]},ensure_ascii=False))
