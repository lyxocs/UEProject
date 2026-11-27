"""Verify authored PCM and actual UE-rendered mix; this does not grade musical taste."""
from array import array
from pathlib import Path
import hashlib,json,math,wave
root=Path(__file__).resolve().parents[1];source=root/'AssetsSource/Audio'
manifest=json.loads((source/'audio-manifest.json').read_text(encoding='utf8'));checks=[]
def check(name,ok):
    assert ok,name
    checks.append(name)
hashes=[]
for item in manifest['assets']:
    path=source/(item['name']+'.wav');digest=hashlib.sha256(path.read_bytes()).hexdigest();hashes.append(digest)
    with wave.open(str(path),'rb') as f:
        channels=f.getnchannels();seconds=f.getnframes()/f.getframerate();pcm=array('h',f.readframes(f.getnframes()))
    check(item['name']+' valid non-silent unclipped PCM',digest==item['sha256'] and channels==item['channels'] and abs(seconds-item['seconds'])<.001 and max(abs(x) for x in pcm)<32760 and sum(x*x for x in pcm)>10000)
check('all 20 authored cues have different content',len(hashes)==len(set(hashes))==20)
path=root/'Saved/BouncedWavFiles/WarlockAudioReview.wav'
with wave.open(str(path),'rb') as f:
    channels=f.getnchannels();rate=f.getframerate();seconds=f.getnframes()/rate;pcm=array('h',f.readframes(f.getnframes()))
peak=max(abs(x) for x in pcm)/32768;rms=math.sqrt(sum(x*x for x in pcm)/len(pcm))/32768
check('real UE mixed output contains audible-level stereo signal',channels==2 and seconds>2 and rms>.005)
check('recorded game mix does not clip',peak<.99)
report={'passed':len(checks),'checks':checks,'actual_mix':{'file':str(path),'seconds':seconds,'rate':rate,'peak':peak,'rms':rms},'limitation':'verifies real mixer samples, not subjective listening or physical speaker routing'}
(root/'Saved/Validation/audio-files.json').write_text(json.dumps(report,indent=2),encoding='utf8');print(json.dumps(report,indent=2))
