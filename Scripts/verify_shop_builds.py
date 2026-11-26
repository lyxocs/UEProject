"""PIE regression against the supplied 1.00b map's shop and enabled variants.

Pause the play world and advance fixed steps explicitly. Purchases use the real
authority methods; state injection only establishes independently testable cases.
"""
import asyncio
import json
from pathlib import Path
import unreal

w=unreal.EditorLevelLibrary.get_game_world()
assert w and 'UEDPIE_' in w.get_path_name()
actors=unreal.GameplayStatics.get_all_actors_of_class
mode=actors(w,unreal.WarlockMode)[0];match=actors(w,unreal.WarlockMatchState)[0]
c1=unreal.GameplayStatics.get_player_controller(w,0)
mode.restart_match(False);c1.add_mixed_player();await asyncio.sleep(.15)
c2=unreal.GameplayStatics.get_player_controller(w,1)
p1=next(p for p in actors(w,unreal.WarlockPawn) if p.get_editor_property('player_state')==c1.get_editor_property('player_state'))
p2=next(p for p in actors(w,unreal.WarlockPawn) if p.get_editor_property('player_state')==c2.get_editor_property('player_state'))
s1=p1.get_editor_property('player_state');s2=p2.get_editor_property('player_state')
unreal.GameplayStatics.set_game_paused(w,True)
checks=[];path=Path(unreal.Paths.project_saved_dir(),'Validation','shop-builds.json')
def get(o,k):return o.get_editor_property(k)
def put(o,k,v):o.set_editor_property(k,v)
def check(name,value):
    if not value:
        path.write_text(json.dumps({'passed':len(checks),'checks':checks,'failed':name},indent=2),encoding='utf8')
        raise AssertionError(name)
    checks.append(name)
def near(a,b):return abs(a-b)<.005
def clean():
    mode.restart_match(False)
    put(s1,'gold',500);put(s2,'gold',500)
def fighter():
    mode.start_round();p1.reset_for_round(unreal.Vector(0,0,0));p2.reset_for_round(unreal.Vector(0,900,0))
    put(p1,'protection',0);put(p2,'protection',0)
def step(n):
    for _ in range(n):p1.step_simulation(.03)
def shots():return list(actors(w,unreal.WarlockProjectile))
def clear_shots():
    for p in shots():p.destroy_actor()

