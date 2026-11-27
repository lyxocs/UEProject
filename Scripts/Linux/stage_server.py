"""Copy a source-built monolithic server and its receipt-declared runtime files."""
import argparse,json,shutil,stat
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--engine',type=Path,required=True);p.add_argument('--project',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);a=p.parse_args()
engine=a.engine.resolve();project=a.project.resolve();runtime=a.runtime.resolve();runtime.mkdir(parents=True,exist_ok=True)
receipt=json.loads((project/'Binaries/Linux/WarlockArenaServer.target').read_text())
assert receipt['TargetType']=='Server' and receipt['Platform']=='Linux', 'Expected a genuine Linux Server target'
copied=[]
def stage(source):
    source=Path(str(source).replace('$(EngineDir)',str(engine/'Engine')).replace('$(ProjectDir)',str(project)))
    if not source.exists():raise FileNotFoundError(source)
    if source.suffix in {'.debug','.sym','.pdb'}:return
    if source.is_relative_to(engine/'Engine'):relative=Path('Engine')/source.relative_to(engine/'Engine')
    elif source.is_relative_to(project):relative=Path('WarlockArena')/source.relative_to(project)
    else:raise ValueError(f'Unexpected external runtime dependency: {source}')
    target=runtime/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target);copied.append(relative.as_posix())
stage(project/'Binaries/Linux/WarlockArenaServer')
for config in (project/'Config').rglob('*.ini'):stage(config)
for dep in receipt.get('RuntimeDependencies',[]):
    if dep.get('Type')!='DebugNonUFS':stage(dep['Path'])
# Enabled plugin manifests/descriptors determine statically linked startup modules.
enabled=set(receipt.get('BuildPlugins',[]))
for descriptor in (engine/'Engine/Plugins').rglob('*.uplugin'):
    if descriptor.stem not in enabled:continue
    stage(descriptor)
    config=descriptor.parent/'Config'
    if config.exists():
        for file in config.rglob('*.ini'):stage(file)
print(json.dumps({'runtime':str(runtime),'files_copied':len(copied)},indent=2))
