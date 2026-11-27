"""Package only the runnable Linux server, with hashes; excludes builds and saves."""
import argparse,hashlib,json,tarfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--base',type=Path,required=True);p.add_argument('--version',required=True);p.add_argument('--commit',required=True);a=p.parse_args()
base=a.base.resolve();runtime=base/'runtime';output=base/'releases'/('WarlockArena-LinuxServer-'+a.version+'.tar.gz');output.parent.mkdir(parents=True,exist_ok=True)
if output.exists():raise FileExistsError('Use a new release version')
files={}
for top in ['Engine','WarlockArena']:
    for file in sorted((runtime/top).rglob('*')):
        if not file.is_file():continue
        rel=file.relative_to(runtime)
        if 'Saved' in rel.parts or file.suffix in {'.sym','.debug','.pdb','.log'} or file.name.startswith('core'):continue
        files[rel.as_posix()]={'size':file.stat().st_size,'sha256':hashlib.file_digest(file.open('rb'),'sha256').hexdigest()}
manifest={'game':'WarlockArena','target':'Linux Dedicated Server','version':a.version,'source_commit':a.commit,'protocol':'0x574C1002','files':files}
manifestfile=output.parent/'linux-server-BUILD-MANIFEST.json';manifestfile.write_text(json.dumps(manifest,indent=2),encoding='utf8')
with tarfile.open(output,'x:gz') as archive:
    for rel in files:archive.add(runtime/rel,arcname='WarlockArenaServer/runtime/'+rel)
    for script in ['server_ctl.sh','run_server.sh']:archive.add(base/script,arcname='WarlockArenaServer/'+script)
    archive.add(manifestfile,arcname='WarlockArenaServer/BUILD-MANIFEST.json')
checksum=hashlib.file_digest(output.open('rb'),'sha256').hexdigest();output.with_suffix(output.suffix+'.sha256.txt').write_text(checksum+'  '+output.name+'\n')
print(json.dumps({'archive':str(output),'size':output.stat().st_size,'sha256':checksum,'files':len(files),'source_commit':a.commit},indent=2))
