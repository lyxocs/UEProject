"""Actual PIE audio mixer, quick casts, endpoint portals and eight seat colors."""
import asyncio,json
from pathlib import Path
import unreal

preferences=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorMiscSettings'))
background_audio=preferences.get_editor_property('bAllowBackgroundAudio')
preferences.set_editor_property('bAllowBackgroundAudio',True)
try:
    w=unreal.EditorLevelLibrary.get_game_world();assert w and 'UEDPIE_' in w.get_path_name()
    actors=unreal.GameplayStatics.get_all_actors_of_class
    mode=actors(w,unreal.WarlockMode)[0];match=actors(w,unreal.WarlockMatchState)[0]
    c=unreal.GameplayStatics.get_player_controller(w,0);c.add_mixed_player()
    audio=next(a for a in unreal.ObjectIterator(unreal.WarlockAudio) if a.get_outer()==w)
    s=unreal.GameUserSettings.get_game_user_settings();checks=[]
    def get(o,k):return o.get_editor_property(k)
    def put(o,k,v):o.set_editor_property(k,v)
    def check(name,value):
        assert value,name
        checks.append(name)
    put(match,'participant_count',8);mode.restart_match(False);await asyncio.sleep(.25)
    ps=list(actors(w,unreal.WarlockPawn));p=next(p for p in ps if p.get_controller()==c);state=get(p,'player_state')
    check('eight distinct visual seat IDs',sorted(get(get(p,'player_state'),'color_index') for p in ps)==list(range(8)))
    for p2 in ps:put(get(p2,'player_state'),'appearance',0)
    await asyncio.sleep(.15)
    colors=[]
    for pawn in ps:
        avatar=get(pawn,'avatar');index=next(i for i,m in enumerate(avatar.get_skeletal_mesh_asset().get_editor_property('materials')) if 'PBR_04_' in str(m.get_editor_property('material_slot_name')));tint=avatar.get_material(index).get_vector_parameter_value('Tint');colors.append(tuple(round(get(tint,k),4) for k in ['r','g','b']))
    check('eight actual garment material tints are distinct',len(set(colors))==8)
    mode.restart_match(True);await asyncio.sleep(.15)
    check('co-op teammates retain distinct seat colors',len(set(get(get(p,'player_state'),'color_index') for p in ps))==8)
    mode.restart_match(False)
    check('one shared music component for split-screen',get(audio,'music') is not None and get(audio,'battle_layer') is not None)
    check('music and percussion are playing',get(audio,'music').is_playing() and get(audio,'battle_layer').is_playing())
    check('eight separate cast sound assets',len(set(x.get_path_name() for x in get(audio,'cast_sounds')))==8)
    check('eight separate hit sound assets',len(set(x.get_path_name() for x in get(audio,'hit_sounds')))==8)
    check('both 48-second beds loop',all(x.get_editor_property('looping') for x in [get(audio,'music').sound,get(audio,'battle_layer').sound]))

    oldmusic=get(s,'music_volume');oldfx=get(s,'effects_volume')
    try:
        c.edit_display_setting(11,1);c.edit_display_setting(12,-1)
        check('separate music and FX volume controls',get(s,'music_volume')>oldmusic and get(s,'effects_volume')<oldfx)
        reloaded=unreal.new_object(unreal.WarlockUserSettings);reloaded.load_settings(True)
        check('both volume choices survive config reload',abs(get(reloaded,'music_volume')-get(s,'music_volume'))<.001 and abs(get(reloaded,'effects_volume')-get(s,'effects_volume'))<.001)
        put(s,'music_volume',.28);put(s,'effects_volume',.75)
        # Render the actual UE mix, not just the source WAVs or event counters.
        unreal.GameplayStatics.set_game_paused(w,False)
        unreal.AudioMixerLibrary.start_recording_output(w,8)
        await asyncio.sleep(1)
        for slot in range(8):audio.play_event(slot,0,p.get_actor_location());await asyncio.sleep(.15)
        audio.play_event(2,2,p.get_actor_location());await asyncio.sleep(.3)
        check('stone collision selects lightning identity and stone surface',get(audio,'last_slot')==2 and get(audio,'last_surface')==2)
        audio.play_event(5,3,p.get_actor_location());await asyncio.sleep(.3)
        check('ground collision selects meteor identity and ground surface',get(audio,'last_slot')==5 and get(audio,'last_surface')==3)
        unreal.AudioMixerLibrary.stop_recording_output(w,unreal.AudioRecordingExportType.WAV_FILE,'WarlockAudioReview','')
        await asyncio.sleep(.5)
        before=get(get(audio,'music'),'volume_multiplier');unreal.GameplayStatics.set_game_paused(w,True);await asyncio.sleep(.8)
        check('pause ducks the shared music without stopping it',get(audio,'music').is_playing() and get(get(audio,'music'),'volume_multiplier')<before*.8)
        unreal.GameplayStatics.set_game_paused(w,False)
        put(s,'music_volume',0);await asyncio.sleep(2.5)
        check('music volume zero actually fades the playing component',get(get(audio,'music'),'volume_multiplier')<.005)
        put(s,'music_volume',.28);await asyncio.sleep(.7)
        before=get(get(audio,'battle_layer'),'volume_multiplier');mode.start_round();await asyncio.sleep(.8)
        check('battle percussion grows after round begins',get(get(audio,'battle_layer'),'volume_multiplier')>before+.025)
    finally:
        put(s,'music_volume',oldmusic);put(s,'effects_volume',oldfx);s.save_settings()

    unreal.GameplayStatics.set_game_paused(w,True)
    def reset():
        for fx in actors(w,unreal.WarlockSpellFX)+actors(w,unreal.WarlockProjectile):fx.destroy_actor()
        for i,pawn in enumerate(ps):pawn.reset_for_round(unreal.Vector(0,0,0) if pawn==p else unreal.Vector(3000+i*300,3000,0));put(pawn,'protection',1000 if pawn!=p else 0)
        put(state,'levels',[1]*8);unreal.SystemLibrary.execute_console_command(w,'ke WarlockController ServerInput (X=0,Y=0,Z=0) (X=400,Y=0,Z=0)');put(match,'phase','FIGHT')
    reset();p.cast(5);check('meteor windup is 0.12 seconds',abs(get(p,'cast_duration')-.12)<.0001)
    for _ in range(3):p.step_simulation(.03)
    check('meteor has not released at 0.09 seconds',get(p,'pending_cast')==5)
    p.step_simulation(.03)
    warnings=[x for x in actors(w,unreal.WarlockSpellFX) if get(x,'style')==4]
    check('meteor releases exactly on fourth simulation step',get(p,'pending_cast')==-1 and len(warnings)==1)
    check('meteor retains original impact area',get(warnings[0],'size')>200)
    async def advance_game(seconds):
        start=unreal.GameplayStatics.get_time_seconds(w)
        unreal.GameplayStatics.set_game_paused(w,False)
        await asyncio.sleep(seconds+.08)
        remaining=seconds-(unreal.GameplayStatics.get_time_seconds(w)-start)
        if remaining>0:await asyncio.sleep(remaining+.1)
        unreal.GameplayStatics.set_game_paused(w,True)
        assert unreal.GameplayStatics.get_time_seconds(w)-start>=seconds-.005,'game clock did not advance enough'
    await advance_game(.90)
    check('meteor still warns at 0.90 game seconds after release',warnings[0] in list(actors(w,unreal.WarlockSpellFX)) and not any(get(x,'style')==0 and get(x,'slot')==5 and get(x,'source')==p for x in actors(w,unreal.WarlockSpellFX)))
    await advance_game(.35)
    check('meteor then produces its delayed impact',any(get(x,'style')==0 and get(x,'slot')==5 and get(x,'source')==p for x in actors(w,unreal.WarlockSpellFX)))
    reset();before=get(audio,'played_events');p.cast(5);p.step_simulation(.03);p.cancel_cast()
    for _ in range(10):p.step_simulation(.03)
    check('S-equivalent cancellation before meteor release has no release sound or cooldown',not any(get(p,'cooldowns')) and get(audio,'played_events')==before)
    reset();target=next(a for a in ps if a!=p);target.reset_for_round(unreal.Vector(100,0,0));put(target,'protection',0);p.cast(3)
    target.hit(p,7,unreal.Vector(1,0,0),1,0)
    check('fireball hit keeps its sound identity while caster prepares blink',get(audio,'last_slot')==0 and get(audio,'last_surface')==1 and get(p,'pending_cast')==3)
    reset();start=p.get_actor_location();p.cast(3)
    check('blink quick anticipation is 0.09 seconds',abs(get(p,'cast_duration')-.09)<.0001)
    for _ in range(2):p.step_simulation(.03)
    check('blink does not teleport before third step',p.get_actor_location()==start)
    p.step_simulation(.03)
    fx=next(x for x in actors(w,unreal.WarlockSpellFX) if get(x,'style')==2)
    check('blink teleports on third simulation step',p.get_actor_location().x>300)
    check('departure and arrival have separate black portals',len(get(fx,'portals'))==2)
    check('endpoint echoes replace the long trail',len(get(fx,'echoes'))==2)
    check('visual portals never enable collision',all(x.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION for x in get(fx,'portals')))
    unreal.GameplayStatics.set_game_paused(w,False)
    for _ in range(30):
        if fx not in list(actors(w,unreal.WarlockSpellFX)):break
        await asyncio.sleep(.1)
    check('blink portals expire quickly',fx not in list(actors(w,unreal.WarlockSpellFX)))
    check('character returns to full visual scale',abs(get(get(p,'avatar'),'relative_scale3d').x-.92)<.01)
    report={'passed':len(checks),'checks':checks,'colors':colors,'recorded_mix':'Saved/BouncedWavFiles/WarlockAudioReview.wav','scope':'actual PIE, real audio output capture, simulated input and fixed-step assertions; listening taste and physical controllers remain user review'}
    Path(unreal.Paths.project_saved_dir(),'Validation','audio-apparition.json').write_text(json.dumps(report,indent=2),encoding='utf8')
finally:
    preferences.set_editor_property('bAllowBackgroundAudio',background_audio)
report
