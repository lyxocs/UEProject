"""Raw viewport regression for capture -> bindings -> pause home -> play."""
import json,unreal
from pathlib import Path
w=unreal.EditorLevelLibrary.get_game_world();assert w and 'UEDPIE_' in w.get_path_name()
c=unreal.GameplayStatics.get_player_controller(w,0)
view=next(v for v in unreal.ObjectIterator(unreal.WarlockViewport) if not v.get_name().startswith('Default__'))
settings=unreal.GameUserSettings.get_game_user_settings()
bindings=lambda:[str(k.get_editor_property('key_name')) for k in settings.get_editor_property('keyboard_bindings')]
original=bindings();checks=[]
get=lambda o,k:o.get_editor_property(k)
def key(name):view.inject_local_key(name,True);view.inject_local_key(name,False)
def check(name,value):
 assert value,name
 checks.append(name)
try:
 unreal.GameplayStatics.set_game_paused(w,False);c.open_pause_page(0)
 key('Escape');check('Escape opens pause home',unreal.GameplayStatics.is_game_paused(w) and not get(c,'keybind_menu_open'))
 key('F10');check('F10 opens bindings',get(c,'keybind_menu_open'))
 c.start_key_capture(4);check('capture starts',get(c,'waiting_binding')==4)
 key('Escape');check('first Escape cancels capture only',get(c,'waiting_binding')==-1 and get(c,'keybind_menu_open') and unreal.GameplayStatics.is_game_paused(w))
 key('Escape');check('next Escape returns to pause home without resuming',not get(c,'keybind_menu_open') and unreal.GameplayStatics.is_game_paused(w))
 key('Escape');check('Escape from home resumes game',not unreal.GameplayStatics.is_game_paused(w))
 key('Escape');check('reopening pause does not restore stale bindings page',unreal.GameplayStatics.is_game_paused(w) and not get(c,'keybind_menu_open'))
 key('F10');key('BackSpace');check('Backspace still returns to pause home',not get(c,'keybind_menu_open') and unreal.GameplayStatics.is_game_paused(w))
 key('F10');c.toggle_pause();check('continue closes bindings state',not get(c,'keybind_menu_open') and get(c,'waiting_binding')==-1 and not unreal.GameplayStatics.is_game_paused(w))
 key('Escape');check('next opening is root after continue',unreal.GameplayStatics.is_game_paused(w) and not get(c,'keybind_menu_open'))
 check('Escape never changes saved bindings',original==bindings())
 report={'passed':len(checks),'checks':checks,'scope':'actual raw viewport keyboard route in PIE'}
 (Path(unreal.Paths.project_saved_dir())/'Validation/keybind-escape.json').write_text(json.dumps(report,indent=2),encoding='utf8');print(json.dumps(report))
finally:
 c.open_pause_page(0);unreal.GameplayStatics.set_game_paused(w,False)
