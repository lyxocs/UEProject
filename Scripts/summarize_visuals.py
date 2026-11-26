"""Summarize actual PNG dimensions, package warnings and short CSV frame samples."""
import csv
import json
import math
import re
import statistics
import struct
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
folder=ROOT/'Saved/Validation'
csv.field_size_limit(10**7)
report={'scope':'short CSV samples and controlled/combat screenshots; not sustained combat performance acceptance','older_samples':'native/split CSVs precede second spell/character polish; splitfight CSV is recorded separately','views':{}}
for view in ['native','split','ready','shop','crimson','fight','splitfight','keyboardmousefight','pause','splitpause','fireball']+['spell-'+str(i) for i in range(8)]:
    png=folder/f'visual-{view}.png';log=folder/f'visual-{view}.log'
    if not png.exists() or not log.exists():continue
    dimensions=struct.unpack('>II',png.read_bytes()[16:24]);text=log.read_text(encoding='utf8',errors='replace')
    entry={'output':list(dimensions),'material_fallback':bool(re.search(r'missing usage flag|Default Material will be used|Failed to compile Material',text)),'fatal':bool(re.search(r'Assertion failed|Fatal error|=== Critical error',text))}
    if view.startswith('spell-'):entry['scenario']='controlled skill presentation; not natural combat'
    match=re.search(r'Capture Ended\. Writing CSV to file : .*?(Profile\([^\n]+?\)\.csv)',text)
    if match:
        path=ROOT/'Saved/Builds/Windows/WarlockArena/Saved/Profiling/CSV'/match[1]
        if path.exists() and path.stat().st_size:
            rows=list(csv.reader(path.open(encoding='utf-8-sig')));header=rows[0]
            names=['FrameTime','GameThreadTime','RenderThreadTime','GPUTime'];indices=[header.index(n) for n in names];values=[]
            for row in rows[1:]:
                try:
                    sample=[float(row[i]) for i in indices]
                    if sample[0]>.1:values.append(sample)
                except (ValueError,IndexError):pass
            samples=values[300:] if view=='splitfight' and len(values)>600 else values[60:] if len(values)>100 else values
            if samples:
                entry['frame_sample']={'csv':path.name,'discarded_start_frames':len(values)-len(samples),'frames':len(samples),'median_ms':{name:round(statistics.median(v[i] for v in samples),2) for i,name in enumerate(names)},'p95_frame_ms':round(sorted(v[0] for v in samples)[math.ceil(len(samples)*.95)-1],2)}
    report['views'][view]=entry
(folder/'visual-report.json').write_text(json.dumps(report,indent=2),encoding='utf8')
print(json.dumps(report,ensure_ascii=False))
