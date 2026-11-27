"""Actual paused viewport preset selection, conflict-free keys and persistence."""
import json,unreal
from pathlib import Path
w=unreal.EditorLevelLibrary.get_game_world();assert w and 'UEDPIE_' in w.get_path_name()
c=unreal.GameplayStatics.get_player_controller(w,0);settings=unreal.GameUserSettings.get_game_user_settings()
view=next(v for v in unreal.ObjectIterator(unreal.WarlockViewport) if not v.get_name().startswith('Default__'))
old=list(settings.get_editor_property('keyboard_bindings'));checks=[]
def keys():return [str(settings.keyboard_key(i).get_editor_property('key_name')) for i in range(22)]
def check(name,value):assert value,name;checks.append(name)
def press(key):view.inject_local_key(key,True);view.inject_local_key(key,False)
try:
 c.add_mixed_player()
 unreal.GameplayStatics.set_game_paused(w,True);c.open_pause_page(3);press('End')
 league=keys();check('End selects League preset through paused viewport',settings.keyboard_preset()==1)
 check('Q R W F E D C T occupy eight skill slots',league[4:12]==['Q','R','W','F','E','D','C','T'])
 check('Flash F, cancel S, shop Tab and movement arrows remain independent',league[7]=='F' and league[13]=='S' and league[12]=='Tab' and league[:4]==['Up','Down','Left','Right'])
 check('all 22 bindings are unique',len(set(league))==22)
 check('alternative route moved away from Q',league[17]=='V')
 settings.load_settings(True);check('League preset survives disk reload',keys()==league and settings.keyboard_preset()==1)
 c.start_key_capture(4);press('Z');check('custom remap after preset remains available',keys()[4]=='Z' and settings.keyboard_preset()==2)
 press('Home');classic=keys();check('Home restores original preset',settings.keyboard_preset()==0 and classic[4:12]==['G','F','D','R','T','E','C','Y'])
 press('Gamepad_RightShoulder');check('RB selects League preset',settings.keyboard_preset()==1)
 press('Gamepad_LeftShoulder');check('LB selects original preset',settings.keyboard_preset()==0)
 press('Escape');check('Escape returns to pause home',not c.get_editor_property('keybind_menu_open') and unreal.GameplayStatics.is_game_paused(w))
 report={'passed':len(checks),'checks':checks};(Path(unreal.Paths.project_saved_dir())/'Validation/keyboard-presets.json').write_text(json.dumps(report,indent=2),encoding='utf8');print(json.dumps(report))
finally:
 settings.set_editor_property('keyboard_bindings',old);settings.save_settings();c.open_pause_page(3);c.start_key_capture(21);press(str(old[21].get_editor_property('key_name')));c.open_pause_page(0);unreal.GameplayStatics.set_game_paused(w,False)
