"""Actual PIE pause navigation, shared display settings and config reload."""
import asyncio,json
from pathlib import Path
import unreal
w=unreal.EditorLevelLibrary.get_game_world();assert w and 'UEDPIE_' in w.get_path_name()
actors=unreal.GameplayStatics.get_all_actors_of_class
c=unreal.GameplayStatics.get_player_controller(w,0);c.add_mixed_player();await asyncio.sleep(.1)
c2=unreal.GameplayStatics.get_player_controller(w,1)
view=next(v for v in unreal.ObjectIterator(unreal.WarlockViewport) if not v.get_name().startswith('Default__'))
s=unreal.GameUserSettings.get_game_user_settings();assert isinstance(s,unreal.WarlockUserSettings)
checks=[];path=Path(unreal.Paths.project_saved_dir(),'Validation','client-settings.json')
def get(o,k):return o.get_editor_property(k)
def check(name,condition):
    if not condition:
        path.write_text(json.dumps({'passed':len(checks),'failed':name,'checks':checks},indent=2),encoding='utf8');raise AssertionError(name)
    checks.append(name)
async def key(name,pad=False):
    if pad:view.inject_pad_key(name,0,True);view.inject_pad_key(name,0,False)
    else:view.inject_local_key(name,True);view.inject_local_key(name,False)
    await asyncio.sleep(.04)
