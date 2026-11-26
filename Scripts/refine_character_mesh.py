"""Derive a smoother cloth mesh while retaining source bones, UVs and animation.

Original MDX and comparison GLB remain untouched. This is subdivision/detail
work on the comparison model, not a claim of a newly sculpted HD character.
"""
import argparse
import copy
import json
import math
import struct
from pathlib import Path

parser=argparse.ArgumentParser()
parser.add_argument('--polished',action='store_true')
polished=parser.parse_args().polished
ROOT=Path(__file__).resolve().parents[1]
source=ROOT/'AssetsSource/Warlock/Warlock.glb'
raw=source.read_bytes();size=struct.unpack_from('<I',raw,12)[0]
doc=json.loads(raw[20:20+size]);offset=20+size
binary=bytearray(raw[offset+8:offset+8+struct.unpack_from('<I',raw,offset)[0]])

def read(index):
    a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']]
    dims={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}[a['type']]
    fmt={5126:'f',5123:'H',5125:'I'}[a['componentType']]
    stride=v.get('byteStride',struct.calcsize(fmt)*dims);base=v.get('byteOffset',0)+a.get('byteOffset',0)
    result=[list(struct.unpack_from('<'+fmt*dims,binary,base+i*stride)) for i in range(a['count'])]
    return [r[0] for r in result] if dims==1 else result

