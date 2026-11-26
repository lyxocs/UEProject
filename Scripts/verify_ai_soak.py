"""Repeatable 45-second chunks; original timers, explicitly accelerated world clock.
Run in PIE after verify_full_flow. No forced kills or phase changes during soak.
Delete Saved/Validation/ai-soak.json to begin a new run.
"""
import asyncio
import json
import math
from pathlib import Path
import unreal

w=unreal.EditorLevelLibrary.get_game_world()
assert w and 'UEDPIE_' in w.get_path_name()
actors=unreal.GameplayStatics.get_all_actors_of_class
mode=actors(w,unreal.WarlockMode)[0];match=actors(w,unreal.WarlockMatchState)[0]
path=Path(unreal.Paths.project_saved_dir(),'Validation','ai-soak.json')
report=json.loads(path.read_text(encoding='utf8')) if path.exists() else None
if report is None:
    mode.restart_match(False)
    report={'clock_multiplier':20,'passive_humans':2,'events':[],'samples':0,'simulated_seconds':0,'complete':False}
unreal.GameplayStatics.set_game_paused(w,False)
unreal.GameplayStatics.set_global_time_dilation(w,20)
previous_time=unreal.GameplayStatics.get_time_seconds(w)
try:
    for _ in range(90):
        await asyncio.sleep(.5)
        now=unreal.GameplayStatics.get_time_seconds(w)
        report['simulated_seconds']+=now-previous_time;previous_time=now
        pawns=actors(w,unreal.WarlockPawn)
        phase=match.get_editor_property('phase');round_number=match.get_editor_property('round')
        for p in pawns:
            assert all(math.isfinite(p.get_editor_property(k)) for k in ['health','knockback','locomotion_speed']), 'non-finite pawn state'
            assert p.get_editor_property('health')>=0, 'negative HP'
        state=[round_number,phase]
        if not report['events'] or report['events'][-1]['state']!=state:
            report['events'].append({'state':state,'seconds':round(report['simulated_seconds'],2),'scores':[p.get_editor_property('player_state').get_editor_property('points') for p in pawns]})
        report['samples']+=1
        if phase=='FINAL':
            report['complete']=True;report['winner']=match.get_editor_property('announcement')
        path.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
        if report['complete']:break
finally:
    unreal.GameplayStatics.set_global_time_dilation(w,1)
    unreal.GameplayStatics.set_game_paused(w,True)
{'complete':report['complete'],'samples':report['samples'],'simulated_seconds':report['simulated_seconds'],'last_event':report['events'][-1]}
