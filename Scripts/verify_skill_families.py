"""Actual PIE transactions, cancellation, contacts and state cleanup for map families."""
import asyncio,json,math
from pathlib import Path
import unreal
w=unreal.EditorLevelLibrary.get_game_world();assert w and 'UEDPIE_' in w.get_path_name()
actors=unreal.GameplayStatics.get_all_actors_of_class
mode=actors(w,unreal.WarlockMode)[0];match=actors(w,unreal.WarlockMatchState)[0]
c=unreal.GameplayStatics.get_player_controller(w,0)
mode.restart_match(False);c.add_mixed_player();await asyncio.sleep(.15)
p=next(p for p in actors(w,unreal.WarlockPawn) if p.get_controller()==c)
q=next(x for x in actors(w,unreal.WarlockPawn) if x!=p)
s=p.get_editor_property('player_state');t=q.get_editor_property('player_state')
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/skill-families.json';checks=[]
def get(o,k):return o.get_editor_property(k)
def put(o,k,v):o.set_editor_property(k,v)
def check(name,result):
    if not result:
        out.write_text(json.dumps({'passed':len(checks),'failed':name,'checks':checks},ensure_ascii=False,indent=2),encoding='utf8')
        raise AssertionError(name)
    checks.append(name)
catalog=json.loads((root/'Analysis/Warlock1.00b CN/reference_data.json').read_text(encoding='utf8'))
ids=['S000','S001','S002','S011','S015','S008','S005','S019','S007','S004','S003','S012','S013','S014','S016','S010','S009','S030','S006','S048','S017','S018','S050']
byid={x['ability_id']:x for x in catalog['skills']}
slot={'D':2,'R':3,'T':4,'E':5,'C':6,'Y':7}
product=lambda f:f if f<8 else 20 if f==8 else 100+f
unreal.GameplayStatics.set_game_paused(w,True)
def clean():
    mode.restart_match(False)
    for x in actors(w,unreal.WarlockProjectile)+actors(w,unreal.WarlockSpellFX)+actors(w,unreal.WarlockImage):x.destroy_actor()
    put(s,'gold',10000)
def fight():
    mode.start_round()
    for i,x in enumerate(actors(w,unreal.WarlockPawn)):
        x.reset_for_round(unreal.Vector(-300 if x==p else 0 if x==q else 1500,i*0 if x in [p,q] else 1500,0));put(x,'protection',0)
def step(n,projectiles=True):
    for _ in range(n):
        p.step_simulation(.03);q.step_simulation(.03)
        if projectiles:mode.simulate_projectiles(.03)
