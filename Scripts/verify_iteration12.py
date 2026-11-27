"""Final editor suite for client budget, skill families, economy and rewards."""
import ast,json,inspect
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
async def run(name):
    source=(root/'Scripts'/name).read_text(encoding='utf-8-sig')
    result=eval(compile(source,name,'exec',ast.PyCF_ALLOW_TOP_LEVEL_AWAIT),{'__name__':'__main__'})
    if inspect.isawaitable(result):await result
for name in ['verify_offline_network_regressions.py','verify_skill_families.py','verify_economy.py','verify_rewards.py','verify_ai_tactics.py']:
    await run(name)
names=['full-flow','ready-routing','guided-shop-input','shop-builds','skill-families','economy','rewards','ai-tactics']
reports={n:json.loads((root/'Saved/Validation'/(n+'.json')).read_text(encoding='utf8'))['passed'] for n in names}
summary={'passed':sum(reports.values()),'reports':reports,'scope':'offline PIE authority/input regression, not packaged rendering or physical devices'}
(root/'Saved/Validation/iteration12.json').write_text(json.dumps(summary,indent=2),encoding='utf8')
print(json.dumps(summary))
