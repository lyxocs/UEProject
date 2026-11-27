"""Compact, reproducible evidence for the sixth-tier model; keeps failed candidates."""
import hashlib,json,math
from pathlib import Path
root=Path(__file__).resolve().parents[1];base=root/'Saved/Training'
def read(name):return json.loads((base/name/'report.json').read_text(encoding='utf8'))
def result(name):
 r=read(name);n=r['episodes'];p=r['wins']/n;z=1.96;centre=(p+z*z/(2*n))/(1+z*z/n);half=z*math.sqrt(p*(1-p)/n+z*z/(4*n*n))/(1+z*z/n)
 r.update({'win_rate':p,'score_rate':(r['wins']+.5*r['draws'])/n,'win_wilson95':[centre-half,centre+half]});return r
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
old=root/'Content/WarlockArena/AI/TacticalPolicy.json';model=root/'Content/WarlockArena/AI/ExpertPolicy.json'
assert sha(old)=='f53fac546f220331ac757051c2b2528213f612e5d7d78537fc2bf1f0a7b4d424'
report={'date':'2026-11-27','legacy_policy_unchanged_sha256':sha(old),'selected_policy_sha256':sha(model),'selected_checkpoint':'expert-latency-tune/policy.json','selected_updates':json.loads(model.read_text(encoding='utf8'))['updates'],'observation_revision':2,'movement_revision':2,'hard_minimum_observation_seconds':.18,'first_training':read('expert-league-01'),'adaptation_training':read('expert-latency-tune'),'selection_release_revision':{n:result('expert-dodge-select-'+n) for n in ['zero','base','tuned']},'heldout_release_revision':{'versus_legacy_master':result('expert-release-heldout-vs5'),'versus_frozen_500_episode_expert':result('expert-release-heldout-vs500')},'superseded_before_dodge_commit':{'selection':{n:result('expert-latency-select-'+n) for n in ['zero','base','tuned']},'versus_legacy_master':result('expert-final-heldout-vs5'),'versus_frozen_500_episode_expert':result('expert-final-heldout-vs500')},'superseded_enemy_only_delay_results':{'versus_legacy_master':result('expert-heldout-vs5'),'versus_frozen_500_episode_expert':result('expert-heldout-vs500')},'limitations':['Duel with 90 second truncation and 20-200 gold legal curricula','Not human win-rate or calibrated four/eight-player FFA strength','Safety and legal-action masks are hand-authored, not learned from scratch','Several historical results predate self-hit delay or dodge commitment; use release revision','The two trained candidates differ by one score percentage point in selection, not proof of a statistically significant ranking']}
target=root/'Docs/AITraining/2026-11-27-expert.json';target.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8',newline='\n')
print(json.dumps({'evidence':str(target),'selected_sha256':report['selected_policy_sha256'],'final_win_rate':report['heldout_release_revision']['versus_legacy_master']['win_rate'],'final_frozen_score':report['heldout_release_revision']['versus_frozen_500_episode_expert']['score_rate']}))
