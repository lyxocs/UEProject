"""Run through the editor Python/MCP tool in an offline PIE world on the DS branch."""
import ast,json
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());settings=unreal.GameUserSettings.get_game_user_settings()
async def run(name):
    source=(root/'Scripts'/name).read_text(encoding='utf-8-sig')
    await eval(compile(source,name,'exec',flags=ast.PyCF_ALLOW_TOP_LEVEL_AWAIT),{'__name__':'__main__'})
await run('verify_full_flow.py')
old=settings.get_editor_property('cast_trigger_mode')
try:
    # The historical ready/device check explicitly verifies press-to-cast.
    settings.set_editor_property('cast_trigger_mode',0)
    await run('verify_ready_routing.py')
finally:
    settings.set_editor_property('cast_trigger_mode',old)
await run('verify_guided_shop_input.py')
await run('verify_shop_builds.py')
reports={name:json.loads((root/'Saved/Validation'/(name+'.json')).read_text(encoding='utf-8-sig'))['passed'] for name in ['full-flow','ready-routing','guided-shop-input','shop-builds']}
summary={'passed':sum(reports.values()),'reports':reports,'scope':'offline PIE logic, shop economy, software device routing; not physical devices'}
(root/'Saved/Validation/offline-ds-regressions.json').write_text(json.dumps(summary,indent=2),encoding='utf8')