try:
    for f,key in enumerate(ids[2:],2):
        clean();ref=byid[key];i=slot[ref['slot']];price=ref['learn_gold_base']
        check(key+' map learning price',mode.price(p,product(f))==price)
        mode.buy(p,product(f));check(key+' exact debit and family',get(s,'gold')==10000-price and get(s,'families')[i]==f and get(s,'levels')[i]==1)
        check(key+' map upgrade price',mode.next_cost(p,product(f))==ref['upgrade_gold_base'])
        other=next((n for n,k in enumerate(ids[2:],2) if n!=f and slot[byid[k]['slot']]==i),None)
        if other is not None:check(key+' slot family exclusive',mode.price(p,product(other))<0)
        fight();check(key+' starts windup',p.cast(i) and get(p,'pending_cast')==i)
        check(key+' no early cooldown',get(p,'cooldowns')[i]==0)
        p.cancel_cast();check(key+' S cancellation is free',get(p,'pending_cast')==-1 and get(p,'cooldowns')[i]==0)
        check(key+' casts after cancellation',p.cast(i));step(math.ceil(get(p,'cast_duration')/.03)+1,False)
        expected=float(ref['ability_object']['fields']['acdn']['1'])
        check(key+' map cooldown at release',abs(get(p,'cooldowns')[i]-expected)<.07)
        check(key+' finite position',all(math.isfinite(v) for v in [p.get_actor_location().x,p.get_actor_location().y,get(p,'health')]))
        dual=key in ['S011','S012','S013','S014','S015','S016','S008','S010','S009','S019','S017','S018','S050']
        if dual:
            put(match,'phase','SHOP');mode.set_skill_mode(p,product(f));check(key+' free alternative mode',get(s,'modes')[i])
            put(match,'phase','FIGHT');p.reset_for_round(unreal.Vector(-300,0,0));put(p,'protection',0);p.cast(i);step(math.ceil(get(p,'cast_duration')/.03)+1,False)
            expected=0 if f==12 else float(ref['ability_object']['fields']['acdn']['11']);check(key+' alternative map cooldown or empty-target refund',abs(get(p,'cooldowns')[i]-expected)<.07)
        p.reset_for_round(unreal.Vector())
        check(key+' round resets timed effects',all(get(p,k)==0 for k in ['root_remaining','disrupt_remaining','stealth_remaining','dash_remaining','rewind_remaining','mirror_remaining','mirror_pending','spell_speed']) and get(p,'spell_output')==1)
    # Real swept collision checks with no renderer or AI scheduling involved.
    for f in [9,10,13,14,20,22]:
        clean();i=slot[byid[ids[f]]['slot']];mode.buy(p,product(f));fight();p.cast(i);step(50)
        if f==20:check('Bind prevents walking after impact',get(q,'root_remaining')>0)
        elif f==22:check('Control applies disruption',get(q,'disrupt_remaining')>0)
        else:check(ids[f]+' damages through real swept collision',get(q,'health')<100)
    clean();mode.buy(p,118);fight();put(p,'health',70);p.cast(6);step(12,False)
    p.set_actor_location(unreal.Vector(200,300,40),False,False);put(p,'health',40);put(p,'knockback',50)
    check('Rewind can activate return during cooldown',p.cast(6))
    check('Rewind returns position and restores snapshot HP',abs(p.get_actor_location().x+300)<1 and abs(get(p,'health')-70)<.01)
    check('Rewind uses map K restoration coefficient',abs(get(p,'knockback')-35)<.01)
    clean();mode.buy(p,119);fight();p.cast(6);step(12,False)
    check('Mirror waits for a point command',get(p,'mirror_pending')>0 and len(actors(w,unreal.WarlockImage))==0)
    p.cast(0);step(13,False)
    check('Mirror creates two non-solid image actors',len(actors(w,unreal.WarlockImage))==2 and get(p,'mirror_remaining')>0)
    check('Mirror fireball emits two additional real projectiles',len(actors(w,unreal.WarlockProjectile))==3)
    clean();mode.buy(p,122);mode.set_skill_mode(p,122);fight();p.cast(7);step(14,False)
    p.set_actor_location(unreal.Vector(-800,0,40),False,False);p.cast(0);step(13,False)
    spell=list(actors(w,unreal.WarlockProjectile))[0]
    check('Matrix casts from stored mark without teleporting caster',abs(spell.get_actor_location().x+300)<1 and abs(p.get_actor_location().x+800)<1)
    check('Matrix temporary mastery is not permanent purchased mastery',get(s,'life_mastery')==get(s,'range_mastery')==get(s,'time_mastery')==0)
    clean();mode.buy(p,115);mode.set_skill_mode(p,115);fight();put(p,'health',60)
    q.set_actor_location(unreal.Vector(-260,0,40),False,False);p.cast(5);step(math.ceil(get(p,'cast_duration')/.03)+2,False)
    check('Wind Walk stealth contact heals caster and damages opponent',get(p,'health')>60 and get(q,'health')<100)
    check('stealth contact does not end the four-second stealth mode',get(p,'stealth_remaining')>0)
    clean();mode.buy(p,111);mode.set_skill_mode(p,111);fight();p.cast(3);step(math.ceil(get(p,'cast_duration')/.03)+1,False)
    before=get(p,'momentum').y;p.hit(q,7,unreal.Vector(0,1,0),1,0)
    check('Wind Dash mass reduction does not multiply ordinary spell knockback by 0.4',abs(get(p,'momentum').y-before-21)<.01)
    clean();mode.buy(p,112);fight();arena=actors(w,unreal.WarlockArena)[0];old=get(arena,'pillars')[0]
    p.reset_for_round(unreal.Vector(-1000,-1000,0));put(p,'protection',0);p.cast(3);step(30)
    check('Relocate exchanges caster with a real destructible pillar',abs(p.get_actor_location().x-old.x)<1 and abs(get(arena,'pillars')[0].x+1000)<1)
    clean();fight();check('next round resets swapped pillar positions',abs(get(arena,'pillars')[0].x-old.x)<1)
    out.write_text(json.dumps({'passed':len(checks),'checks':checks,'scope':'PIE real authority paths; not full original-runtime equivalence'},ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'passed':len(checks),'report':str(out)}))
finally:
    mode.restart_match(False);unreal.GameplayStatics.set_game_paused(w,False)
