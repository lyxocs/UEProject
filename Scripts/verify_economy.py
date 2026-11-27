"""PIE ledger regression with deliberately different kill/assist rewards (BUG04)."""
import asyncio,json
from pathlib import Path
import unreal
w=unreal.EditorLevelLibrary.get_game_world();assert w and 'UEDPIE_' in w.get_path_name()
actors=unreal.GameplayStatics.get_all_actors_of_class
mode=actors(w,unreal.WarlockMode)[0];match=actors(w,unreal.WarlockMatchState)[0]
economy=unreal.get_default_object(unreal.load_class(None,'/Script/WarlockArena.WarlockEconomy'))
keys=['starting_gold','round_supply','kill_gold','assist_gold','victory_gold','top_damage_gold']
get=lambda o,k:o.get_editor_property(k.title().replace('_','') if o==economy else k)
put=lambda o,k,v:o.set_editor_property(k.title().replace('_','') if o==economy else k,v)
original={k:get(economy,k) for k in keys};checks=[]
path=Path(unreal.Paths.project_saved_dir())/'Validation/economy.json'
def check(name,value):
    if not value:
        path.write_text(json.dumps({'passed':len(checks),'failed':name,'checks':checks,'phase':get(match,'phase'),'players':[{'gold':get(x,'gold'),'supply':get(x,'supply_income'),'bot':get(x,'bot')} for x in globals().get('ss',[])]},indent=2),encoding='utf8');raise AssertionError(name)
    checks.append(name)
def setup(teams=False):
    mode.restart_match(teams);mode.start_round()
    ps=sorted(actors(w,unreal.WarlockPawn),key=lambda p:get(get(p,'player_state'),'bot'))
    for i,p in enumerate(ps):
        p.reset_for_round(unreal.Vector(i*350,0,0));put(p,'protection',0);put(get(p,'player_state'),'gold',get(economy,'starting_gold'))
    return ps,[get(p,'player_state') for p in ps]
def hit(victim,source,amount):victim.hit(source,amount,unreal.Vector(1,0,0),0,-1)
async def tick():
    unreal.GameplayStatics.set_game_paused(w,False);await asyncio.sleep(.08);unreal.GameplayStatics.set_game_paused(w,True)
unreal.GameplayStatics.set_game_paused(w,True)
try:
    check('1.00b default economy',list(original.values())==[20,10,0,0,0,0])
    ps,ss=setup();hit(ps[1],ps[0],1000)
    check('default kill and assist change score but not gold',get(ss[0],'gold')==20 and get(ss[0],'points')==2 and get(ss[0],'round_kills')==1 and get(ss[0],'round_assists')==1)
    for k,v in zip(keys,[20,17,3,7,11,13]):put(economy,k,v)
    ps,ss=setup();hit(ps[1],ps[2],20);hit(ps[1],ps[0],1000)
    check('distinct source maximum contribution pays killer both awards',get(ss[0],'kill_income')==3 and get(ss[0],'assist_income')==7 and get(ss[0],'gold')==30)
    # Highest contributor can differ from the last hit.
    ps,ss=setup();hit(ps[1],ps[2],70);hit(ps[1],ps[0],35)
    check('last-hit killer receives only kill gold',get(ss[0],'gold')==23 and get(ss[0],'kill_income')==3 and get(ss[0],'assist_income')==0)
    check('largest contributor receives configured assist gold, not kill gold',get(ss[2],'gold')==27 and get(ss[2],'assist_income')==7 and get(ss[2],'kill_income')==0)
    check('victim receives no fabricated death-placement gold',get(ss[1],'gold')==20)
    await tick();check('first elimination gets fourth place',get(ss[1],'round_placement')==4)
    ps,ss=setup();hit(ps[1],ps[0],1000);hit(ps[2],ps[0],1000);await tick()
    check('same fixed-batch deaths share third place',get(ss[1],'round_placement')==get(ss[2],'round_placement')==3)
    # No award at first death is paid twice by repeated end-of-frame checks.
    before=[get(s,'gold') for s in ss];await tick();check('death rewards are idempotent',before==[get(s,'gold') for s in ss])
    ps,ss=setup();hit(ps[1],ps[0],1000)
    for x in ps[2:]:put(x,'alive',False)
    await tick()
    check('winner and top damage rewards are separate ledger entries',get(ss[0],'victory_income')==11 and get(ss[0],'damage_income')==13 and get(ss[0],'gold')==54)
    check('winner receives first placement',get(ss[0],'round_placement')==1)
    put(match,'remaining',.01);await tick()
    check('next shop pays supply equally to survivor and eliminated player',get(match,'phase')=='SHOP' and get(ss[0],'gold')==71 and get(ss[1],'gold')==37 and all(get(s,'supply_income')==17 for s in ss))
    check('unequal savings survive the round boundary',get(ss[0],'gold')-get(ss[1],'gold')==34)
    ps,ss=setup();hit(ps[1],ps[0],20);hit(ps[0],ps[1],20)
    for x in ps[1:]:put(x,'alive',False)
    await tick();check('positive tied highest damage rewards every tied player',get(ss[0],'damage_income')==get(ss[1],'damage_income')==13)
    ps,ss=setup(True);hit(ps[1],ps[2],1000)
    for x in ps[2:]:hit(x,ps[0],1000)
    await tick();check('winning team gold includes an already eliminated teammate',get(ss[0],'victory_income')==get(ss[1],'victory_income')==11)
    path.write_text(json.dumps({'passed':len(checks),'checks':checks,'source':'1.00b globals/BN/VN/EN; no unverified placement bounty'},indent=2),encoding='utf8')
    print(json.dumps({'passed':len(checks),'report':str(path)}))
finally:
    for k,v in original.items():put(economy,k,v)
    mode.restart_match(False);unreal.GameplayStatics.set_game_paused(w,False)
