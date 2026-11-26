"""Compare packaged client captures; same 1600x900 output, uncapped, VSync off."""
import csv,json,math,re,statistics
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
csv.field_size_limit(10**7)
report={'scope':'Short captures with two idle human seats and two active bots (solo has three bots); not identical combat replays or another-PC guarantees. Discard 300 warmup frames.','profiles':{}}
for log in sorted((ROOT/'Saved/Validation').glob('client-*.log')):
    if '-backup-' in log.stem:continue
    text=log.read_text(encoding='utf8',errors='replace')
    match=re.search(r'Capture Ended\. Writing CSV to file : .*?(Profile\([^\n]+?\)\.csv)',text)
    if not match:continue
    path=ROOT/'Saved/Builds/Windows/WarlockArena/Saved/Profiling/CSV'/match[1]
    if not path.exists():continue
    rows=list(csv.reader(path.open(encoding='utf-8-sig')));header=rows[0]
    names=['FrameTime','GameThreadTime','RenderThreadTime','GPUTime'];indices=[header.index(n) for n in names];values=[]
    for row in rows[1:]:
        try:
            v=[float(row[i]) for i in indices]
            if v[0]>.1:values.append(v)
        except (ValueError,IndexError):pass
    samples=values[300:]
    if not samples:continue
    medians={n:round(statistics.median(v[i] for v in samples),3) for i,n in enumerate(names)}
    report['profiles'][log.stem]={'csv':path.name,'frames':len(samples),'median_ms':medians,'approx_fps':round(1000/medians['FrameTime'],1),'p95_frame_ms':round(sorted(v[0] for v in samples)[math.ceil(.95*len(samples))-1],3),'fatal_or_material_fallback':bool(re.search(r'Fatal error|Assertion failed|Default Material will be used|Failed to compile Material',text))}
out=ROOT/'Saved/Validation/client-performance.json';out.write_text(json.dumps(report,indent=2),encoding='utf8');print(json.dumps(report,indent=2))