methods=['anti_aliasing','texture','shadow','global_illumination','reflection','visual_effect','post_processing','view_distance','foliage','shading','landscape']
old={m:getattr(s,'get_'+m+'_quality')() for m in methods}
old.update(cap=s.get_frame_rate_limit(),sync=s.is_v_sync_enabled(),dynamic=s.is_dynamic_resolution_enabled(),scale=s.get_resolution_scale_information_ex()[1],preset=get(s,'graphics_preset'),aa=get(s,'anti_aliasing_mode'),initialized=get(s,'client_settings_initialized'))
try:
    for i in range(5):
        c.set_graphics_preset(i)
        check('preset %d persists explicit index'%i,get(s,'graphics_preset')==i)
        check('preset %d resolution scale bounded'%i,50<=s.get_resolution_scale_information_ex()[1]<=100)
        check('preset %d retains appropriate temporal AA'%i,s.get_anti_aliasing_quality()==(3 if i<=1 or i==4 else 2))
        check('preset %d selects explicit reconstruction mode'%i,get(s,'anti_aliasing_mode')==(0 if i<=1 else 2 if i==4 else 1))
    c.set_graphics_preset(1)
    check('fast disables Lumen GI and reflections',s.get_global_illumination_quality()==0 and s.get_reflection_quality()==0)
    check('fast lowers post process and shadow cost',s.get_post_processing_quality()==0 and s.get_shadow_quality()==1)
    c.set_graphics_preset(3)
    check('high restores Lumen and full material quality',s.get_global_illumination_quality()==2 and s.get_reflection_quality()==2)
    quality=[getattr(s,'get_'+m+'_quality')() for m in methods[1:]]
    scale=s.get_resolution_scale_information_ex()[1];cap=s.get_frame_rate_limit()
    for expected in [2,0,1]:
        c.edit_display_setting(10,1)
        check('AA cycle selects mode %d'%expected,get(s,'anti_aliasing_mode')==expected)
        check('AA mode %d preserves other quality and scale'%expected,quality==[getattr(s,'get_'+m+'_quality')() for m in methods[1:]] and scale==s.get_resolution_scale_information_ex()[1] and cap==s.get_frame_rate_limit())
    s.set_frame_rate_limit(240);c.edit_display_setting(4,1)
    check('240 cycles to uncapped',s.get_frame_rate_limit()==0)
    c.edit_display_setting(4,1);check('uncapped cycles to 30',s.get_frame_rate_limit()==30)
    c.edit_display_setting(4,-1);check('reverse cycles back to uncapped',s.get_frame_rate_limit()==0)
    before=s.is_v_sync_enabled();c.edit_display_setting(5,1);check('VSync toggle works',s.is_v_sync_enabled()!=before)
    c.edit_display_setting(3,1);check('custom scale marks preset custom',get(s,'graphics_preset')==-1)
    c.edit_display_setting(6,1);shadow=s.get_shadow_quality()
    reloaded=unreal.new_object(unreal.WarlockUserSettings);reloaded.load_settings(True)
    check('derived settings reload saved frame cap',reloaded.get_frame_rate_limit()==0)
    check('derived settings reload saved VSync',reloaded.is_v_sync_enabled()==s.is_v_sync_enabled())
    check('derived settings reload saved custom preset',get(reloaded,'graphics_preset')==-1)
    check('reconstruction choice survives config reload',get(reloaded,'anti_aliasing_mode')==get(s,'anti_aliasing_mode'))
    # SaveState writes the editor scalability group in PIE; standalone persistence is
    # additionally checked by packaged restarts, rather than claiming this covers it.
    unreal.GameplayStatics.set_game_paused(w,False)
    await key('Escape');check('raw Escape pauses world',unreal.GameplayStatics.is_game_paused(w))
    c.set_editor_property('display_settings_open',False);c2.set_editor_property('display_settings_open',False)
    await key('Tab');check('paused Tab opens shared display tab',get(c,'display_settings_open') and get(c2,'display_settings_open'))
    c.set_editor_property('selected_display_row',4);before=s.get_frame_rate_limit()
    await key('Right');check('keyboard adjusts frame cap while paused',s.get_frame_rate_limit()!=before)
    await key('Gamepad_DPad_Down',True);check('pad selects next settings row',get(c,'selected_display_row')==5)
    before=s.is_v_sync_enabled();await key('Gamepad_DPad_Right',True);check('pad edits VSync while paused',s.is_v_sync_enabled()!=before)
    check('editing settings preserves pause',unreal.GameplayStatics.is_game_paused(w))
    c.set_editor_property('selected_display_row',9)
    await key('Gamepad_DPad_Down',True);check('pad reaches reconstruction row',get(c,'selected_display_row')==10)
    before=get(s,'anti_aliasing_mode');await key('Right');check('keyboard changes reconstruction while paused',get(s,'anti_aliasing_mode')!=before)
    await key('Gamepad_FaceButton_Left',True);check('pad X returns to AI tab',not get(c,'display_settings_open') and not get(c2,'display_settings_open'))
    await key('Escape');check('Escape resumes after display changes',not unreal.GameplayStatics.is_game_paused(w))
    c.set_graphics_preset(0);await asyncio.sleep(.15)
    arena=actors(w,unreal.WarlockArena)[0]
    motes=arena.get_components_by_class(unreal.MaterialBillboardComponent);check('low hides all ambient decorative motes',len(motes)==24 and all(not m.is_visible() for m in motes))
    c.set_graphics_preset(3);await asyncio.sleep(.15)
    check('high restores ambient decorative motes',all(m.is_visible() for m in motes))
    before=s.get_frame_rate_limit();aa=get(s,'anti_aliasing_mode');c.edit_display_setting(-1,1);c.edit_display_setting(11,1)
    check('invalid settings rows ignored',s.get_frame_rate_limit()==before)
    check('invalid rows preserve AA choice',get(s,'anti_aliasing_mode')==aa)
finally:
    for m in methods:getattr(s,'set_'+m+'_quality')(old[m])
    s.set_frame_rate_limit(old['cap']);s.set_v_sync_enabled(old['sync']);s.set_dynamic_resolution_enabled(old['dynamic']);s.set_resolution_scale_value_ex(old['scale'])
    s.set_editor_property('graphics_preset',old['preset']);s.set_editor_property('anti_aliasing_mode',old['aa']);s.set_editor_property('client_settings_initialized',old['initialized']);s.apply_non_resolution_settings();s.save_settings()
    c.set_editor_property('display_settings_open',False);c2.set_editor_property('display_settings_open',False)
    unreal.GameplayStatics.set_game_paused(w,False)
report={'passed':len(checks),'checks':checks,'physical_devices':'software viewport events; no physical controller claim'}
path.write_text(json.dumps(report,indent=2),encoding='utf8')
report
