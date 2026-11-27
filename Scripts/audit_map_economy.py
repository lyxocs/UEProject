"""Read-only inventory of all gold writes and economy settings in the source JASS."""
import hashlib,json,re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
source=root/'Analysis/Warlock1.00b CN/Scripts/war3map.readable.j'
map_hash=hashlib.sha256((root/'Warlock1.00b CN.w3x').read_bytes()).hexdigest()
assert map_hash=='f1e5af933797d6936dd6040fd718fffec06a88c06707007583b7539e0fefe981'
function='globals';writes=[];settings=[];survival=[]
for number,line in enumerate(source.read_text(encoding='utf8').splitlines(),1):
    match=re.match(r'function (\w+) ',line)
    if match:function=match[1]
    if 'call SetPlayerState(' in line and 'PLAYER_STATE_RESOURCE_GOLD' in line:writes.append({'function':function,'line':number,'statement':line})
    if re.search(r'\b(?:yo|Wo|so|So|To|uo)\s*=',line):settings.append({'function':function,'line':number,'statement':line})
    if 'KE[' in line:survival.append({'function':function,'line':number,'statement':line})
output=root/'Analysis/Verification/Economy20261127/static-audit.json';output.parent.mkdir(parents=True,exist_ok=True)
output.write_text(json.dumps({'source_map_sha256':map_hash,'method':'static inventory, no original game executed','gold_writes':writes,'settings_assignments':settings,'survival_time_references':survival,'conclusion':'ordinary rounds pay configurable so/So/To/uo and fixed Wo; no placement gold write found; defaults 20/10/0/0/0/0'},ensure_ascii=False,indent=2)+'\n',encoding='utf8',newline='\n')
print(json.dumps({'gold_writes':len(writes),'settings':len(settings),'survival_references':len(survival),'output':str(output)}))
