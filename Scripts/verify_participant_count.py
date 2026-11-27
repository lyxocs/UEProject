"""Actual 2..8-seat roster, input menu, starts, combat and cooperative guard."""
import asyncio,json,math
from pathlib import Path
import unreal
w=unreal.EditorLevelLibrary.get_game_world();assert w and 'UEDPIE_' in w.get_path_name()
actors=unreal.GameplayStatics.get_all_actors_of_class
mode=actors(w,unreal.WarlockMode)[0];match=actors(w,unreal.WarlockMatchState)[0]
c=unreal.GameplayStatics.get_player_controller(w,0)
view=next(v for v in unreal.ObjectIterator(unreal.WarlockViewport) if not v.get_name().startswith('Default__'))
get=lambda o,k:o.get_editor_property(k)
old=get(match,'participant_count');checks=[];path=Path(unreal.Paths.project_saved_dir(),'Validation','participant-count.json')
def check(name,value):
    if not value:raise AssertionError(name)
    checks.append(name)
def pawns():return list(actors(w,unreal.WarlockPawn))
def humans():return [p for p in pawns() if not get(get(p,'player_state'),'bot')]
def pause(value=True):unreal.GameplayStatics.set_game_paused(w,value)
async def key(name,pad=None):
    if pad is None:view.inject_local_key(name,True);view.inject_local_key(name,False)
    else:view.inject_pad_key(name,pad,True);view.inject_pad_key(name,pad,False)
    await asyncio.sleep(.03)
try:
    pause(False);mode.restart_match(False);c.add_mixed_player();await asyncio.sleep(.1);pause()
    for count in range(2,9):
        c.set_participant_count(count);ps=pawns();ss=[get(p,'player_state') for p in ps]
        check('%d total seats and matching replicated target'%count,len(ps)==count and get(match,'participant_count')==count)
        check('%d seats preserve two humans and fill AI'%count,len(humans())==2 and sum(get(s,'bot') for s in ss)==count-2)
        check('%d seats have unique names and FFA teams'%count,len({s.get_player_name() for s in ss})==count and len({get(s,'team') for s in ss})==count)
        positions=[p.get_actor_location() for p in ps]
        check('%d spawn positions spaced without overlap'%count,all((a-b).length()>100 for i,a in enumerate(positions) for b in positions[i+1:]) and all(abs(math.hypot(p.x,p.y)-700)<.1 for p in positions))
        check('%d preparation AI ready and humans waiting'%count,all(get(s,'ready')==get(s,'bot') for s in ss) and get(match,'phase')=='SHOP' and get(match,'round')==0)
    check('resize leaves no orphan AI controllers',len(actors(w,unreal.WarlockAI))==6)
    pause(False);c.set_participant_count(7);check('changes rejected while unpaused',get(match,'participant_count')==8);pause()
    c.set_participant_count(9);c.set_participant_count(1);check('out-of-range counts rejected',get(match,'participant_count')==8 and len(pawns())==8)
    c.configure_local_players(2);c.set_participant_count(7);check('cannot alter roster while assigning devices',get(match,'participant_count')==8);view.back_local_setup()
    c.open_pause_page(2);c.set_participant_count(4);await key('PageUp');check('keyboard PageUp increases total without changing locals',get(match,'participant_count')==5 and len(humans())==2)
    await key('Gamepad_RightShoulder',0);check('pad RB increases total',get(match,'participant_count')==6)
    await key('PageDown');await key('Gamepad_LeftShoulder',0);check('keyboard and pad decrement total',get(match,'participant_count')==4)
    c.set_participant_count(8);mode.start_round();h=humans()[0];s=get(h,'player_state');h.set_editor_property('health',71);s.set_editor_property('gold',29)
    check('eight-seat round retains four-second starting protection',all(abs(get(p,'protection')-4)<.001 for p in pawns()))
    c.set_participant_count(8);check('same total does not restart or reset economy',get(match,'phase')=='FIGHT' and get(h,'health')==71 and get(s,'gold')==29)
    before=sum(get(p.get_controller(),'policy_decisions') for p in pawns() if get(get(p,'player_state'),'bot'))
    for p in pawns():
        if get(get(p,'player_state'),'bot'):p.set_editor_property('protection',0)
    pause(False);await asyncio.sleep(2);pause()
    after=sum(get(p.get_controller(),'policy_decisions') for p in pawns() if get(get(p,'player_state'),'bot'))
    check('six AI execute policy in actual eight-seat combat',after>before)
    for p in pawns():
        if p!=h:p.set_editor_property('alive',False);p.set_editor_property('health',0)
    pause(False);await asyncio.sleep(.15);pause()
    check('eight-seat FFA settles with one survivor',get(match,'phase')=='RESULT' and get(s,'points')==1)
    mode.restart_match(False);mode.start_round();h=humans()[0];h.cast(0)
    for _ in range(16):h.step_simulation(.03)
    check('reset fixture has live projectile',len(actors(w,unreal.WarlockProjectile))>0)
    c.set_participant_count(3)
    check('changing total during combat starts clean preparation',get(match,'phase')=='SHOP' and get(match,'round')==0 and len(pawns())==3 and all(get(get(p,'player_state'),'gold')==20 for p in humans()))
    check('resize clears projectiles and transient spell effects',not actors(w,unreal.WarlockProjectile) and not actors(w,unreal.WarlockSpellFX))
    c.set_participant_count(2);mode.restart_match(True)
    check('two-human cooperation automatically retains one AI opponent',get(match,'participant_count')==3 and len(pawns())==3)
    c.set_participant_count(2);check('cooperative lower bound cannot be bypassed',get(match,'participant_count')==3)
    c.set_participant_count(8);check('eight-seat cooperation has human and AI teams',{get(get(p,'player_state'),'team') for p in humans()}=={0} and all(get(get(p,'player_state'),'team')==1 for p in pawns() if get(get(p,'player_state'),'bot')))
    c.configure_local_players(1);view.claim_device(-1)
    check('local two-to-one keeps eight total and seven AI',get(match,'participant_count')==8 and len(pawns())==8 and len(humans())==1)
    c.set_participant_count(2);check('single human can select two total with one opponent',len(pawns())==2 and len(humans())==1)
except Exception as exc:
    path.write_text(json.dumps({'passed':len(checks),'error':repr(exc),'checks':checks},indent=2),encoding='utf8');raise
finally:
    pause();mode.restart_match(False);c.set_participant_count(old);c.open_pause_page(0);pause(False)
report={'passed':len(checks),'checks':checks,'physical_devices':'software viewport events only','policy':'existing model exercised, not retrained/calibrated for eight seats'}
path.write_text(json.dumps(report,indent=2),encoding='utf8')
report
