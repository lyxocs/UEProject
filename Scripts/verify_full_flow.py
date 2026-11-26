"""MCP PIE regression. Test-only state changes stay in the transient play world.
Round transitions are accelerated by advancing their remaining timer; separate
AI soak checks run the unmodified timers under explicit world time dilation.
"""
import asyncio
import json
import math
from pathlib import Path
import unreal

# Isolate original numeric/phase regression from the newly requested feel preset.
# Default windups and slower projectiles are verified in verify_casting_controls.py.
tuning=unreal.get_default_object(unreal.load_class(None,'/Script/WarlockArena.WarlockTuning'))
original_windups=list(tuning.get_editor_property('Windups'))
original_speed=tuning.get_editor_property('ProjectileSpeedScale')
original_fireball=tuning.get_editor_property('FireballSpeedScale')
tuning.set_editor_property('Windups',[0.0]*8)
tuning.set_editor_property('ProjectileSpeedScale',1.0)
tuning.set_editor_property('FireballSpeedScale',1.0)
try:
    w=unreal.EditorLevelLibrary.get_game_world()
    assert w and 'UEDPIE_' in w.get_path_name()
    actors=unreal.GameplayStatics.get_all_actors_of_class
    mode=actors(w,unreal.WarlockMode)[0];match=actors(w,unreal.WarlockMatchState)[0]
    c1=unreal.GameplayStatics.get_player_controller(w,0)
    mode.restart_match(False)
    c1.add_mixed_player()
    await asyncio.sleep(.15)
    c2=unreal.GameplayStatics.get_player_controller(w,1)
    pawns=list(actors(w,unreal.WarlockPawn))
    p1=next(p for p in pawns if p.get_editor_property('player_state')==c1.get_editor_property('player_state'))
    p2=next(p for p in pawns if p.get_editor_property('player_state')==c2.get_editor_property('player_state'))
    s1=p1.get_editor_property('player_state');s2=p2.get_editor_property('player_state')
    unreal.GameplayStatics.set_game_paused(w,True)
    checks=[]
    path=Path(unreal.Paths.project_saved_dir(),'Validation','full-flow.json')
    def check(name,value):
        if not value:
            path.write_text(json.dumps({'passed':len(checks),'checks':checks,'failed':name},indent=2),encoding='utf8')
            raise AssertionError(name)
        checks.append(name)
    def get(p,key):return p.get_editor_property(key)
    def put(p,key,value):p.set_editor_property(key,value)
    def near(a,b,tol=.003):return abs(a-b)<tol
    def reset(p,xyz=(0,0,0)):
        p.reset_for_round(unreal.Vector(*xyz));put(p,'protection',0)
    def clean_build(s):
        put(s,'items',[0]*8)
        for key in ['life_mastery','range_mastery','time_mastery']:put(s,key,0)
    def momentum(p):return get(p,'momentum').x
    async def tick(seconds=.12):
        unreal.GameplayStatics.set_game_paused(w,False);await asyncio.sleep(seconds);unreal.GameplayStatics.set_game_paused(w,True)
    def clear_shots():
        for p in actors(w,unreal.WarlockProjectile):p.destroy_actor()

    check('two local players and four seats',c2 is not None and len(pawns)==4)
    mode.restart_match(False)
    check('new match starts SHOP with 40 seconds',get(match,'phase')=='SHOP' and near(get(match,'remaining'),40))
    check('both humans start with 20 gold',get(s1,'gold')==get(s2,'gold')==20)
    mode.buy(p1,3)
    check('learning Blink costs 11',get(s1,'gold')==9 and get(s1,'levels')[3]==1)
    mode.buy(p1,5)
    check('failed purchase changes neither balance nor skill',get(s1,'gold')==9 and get(s1,'levels')[5]==0)
    check('cannot cast in SHOP',not p1.cast(0))
    mode.start_round()
    check('round begins at 1 with four-second protection',get(match,'round')==1 and near(get(p1,'protection'),4))
    balance=get(s1,'gold');mastery=get(s1,'life_mastery');mode.buy(p1,8)
    check('cannot purchase during FIGHT',get(s1,'gold')==balance and get(s1,'life_mastery')==mastery)
    clean_build(s1);clean_build(s2)
    reset(p1,(-300,0,0));reset(p2)
    put(p2,'knockback',50)
    p2.hit(p1,7,unreal.Vector(1,0,0))
    check('7 damage reduces HP to 93 and K50 to K57',near(get(p2,'health'),93) and near(get(p2,'knockback'),57))
    check('hit uses old K50 impulse 31.5',near(momentum(p2),31.5))
    p2.hit(p1,7,unreal.Vector(1,0,0))
    check('second impulse adds rather than overwrites',near(momentum(p2),31.5+157*7*.03))
    reset(p2);p2.hit(p1,7,unreal.Vector(1,0,0))
    for _ in range(100):p2.step_simulation(.03)
    expected=21*24*(1-.96**100)
    check('100 fixed steps decay then integrate exactly',near(p2.get_actor_location().x,expected,.03))
    check('passive knockback does not animate walking',near(get(p2,'locomotion_speed'),0))
    reset(p2,(-896/1.5-65,-896/1.5,0));blocked_start=p2.get_actor_location().x
    move_action=next(a for a in get(c2,'actions') if a.get_name()=='Move')
    subsystem=c2.local_input()
    subsystem.stop_continuous_input_injection_for_action(move_action)
    def hold_move(dt):
        subsystem.inject_input_vector_for_action(move_action,unreal.Vector(1,0,0),[],[])
        return True
    handle=unreal.register_ticker_callback(hold_move)
    try:
        await tick(.2)
        check('blocked pillar movement produces zero walk speed',get(p2,'move_input').x>.5 and near(p2.get_actor_location().x,blocked_start,.1) and near(get(p2,'locomotion_speed'),0,.03))
    finally:
        unreal.unregister_ticker_callback(handle)
    await tick(.08)
    reset(p2);put(p2,'protection',4)
    p2.hit(p1,7,unreal.Vector(1,0,0))
    check('protected target retains HP and K',near(get(p2,'health'),100) and near(get(p2,'knockback'),0))
    check('protected target receives 10 percent impulse',near(momentum(p2),2.1))
    reset(p2);put(p1,'protection',4)
    p2.hit(p1,7,unreal.Vector(1,0,0))
    check('protected attacker outputs 10 percent damage and impulse',near(get(p2,'health'),99.3) and near(get(p2,'knockback'),.7) and near(momentum(p2),2.1))
    reset(p2);put(p2,'protection',4)
    p2.hit(p1,7,unreal.Vector(1,0,0))
    check('both protection factors multiply to one percent',near(momentum(p2),.21) and near(get(p2,'health'),100))
    for _ in range(134):p2.step_simulation(.03)
    check('protection expires after four seconds',near(get(p2,'protection'),0))
    put(p1,'protection',0);reset(p2)
    items=[0]*8;items[7]=1;put(s2,'items',items)
    p2.hit(p1,7,unreal.Vector(1,0,0))
    check('helmet applies 0.84 impulse without dividing damage by mass',near(momentum(p2),21*.84) and near(get(p2,'health'),93))
    clean_build(s2);reset(p2);put(p2,'health',90);put(p2,'knockback',50)
    for _ in range(10):p2.step_ground()
    check('safe ground regenerates 0.5 HP per second without clearing K',near(get(p2,'health'),90.5) and near(get(p2,'knockback'),50))
    reset(p2,(1500,0,0))
    for _ in range(10):p2.step_ground()
    check('lava removes 9 HP and adds 4.5 K per second',near(get(p2,'health'),91) and near(get(p2,'knockback'),4.5))
    reset(p2,(1500,0,0));put(p2,'protection',4)
    for _ in range(10):p2.step_ground()
    check('protection blocks lava damage and K',near(get(p2,'health'),100) and near(get(p2,'knockback'),0))
    reset(p1,(-300,0,0));reset(p2);put(s1,'life_mastery',1);put(p1,'knockback',50)
    p2.hit(p1,7,unreal.Vector(1,0,0),0)
    check('full-health nominal lifesteal still reduces K',near(get(p1,'health'),100) and near(get(p1,'knockback'),49.65))
    clean_build(s1);reset(p2,(1500,0,0));p2.hit(p1,.1,unreal.Vector(1,0,0),0);put(p2,'health',1.3)
    score=get(s1,'points');gold=get(s1,'gold');p2.step_ground()
    check('lava death retains last attacker and highest contributor',not get(p2,'alive') and get(s1,'points')==score+2 and get(s1,'gold')==gold)
    p2.step_ground();p2.hit(p1,7,unreal.Vector(1,0,0))
    check('death cannot award points twice',get(s1,'points')==score+2)
    reset(p2,(1500,0,0));put(p2,'health',1.3);score=get(s2,'points');p2.step_ground()
    check('unattributed FFA lava death deducts one point',get(s2,'points')==score-1)
    reset(p1,(-300,0,0));reset(p2,(300,0,0));clear_shots()
    check('opposing fireballs cast successfully',p1.cast(0) and p2.cast(0))
    shot=actors(w,unreal.WarlockProjectile)[0]
    check('projectile begins at caster center',any(near(shot.get_actor_location().x,p.get_actor_location().x) for p in [p1,p2]))
    for _ in range(12):mode.simulate_projectiles(.03)
    check('opposing swept projectiles cancel without HP loss',len(actors(w,unreal.WarlockProjectile))==0 and near(get(p1,'health'),100) and near(get(p2,'health'),100))
    reset(p1,(-300,0,0));reset(p2,(300,0,0));p1.cast(0);mode.simulate_projectiles(.6)
    check('high-speed projectile cannot tunnel through target',near(get(p2,'health'),93) and len(actors(w,unreal.WarlockProjectile))==0)
    reset(p1,(-300,0,0));reset(p2,(300,0,0));put(p2,'shield_remaining',3);p1.cast(0)
    for _ in range(3):mode.simulate_projectiles(.3)
    check('reflected fireball damages original caster',near(get(p1,'health'),93) and near(get(p2,'health'),100))
    clear_shots();reset(p1,(-200,0,0));reset(p2,(0,900,0));p1.cast(0)
    for _ in range(33):mode.simulate_projectiles(.03)
    shots=actors(w,unreal.WarlockProjectile)
    check('fireball speed and quantized range match 1000 and 0.99 seconds',len(shots)==1 and near(shots[0].get_actor_location().x,790,.05))
    mode.simulate_projectiles(.03)
    check('expired projectile is removed before another movement step',len(actors(w,unreal.WarlockProjectile))==0)
    reset(p1,(-1000,0,0));p1.hit(p2,7,unreal.Vector(1,0,0));p1.cast(3)
    check('Blink L1 travels at most 770 and preserves momentum',near(p1.get_actor_location().x,-230) and near(momentum(p1),21))
    check('Blink consumes 16-second cooldown',near(get(p1,'cooldowns')[3],16) and not p1.cast(3))
    put(p1,'health',3);p1.cast(1)
    check('Smite self cost retains minimum 0.5 HP',near(get(p1,'health'),.5) and get(p1,'alive'))
    clear_shots();reset(p1);p1.hit(p2,float('nan'),unreal.Vector(1,0,0))
    check('non-finite damage cannot corrupt HP or K',near(get(p1,'health'),100) and near(get(p1,'knockback'),0))
    mode.restart_match(False);mode.start_round();reset(p1,(-300,0,0));p1.cast(0)
    check('restart cleanup case has a live projectile',len(actors(w,unreal.WarlockProjectile))>0)
    put(s1,'appearance',1);mode.restart_match(False)
    check('restart clears old projectiles',len(actors(w,unreal.WarlockProjectile))==0)
    check('restart restores both human balances and starter skills',get(s1,'gold')==get(s2,'gold')==20 and list(get(s1,'levels'))==[1,1,0,0,0,0,0,0])
    check('restart preserves cosmetic choice',get(s1,'appearance')==1)

    # Full eleven-round progression with the actual phase machine, accelerated waits.
    for round_number in range(1,12):
        put(match,'remaining',.01);await tick()
        check(f'round {round_number} starts and resets HP K cooldowns',get(match,'phase')=='FIGHT' and get(match,'round')==round_number and near(get(p1,'health'),100) and near(get(p1,'knockback'),0) and all(c==0 for c in get(p1,'cooldowns')))
        put(p1,'protection',0);put(p1,'locomotion_speed',210)
        for p in pawns:
            if p!=p1:put(p,'protection',0);p.hit(p1,1000,unreal.Vector(1,0,0),0)
        balance=get(s1,'gold');await tick()
        check(f'round {round_number} settles without kill gold',get(match,'phase')=='RESULT' and get(s1,'gold')==balance and near(get(p1,'locomotion_speed'),0))
        put(match,'remaining',.01);await tick()
        if round_number<11:
            check(f'round {round_number} next shop preserves savings and adds 10',get(match,'phase')=='SHOP' and get(s1,'gold')==20+10*round_number and get(s2,'gold')==20+10*round_number and 29<get(match,'remaining')<=30)
        else:check('eleven rounds reach FINAL with named winner',get(match,'phase')=='FINAL' and get(s1,'points')==77 and s1.get_player_name() in get(match,'announcement'))

    put(c1,'shop_open',False);put(c1,'help_open',False)
    confirm=next(a for a in get(c1,'actions') if a.get_name()=='Confirm')
    c1.local_input().inject_input_vector_for_action(confirm,unreal.Vector(1,0,0),[],[])
    await tick(.15)
    check('FINAL confirm input starts a clean new match',get(match,'phase')=='SHOP' and get(match,'round')==0 and get(s1,'gold')==20)
    mode.restart_match(True);mode.start_round()
    enemy=next(p for p in pawns if get(get(p,'player_state'),'bot'))
    put(p1,'protection',0);put(enemy,'protection',0);p1.hit(enemy,1000,unreal.Vector(1,0,0),0)
    await tick(.2)
    check('one dead local teammate does not end co-op round',get(match,'phase')=='FIGHT' and get(p2,'alive'))
    check('dead player camera follows living teammate',get(p1,'spectating')==p2)
    check('death selects full-body death animation',get(get(p1,'avatar').get_anim_instance(),'playing_death'))
    mode.restart_match(False);mode.start_round()
    corners=[(1800,1800,0),(-1800,1800,0),(-1800,-1800,0),(1800,-1800,0)]
    for p,corner in zip(pawns,corners):reset(p,corner);put(p,'health',1.0)
    await tick(.18)
    check('same ground batch all-dead is a draw',get(match,'phase')=='RESULT' and '平局' in get(match,'announcement') and all(not get(p,'alive') for p in pawns))
    put(match,'round',11)
    for s in [get(p,'player_state') for p in pawns]:put(s,'points',0)
    put(match,'remaining',.01);await tick()
    check('tied round 11 proceeds to overtime shop',get(match,'phase')=='SHOP')
    put(match,'remaining',.01);await tick()
    check('overtime starts at round 12',get(match,'phase')=='FIGHT' and get(match,'round')==12)
    mode.restart_match(False)
    check('FINAL/overtime restart returns to clean round zero',get(match,'phase')=='SHOP' and get(match,'round')==0 and near(get(p1,'health'),100) and get(p1,'spectating') is None)

    # Pose blending tests run in SHOP, so AI and authoritative motion do not overwrite
    # the deliberately supplied presentation speed while the mesh is evaluated.
    avatar=get(p1,'avatar');anim=avatar.get_anim_instance()
    put(p1,'locomotion_speed',210);await tick(.3)
    check('new round clears death animation',not get(anim,'playing_death'))
    check('walk blend eases in',get(anim,'locomotion_blend')>.9)
    put(p1,'locomotion_speed',0);await tick(.5)
    check('release eases back to idle',get(anim,'locomotion_blend')<.02)
    put(p1,'last_cast_slot',0);put(p1,'cast_serial',get(p1,'cast_serial')+1);await tick(.15)
    check('cast blend starts without moving feet',get(anim,'casting_blend')>.8 and get(anim,'locomotion_blend')<.02)
    await tick(1.8)
    check('cast blend returns to idle',get(anim,'casting_blend')<.01)
    unreal.GameplayStatics.set_game_paused(w,False)
    report={'passed':len(checks),'checks':checks,'flow':'eleven rounds and overtime with accelerated phase waits','hardware_gamepads':'not exercised','original_map_runtime':'not installed; formulas checked against map script'}
    path.write_text(json.dumps(report,indent=2),encoding='utf8')
    report
finally:
    tuning.set_editor_property('Windups',original_windups)
    tuning.set_editor_property('ProjectileSpeedScale',original_speed)
    tuning.set_editor_property('FireballSpeedScale',original_fireball)
report['preset']='isolated original numeric rules; windups disabled, projectile scale 1'
path.write_text(json.dumps(report,indent=2),encoding='utf8')
report
