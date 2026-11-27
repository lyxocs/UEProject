"""Stage loose LinuxServer cook and required runtime data, never Zen credentials."""
from pathlib import Path
import tarfile
ROOT=Path(__file__).resolve().parents[1]
import argparse
p=argparse.ArgumentParser();p.add_argument('--engine',type=Path,required=True);a=p.parse_args()
cooked=ROOT/'Saved/Cooked/LinuxServer';assert (cooked/'WarlockArena/Content/WarlockArena/Maps/Arena.umap').exists()
output=ROOT/'Saved/Distribution/linux-server-resources.tar.gz';output.parent.mkdir(parents=True,exist_ok=True)
with tarfile.open(output,'w:gz') as archive:
    for top in ['Engine/Content','Engine/Plugins','WarlockArena/Content','WarlockArena/AssetRegistry.bin']:
        archive.add(cooked/top,arcname=top)
    archive.add(ROOT/'WarlockArena.uproject',arcname='WarlockArena/WarlockArena.uproject')
    archive.add(ROOT/'Config',arcname='WarlockArena/Config')
    archive.add(ROOT/'Content/WarlockArena/AI',arcname='WarlockArena/Content/WarlockArena/AI')
    for folder in ['Internationalization','Slate']:
        archive.add(a.engine/'Engine/Content'/folder,arcname='Engine/Content/'+folder)
    for file in (a.engine/'Engine/Config').glob('Base*.ini'):archive.add(file,arcname='Engine/Config/'+file.name)
    # NullRHI still initializes a shader-platform description on a dedicated
    # server. Linux inherits Unix and VulkanPC metadata; include Windows too
    # for the stock NullRHI's SM5 fallback description.
    for name in ['Linux','LinuxServer','Unix','VulkanPC','Windows']:
        source=a.engine/'Engine/Config'/name
        if source.exists():archive.add(source,arcname='Engine/Config/'+name)
print(str(output),output.stat().st_size)