try:
    clean()
    for slot,cost in [(2,11),(3,11),(4,14),(5,14),(6,12),(7,11),(20,12)]:
        check(f'base learning price product {slot}',mode.price(p1,slot)==cost)
    for count in range(6):
        levels=[1,1,0,0,0,0,0,0]
        for slot in list(range(2,7))[:count]:levels[slot]=1
        put(s1,'levels',levels)
        check(f'new skill surcharge after {min(count,5)} learned slots',mode.next_cost(p1,7)==11+max(0,min(count,5)-2))
    clean()
    for slot,first,cap in [(2,8,7),(3,6,7),(4,6,6),(5,6,6),(6,6,6),(7,4,7)]:
        for level in range(1,cap):
            levels=[1,1,0,0,0,0,0,0];levels[slot]=level;put(s1,'levels',levels)
            check(f'upgrade {slot} L{level} quoted cost',mode.next_cost(p1,slot)==first+level-1)
    clean();put(s1,'gold',2);put(s1,'ready',True)
    mode.buy(p1,3)
    check('insufficient funds does not spend, learn, or revoke ready',get(s1,'gold')==2 and get(s1,'levels')[3]==0 and get(s1,'ready'))
    check('insufficient funds gives exact deficit', '9' in mode.purchase_reason(p1,3))
    put(s1,'gold',500);mode.buy(p1,3)
    check('purchase debits exact price and revokes ready',get(s1,'gold')==489 and get(s1,'levels')[3]==1 and not get(s1,'ready'))
    check('P1 purchase does not change P2 build',get(s2,'gold')==500 and get(s2,'levels')[3]==0)
    check('fireball upgrade blocked by mastery',mode.price(p1,0)==-1 and '1' in mode.purchase_reason(p1,0))
    mode.buy(p1,8);check('life mastery unlocks first fireball upgrade',mode.price(p1,0)==3)
    put(s1,'levels',[1,1,3,1,0,0,0,0]);check('lightning L4 requires range mastery one',mode.price(p1,2)==-1)
    mode.buy(p1,9);check('range mastery unlocks lightning L4',mode.price(p1,2)==10)
    for product,base,key in [(8,6,'life_mastery'),(9,5,'range_mastery'),(10,4,'time_mastery')]:
        put(s1,key,5);check(f'mastery progressive price {product}',mode.price(p1,product)==base+5)
        put(s1,key,6);check(f'mastery cap {product}',mode.price(p1,product)==-1)
        put(s1,key,0)
    check('mastery package base cost 13',mode.price(p1,11)==13)
    mode.buy(p1,11)
    check('mastery package adds each mastery once',all(get(s1,k)==1 for k in ['life_mastery','range_mastery','time_mastery']))
    check('mastery package increases cost by three',mode.price(p1,11)==16)
    put(s1,'life_mastery',6);check('package blocked if one mastery capped',mode.price(p1,11)==-1)

    # Ah refunds the complete equipped item, not only its last purchased level.
    for product,cost,refund,cap in [(12,4,3,1),(13,7,5,3),(14,8,7,2),(16,5,4,3),(17,3,2,3),(18,4,3,3),(19,6,5,3)]:
        for level in range(1,cap+1):
            clean();before=get(s1,'gold')
            for _ in range(level):mode.buy(p1,product)
            check(f'item {product} L{level} fixed upgrade price',get(s1,'gold')==before-cost*level)
            check(f'item {product} L{level} whole sale quote',mode.sale_price(p1,product)==refund*level)
            mode.sell(p1,product)
            check(f'item {product} L{level} sale removes item and refunds',get(s1,'items')[product-12]==0 and get(s1,'gold')==before-cost*level+refund*level)
            balance=get(s1,'gold');mode.sell(p1,product)
            check(f'item {product} cannot be sold twice',get(s1,'gold')==balance)
    clean();mode.buy(p1,16);check('speed boots blocks lava boots',mode.price(p1,13)==-1)
    mode.sell(p1,16);check('selling boots permits other boots',mode.price(p1,13)==7)
    mode.buy(p1,13);mode.buy(p1,18)
    check('third distinct item blocked',mode.price(p1,17)==-1)
    check('upgrading equipped item allowed in full backpack',mode.price(p1,18)==4)
    mode.buy(p1,18);put(p1,'health',120);mode.sell(p1,18)
    check('selling blood card clamps current HP to new maximum',near(get(p1,'health'),100))

    clean();mode.buy(p1,15)
    check('stone costs one and occupies inventory slot',get(s1,'gold')==499 and get(s1,'items')[3]==1 and get(s1,'has_stone'))
    check('stone cannot be sold or repurchased',mode.sale_price(p1,15)==-1 and mode.price(p1,15)==-1)
    mode.assign_stone(p1,3);check('unlearned skill cannot consume stone',get(s1,'stone_slot')==-1)
    mode.assign_stone(p1,1);check('Smite cannot consume stone',get(s1,'stone_slot')==-1)
    put(s1,'life_mastery',6);put(s1,'levels',[10,1,0,0,0,0,0,0]);mode.assign_stone(p1,0)
    check('stone expands cap without giving free levels',get(s1,'stone_slot')==0 and get(s1,'levels')[0]==10 and mode.price(p1,0)==12)
    mode.buy(p1,0);mode.buy(p1,0);check('stone allows exactly two extra levels',get(s1,'levels')[0]==12 and mode.price(p1,0)==-1)
    mode.assign_stone(p1,6);check('stone cannot be reassigned',get(s1,'stone_slot')==0)
    clean();mode.buy(p1,20)
    check('Haste occupies C slot with same twelve gold learning cost',get(s1,'defense_choice')==1 and get(s1,'levels')[6]==1 and get(s1,'gold')==488)
    check('cannot also learn Reflect in occupied C slot',mode.price(p1,6)==-1 and mode.price(p1,20)==6)
    balance=get(s1,'gold');mode.buy(p1,6);check('blocked family replacement cannot spend gold',get(s1,'gold')==balance)
    mode.buy(p1,15);mode.assign_stone(p1,20);check('stone accepts selected Haste C slot',get(s1,'stone_slot')==6)
    clean();mode.buy(p1,6);check('Reflect prevents Haste purchase in same slot',mode.price(p1,20)==-1)

    clean();mode.buy(p1,3);mode.buy(p1,4);balance=get(s1,'gold')
    mode.set_skill_mode(p1,3);mode.set_skill_mode(p1,4)
    check('switching effects is free and keeps levels',get(s1,'gold')==balance and get(s1,'levels')[3:5]==[1,1] and get(s1,'modes')[3:5]==[True,True])
    mode.set_skill_mode(p1,2);check('unlearned or single mode cannot toggle',not get(s1,'modes')[2])
    fighter();before=get(p1,'health');balance=get(s1,'gold')
    mode.buy(p1,8);mode.sell(p1,15);mode.assign_stone(p1,3);mode.set_skill_mode(p1,3)
    check('all shop operations rejected in combat',get(s1,'gold')==balance and get(s1,'modes')[3] and get(s1,'stone_slot')==-1 and near(get(p1,'health'),before))
    p1.cast(4);check('salvo has visible windup without early projectiles',len(shots())==0 and get(p1,'pending_cast')==4)
    p1.cancel_cast();check('cancelled salvo consumes no cooldown',len(shots())==0 and near(get(p1,'cooldowns')[4],0))
    p1.cast(4);step(15)
    check('salvo releases exactly four projectiles',len(shots())==4)
    check('salvo uses map mode two cooldown 16.5',near(get(p1,'cooldowns')[4],16.5))
    # Real hit checks distinguish 3.6 salvo from 2.6 sequential spray.
    p2.reset_for_round(unreal.Vector(350,0,0));put(p2,'protection',0)
    for _ in range(26):mode.simulate_projectiles(.03)
    check('salvo does real damage in multiples of 3.6',get(p2,'health')<100 and near((100-get(p2,'health'))/3.6,round((100-get(p2,'health'))/3.6)))
    clear_shots();p1.reset_for_round(unreal.Vector(-1000,0,0));p1.cast(3);step(10)
    check('fold first hop is L1 350',near(p1.get_actor_location().x,-650))
    check('replicated second-hop indicator opens for 3.3 seconds',near(get(p1,'second_blink'),3.3))
    check('fold first hop starts eighteen second cooldown',near(get(p1,'cooldowns')[3],18))
    p1.cast(3);p1.cancel_cast();check('cancelling second hop preserves first-hop cooldown',near(get(p1,'cooldowns')[3],18))
    p1.cast(3);step(10)
    check('fold second hop is 300 and does not reset cooldown',near(p1.get_actor_location().x,-350) and near(get(p1,'cooldowns')[3],17.7))
    check('fold has no third hop',not p1.cast(3))
    check('second-hop indicator closes on use',near(get(p1,'second_blink'),0))
    p1.reset_for_round(unreal.Vector(-1000,0,0));p1.cast(3);step(10);step(111)
    check('second hop expires after 3.3 seconds',not p1.cast(3))
    check('second-hop indicator closes on expiry',near(get(p1,'second_blink'),0))

    clean();mode.buy(p1,20);fighter();p1.cast(6)
    check('Haste starts with its own visible windup',near(get(p1,'haste_remaining'),0) and get(p1,'pending_cast')==6)
    step(11)
    check('Haste L1 duration pool speed and cooldown match PA',near(get(p1,'haste_remaining'),6) and near(get(p1,'haste_pool'),5) and near(get(p1,'haste_speed'),35) and near(get(p1,'cooldowns')[6],21) and near(get(p1,'shield_remaining'),0))
    p1.hit(p2,6,unreal.Vector(1,0,0),1)
    check('Haste absorbs half hit and converts to speed',near(get(p1,'health'),97) and near(get(p1,'haste_pool'),2) and near(get(p1,'haste_speed'),80))
    check('Haste preserves original pre-absorption impulse BUG02',near(get(p1,'momentum').x,18) and near(get(p1,'knockback'),3))
    p1.hit(p2,6,unreal.Vector(1,0,0),0)
    check('Haste absorption is limited by remaining pool',near(get(p1,'health'),93) and near(get(p1,'haste_pool'),0) and near(get(p1,'haste_speed'),110))
    step(201);check('Haste expiry removes all speed and absorption',near(get(p1,'haste_remaining'),0) and near(get(p1,'haste_speed'),0) and near(get(p1,'haste_pool'),0))
    p1.reset_for_round(unreal.Vector(0,0,0));put(p1,'protection',0);p1.cast(6);step(11);p1.hit(p2,1000,unreal.Vector(),0)
    check('death clears Haste transient attributes',not get(p1,'alive') and near(get(p1,'haste_speed'),0) and near(get(p1,'haste_remaining'),0))
    mode.restart_match(False)
    check('restart resets defense choice, effects and stone',get(s1,'defense_choice')==0 and not get(s1,'has_stone') and get(s1,'stone_slot')==-1 and not any(get(s1,'modes')) and near(get(p1,'haste_speed'),0))
    clean();mode.buy(p1,20);fighter();p1.cast(6);step(11)
    for other in actors(w,unreal.WarlockPawn):
        if other!=p1:put(other,'protection',0);other.hit(p1,1000,unreal.Vector(),0)
    unreal.GameplayStatics.set_game_paused(w,False);await asyncio.sleep(.12);unreal.GameplayStatics.set_game_paused(w,True)
    check('round result clears active Haste immediately',get(match,'phase')=='RESULT' and near(get(p1,'haste_remaining'),0) and near(get(p1,'haste_speed'),0))
    path.write_text(json.dumps({'passed':len(checks),'checks':checks},indent=2),encoding='utf8')
    print(json.dumps({'passed':len(checks),'report':str(path)}))
finally:
    mode.restart_match(False)
    unreal.GameplayStatics.set_game_paused(w,False)
