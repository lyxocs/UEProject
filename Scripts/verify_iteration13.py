"""Legacy regression plus six-tier difficulty and expert sensor/shop checks."""
import ast,inspect,json
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
w=unreal.EditorLevelLibrary.get_game_world();controller=unreal.GameplayStatics.get_player_controller(w,0)
unreal.GameplayStatics.set_game_paused(w,True);controller.set_ai_difficulty(3);unreal.GameplayStatics.set_game_paused(w,False)
for name in ['verify_iteration12.py','verify_ai_difficulty.py','verify_expert_ai.py']:
 result=eval(compile((root/'Scripts'/name).read_text(encoding='utf8'),name,'exec',ast.PyCF_ALLOW_TOP_LEVEL_AWAIT),{'__name__':'__main__'})
 if inspect.isawaitable(result):await result
reports={n:json.loads((root/'Saved/Validation'/(n+'.json')).read_text(encoding='utf8'))['passed'] for n in ['iteration12','ai-difficulty','expert-ai']}
report={'passed':sum(reports.values()),'reports':reports,'scope':'PIE authority, software input, 180ms delayed sensing and shop legality; strength measured separately'}
(root/'Saved/Validation/iteration13.json').write_text(json.dumps(report,indent=2),encoding='utf8');print(json.dumps(report))
