"""Record compact, reproducible training evidence without dependencies or raw logs."""
import hashlib
import json
import math
from pathlib import Path

root=Path(__file__).resolve().parents[1]
training=root/'Saved/Training';model=root/'Content/WarlockArena/AI/TacticalPolicy.json'
policy=json.loads(model.read_text(encoding='utf8'))
results={}
for report in sorted(training.glob('*/report.json')):
    data=json.loads(report.read_text(encoding='utf8'));n=data['episodes'];wins=data['wins']
    rate=wins/n;z=1.96;denom=1+z*z/n
    center=(rate+z*z/(2*n))/denom;half=z*math.sqrt(rate*(1-rate)/n+z*z/(4*n*n))/denom
    data['win_rate']=round(rate,5);data['score_rate']=round((wins+.5*data['draws'])/n,5)
    data['win_rate_wilson95']=[round(center-half,5),round(center+half,5)]
    data['steps_per_second']=round(data['simulation_seconds']/data['fixed_step']/data['wall_seconds'])
    data['superseded_calibration']=report.parent.name in {'test-tier-4','test-tier-5','test-untrained'}
    data['purpose']='deployment smoke; not selected training' if report.parent.name.startswith('packaged-') else 'training' if not data['evaluation'] else 'evaluation/calibration'
    results[report.parent.name]=data
tiers=[results.get(('calibrated-tier-' if t>=4 else 'test-tier-')+str(t)) for t in range(1,6)]
assert all(tiers),'Complete all five held-out tier evaluations first'
output={'scope':'fixed scripted opponent duels, legal shop curriculum, 90-second truncation; no human strength guarantee','selected_model_sha256':hashlib.sha256(model.read_bytes()).hexdigest(),'selected_updates':policy['updates'],'nonzero_actor_weights':sum(abs(x)>1e-7 for x in policy['actor']),'tier_order_strictly_increasing_by_score':all(tiers[t]['score_rate']>tiers[t-1]['score_rate'] for t in range(1,5)),'results':results}
target=root/'Docs/AITraining/2026-11-26.json';target.parent.mkdir(parents=True,exist_ok=True);target.write_text(json.dumps(output,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps({'evidence':str(target),'ordered':output['tier_order_strictly_increasing_by_score'],'tiers':[{'tier':t['tier'],'wins':t['wins'],'losses':t['losses'],'draws':t['draws'],'score':t['score_rate']} for t in tiers]},ensure_ascii=False))
