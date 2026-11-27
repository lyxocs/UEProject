"""Deterministic original dark orchestral synth beds and spell foley; no samples."""
from array import array
from pathlib import Path
import hashlib,json,math,random,wave

ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'AssetsSource/Audio';OUT.mkdir(parents=True,exist_ok=True)
RATE=32000;TAU=math.tau;manifest=[]
def save(name,channels,loop=False):
    peak=max(abs(x) for ch in channels for x in ch);gain=.78/max(.001,peak)
    pcm=array('h');count=len(channels[0]);squares=0
    for i in range(count):
        for ch in channels:
            v=ch[i]*gain;squares+=v*v;pcm.append(round(max(-1,min(1,v))*32760))
    p=OUT/(name+'.wav')
    with wave.open(str(p),'wb') as f:f.setparams((len(channels),2,RATE,0,'NONE','not compressed'));f.writeframes(pcm.tobytes())
    manifest.append({'name':name,'seconds':count/RATE,'channels':len(channels),'loop':loop,'peak':.78,'rms':math.sqrt(squares/(count*len(channels))),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
def reverb(x):
    dry=array('f',x)
    for seconds,gain in [(.071,.22),(.137,.16),(.229,.10)]:
        shift=round(seconds*RATE)
        for i in range(shift,len(x)):x[i]+=dry[i-shift]*gain
    return x

duration=48;n=round(duration*RATE);left=array('f',[0])*n;right=array('f',[0])*n;drum=array('f',[0])*n
notes=[(36.708,43.654,55.0),(29.135,36.708,43.654),(32.703,41.203,48.999),(27.5,34.648,41.203)]
for i in range(n):
    t=i/RATE;section=int(t//12);u=t%12
    # Crossfade each harmony over the bar line, retaining a dark D pedal.
    mix=.5-.5*math.cos(math.pi*min(1,u/2.5));a=b=0
    for c,weight in [((section-1)%4,1-mix),(section,mix)]:
        for j,f in enumerate(notes[c]):
            f=round(f*duration)/duration;p=TAU*f*t
            swell=.75+.25*math.sin(TAU*t/12+j)
            horn=math.sin(p+1.15*math.sin(p))*.20+math.sin(p*2)*.11+math.sin(p*3)*.045
            strings=math.sin(p*4+math.sin(TAU*t/6)*.16)*.08+math.sin(p*6+math.sin(TAU*t/8)*.12)*.025
            a+=weight*swell*(horn+strings+math.sin(p*2+TAU*t/48)*.035)
            b+=weight*swell*(horn+strings+math.sin(p*2-TAU*t/48)*.035)
    pedal=math.sin(TAU*(round(36.708*48)/48)*t)*.19
    # A restrained four-note low brass motif, with no abrupt note attacks.
    beat=t/.75;phrase=int(beat//4)%4;z=(beat%4)/4
    mf=[146.832,130.813,110,82.407][phrase];env=math.sin(math.pi*z)**2
    motif=math.sin(TAU*mf*t+.55*math.sin(TAU*mf*t))*.10*env
    edge=min(1,t/.06,(duration-t)/.06)
    left[i]=(a+pedal+motif)*edge;right[i]=(b+pedal+motif)*edge
    since=t%3;boom=math.sin(TAU*(46*since+18*(1-math.exp(-since*12))/12))*math.exp(-since*3.6)
    taiko=.15*math.sin(TAU*93*since)*math.exp(-since*7)
    drum[i]=(boom+taiko)*min(1,t/.02,(duration-t)/.06)
save('S_AbyssTheme',[left,right],True);save('S_BattlePercussion',[reverb(drum)],True)

for slot in range(8):
    for hit in [False,True]:
        rng=random.Random(1301+slot*17+int(hit));length=(1.35 if slot in [1,5] else .8)+.3*int(hit);x=array('f');lp=0
        for i in range(round(length*RATE)):
            t=i/RATE;p=t/length;noise=rng.uniform(-1,1);lp+=.12*(noise-lp);high=noise-lp
            attack=min(1,t/.009);env=attack*math.exp(-t*(5 if hit else 4))*(1-p)**2
            if slot==0:v=lp*2.4+high*.14+math.sin(TAU*(72*t+20*t*t))*.32
            elif slot==1:v=lp*1.6+math.sin(TAU*(48*t+9*(1-math.exp(-t*15))))*.70+high*.14
            elif slot==2:v=high*(.45+.5*math.sin(t*730)**12)+math.sin(TAU*(155*t+50*t*t))*.17
            elif slot==3:v=lp*2.1+math.sin(TAU*(240*t-170*t*t/length))*.24+high*.08
            elif slot==4:v=lp*1.9+high*.30*(.5+.5*math.sin(t*94))
            elif slot==5:v=math.sin(TAU*(39*t+16*(1-math.exp(-t*9))))*.85+lp*2.0+high*.16
            elif slot==6:v=sum(math.sin(TAU*f*t)*math.exp(-t*(j+2)) for j,f in enumerate([174,261,348.8]))*.23+lp*.5
            else:v=high*.28*(.4+.6*math.sin(t*130)**8)+math.sin(TAU*116*t)*.24+lp*.7
            if hit:v=v*.75+lp*1.1*math.exp(-t*8)
            x.append(v*env)
        save(('S_Hit' if hit else 'S_Cast')+str(slot),[reverb(x)])
for name,frequency in [('S_Stone',83),('S_Ground',51)]:
    rng=random.Random(frequency);x=array('f');lp=0
    for i in range(round(.9*RATE)):
        t=i/RATE;noise=rng.uniform(-1,1);lp+=.1*(noise-lp)
        x.append((lp*1.7+math.sin(TAU*frequency*t)*.4+noise*.08)*min(1,t/.005)*math.exp(-t*11)*(1-t/.9))
    save(name,[reverb(x)])
(OUT/'audio-manifest.json').write_text(json.dumps({'authoring':'Original deterministic synthesis; no film/game samples or external music. Replaceable project-owned prototype score.','sample_rate':RATE,'assets':manifest},indent=2),encoding='utf8')
print(json.dumps({'assets':len(manifest),'seconds':sum(a['seconds'] for a in manifest),'directory':str(OUT)}))
