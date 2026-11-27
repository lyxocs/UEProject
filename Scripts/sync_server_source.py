"""Create a source-only build archive; excludes private settings and generated data."""
from pathlib import Path
import tarfile
ROOT=Path(__file__).resolve().parents[1]
output=ROOT/'Saved/Distribution/server-project-source.tar.gz'
output.parent.mkdir(parents=True,exist_ok=True)
with tarfile.open(output,'w:gz') as archive:
    for base in ['WarlockArena.uproject','Source','Config']:
        archive.add(ROOT/base,arcname='WarlockArena/'+base)
print(output)