def write(values,kind,component=5126,bounds=False):
    while len(binary)%4:binary.append(0)
    dims={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[kind];rows=[[x] for x in values] if dims==1 else values
    start=len(binary);fmt={5126:'f',5123:'H',5125:'I'}[component]
    binary.extend(struct.pack('<'+fmt*len(rows)*dims,*[x for row in rows for x in row]))
    view=len(doc['bufferViews']);doc['bufferViews'].append({'buffer':0,'byteOffset':start,'byteLength':len(binary)-start})
    a={'bufferView':view,'componentType':component,'count':len(rows),'type':kind}
    if bounds:a.update(min=[min(row[k] for row in rows) for k in range(dims)],max=[max(row[k] for row in rows) for k in range(dims)])
    doc['accessors'].append(a);return len(doc['accessors'])-1

def blend_skin(parts):
    result={}
    for skin,weight in parts:
        for bone,value in skin.items():result[bone]=result.get(bone,0)+value*weight
    result=dict(sorted(result.items(),key=lambda p:p[1],reverse=True)[:8]);total=sum(result.values())
    return {k:v/total for k,v in result.items() if v>1e-7}

def weighted(parts):return [sum(p[k]*w for p,w in parts) for k in range(3)]

def subdivide(vertices,triangles):
    # Topology is welded by position; UV seams keep separate attribute vertices.
    unique={};weld=[];positions=[];skins=[]
    for vertex in vertices:
        key=tuple(round(x,6) for x in vertex['p'])
        if key not in unique:unique[key]=len(positions);positions.append(vertex['p']);skins.append(vertex['skin'])
        weld.append(unique[key])
    neighbors=[set() for _ in positions];opposites={}
    for tri in triangles:
        a,b,c=[weld[x] for x in tri]
        for x,y,z in [(a,b,c),(b,c,a),(c,a,b)]:
            if x==y:continue
            neighbors[x].add(y);neighbors[y].add(x);opposites.setdefault(tuple(sorted((x,y))),set()).add(z)
    boundary=set(x for edge,opps in opposites.items() if len(opps)!=2 for x in edge)
    smoothed=[]
    for i,p in enumerate(positions):
        n=len(neighbors[i])
        if i in boundary or n<3:smoothed.append(p);continue
        beta=3/16 if n==3 else 3/(8*n)
        q=weighted([(p,1-n*beta)]+[(positions[j],beta) for j in neighbors[i]])
        # Limit movement of the source silhouette and keep garments off the body.
        delta=[(q[k]-p[k])*.35 for k in range(3)];length=math.sqrt(sum(x*x for x in delta));factor=min(1,.03/max(length,1e-9))
        smoothed.append([p[k]+delta[k]*factor for k in range(3)])
    output=[{'p':smoothed[weld[i]],'uv':v['uv'],'skin':v['skin']} for i,v in enumerate(vertices)]
    edges={}
    def midpoint(a,b):
        key=tuple(sorted((a,b)))
        if key in edges:return edges[key]
        u,v=weld[a],weld[b];opp=opposites.get(tuple(sorted((u,v))),set())
        parts=[(positions[u],.5),(positions[v],.5)];skin=[(vertices[a]['skin'],.5),(vertices[b]['skin'],.5)]
        if len(opp)==2:
            parts=[(positions[u],.375),(positions[v],.375)]+[(positions[j],.125) for j in opp]
            skin=[(vertices[a]['skin'],.375),(vertices[b]['skin'],.375)]+[(skins[j],.125) for j in opp]
        edges[key]=len(output)
        output.append({'p':weighted(parts),'uv':[(vertices[a]['uv'][k]+vertices[b]['uv'][k])*.5 for k in range(2)],'skin':blend_skin(skin)})
        return edges[key]
    faces=[]
    for a,b,c in triangles:
        ab,bc,ca=midpoint(a,b),midpoint(b,c),midpoint(c,a)
        faces.extend([(a,ab,ca),(ab,b,bc),(ca,bc,c),(ab,bc,ca)])
    return output,faces

report=[]
for primitive in doc['meshes'][0]['primitives']:
    material=doc['materials'][primitive['material']]['name']
    if not polished and not any(x in material for x in ['Medivh','TeamCloth','BloodPriest','Black32']):continue
    attrs=primitive['attributes'];ps=read(attrs['POSITION']);uv=read(attrs['TEXCOORD_0']);js=read(attrs['JOINTS_0']);ws=read(attrs['WEIGHTS_0'])
    if 'JOINTS_1' in attrs:
        js=[a+b for a,b in zip(js,read(attrs['JOINTS_1']))];ws=[a+b for a,b in zip(ws,read(attrs['WEIGHTS_1']))]
    vertices=[{'p':p,'uv':t,'skin':{j:w for j,w in zip(bones,weights) if w>0}} for p,t,bones,weights in zip(ps,uv,js,ws)]
    indices=read(primitive['indices']);faces=[tuple(indices[i:i+3]) for i in range(0,len(indices),3)]
    before=len(faces)
    iterations=2 if any(x in material for x in ['Medivh','TeamCloth','BloodPriest','Black32','Hero_BloodElf']) else 1
    for _ in range(iterations):vertices,faces=subdivide(vertices,faces)
    sums={}
    for a,b,c in faces:
        p,q,r=[vertices[i]['p'] for i in (a,b,c)];u=[q[k]-p[k] for k in range(3)];v=[r[k]-p[k] for k in range(3)]
        n=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]
        for i in (a,b,c):
            key=tuple(round(x,6) for x in vertices[i]['p']);old=sums.setdefault(key,[0,0,0])
            for k in range(3):old[k]+=n[k]
    normals=[];joints=[];weights=[]
    for vertex in vertices:
        n=sums.get(tuple(round(x,6) for x in vertex['p']),[0,1,0]);length=math.sqrt(sum(x*x for x in n));normals.append([x/max(length,1e-8) for x in n])
        skin=vertex['skin'];assert abs(sum(skin.values())-1)<1e-4
        joints.append(list(skin)+[0]*(8-len(skin)));weights.append(list(skin.values())+[0]*(8-len(skin)))
    primitive['attributes']={'POSITION':write([v['p'] for v in vertices],'VEC3',bounds=True),'NORMAL':write(normals,'VEC3'),'TEXCOORD_0':write([v['uv'] for v in vertices],'VEC2'),'JOINTS_0':write([j[:4] for j in joints],'VEC4',5123),'WEIGHTS_0':write([w[:4] for w in weights],'VEC4'),'JOINTS_1':write([j[4:] for j in joints],'VEC4',5123),'WEIGHTS_1':write([w[4:] for w in weights],'VEC4')}
    primitive['indices']=write([i for f in faces for i in f],'SCALAR',5125)
    report.append({'material':material,'triangles_before':before,'triangles_after':len(faces),'vertices_after':len(vertices)})
doc['asset']['generator']='WarlockArena cloth refinement; original skeleton/animation preserved'
doc['buffers'][0]['byteLength']=len(binary)
header=json.dumps(doc,separators=(',',':')).encode();header+=b' '*((-len(header))%4);binary+=b'\0'*((-len(binary))%4)
out=ROOT/('AssetsSource/Warlock/Polished' if polished else 'AssetsSource/Warlock/Refined');out.mkdir(parents=True,exist_ok=True)
(out/'Warlock.glb').write_bytes(struct.pack('<4sII',b'glTF',2,28+len(header)+len(binary))+struct.pack('<I4s',len(header),b'JSON')+header+struct.pack('<I4s',len(binary),b'BIN\0')+binary)
(out/'refinement.json').write_text(json.dumps(report,indent=2),encoding='utf8')
print(json.dumps({'refined_sections':len(report),'triangles_before':sum(x['triangles_before'] for x in report),'triangles_after':sum(x['triangles_after'] for x in report),'output':str(out/'Warlock.glb')}))
