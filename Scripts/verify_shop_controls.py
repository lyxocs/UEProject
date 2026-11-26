"""Raw viewport events for the independent local shops and new build actions."""
import asyncio
import json
from pathlib import Path
import unreal

w=unreal.EditorLevelLibrary.get_game_world()
actors=unreal.GameplayStatics.get_all_actors_of_class
mode=actors(w,unreal.WarlockMode)[0]
c1=unreal.GameplayStatics.get_player_controller(w,0)
mode.restart_match(False);c1.add_mixed_player();await asyncio.sleep(.1)
c2=unreal.GameplayStatics.get_player_controller(w,1)
p1=next(p for p in actors(w,unreal.WarlockPawn) if p.get_editor_property('player_state')==c1.get_editor_property('player_state'))
p2=next(p for p in actors(w,unreal.WarlockPawn) if p.get_editor_property('player_state')==c2.get_editor_property('player_state'))
s1=p1.get_editor_property('player_state');s2=p2.get_editor_property('player_state')
view=next(v for v in unreal.ObjectIterator(unreal.WarlockViewport) if not v.get_name().startswith('Default__'))
checks=[]
def get(o,k):return o.get_editor_property(k)
def put(o,k,v):o.set_editor_property(k,v)
def check(name,value):assert value,name;checks.append(name)
async def key(name,pad=None):
    if pad is None:view.inject_local_key(name,True)
    else:view.inject_pad_key(name,pad,True)
    await asyncio.sleep(.05)
    if pad is None:view.inject_local_key(name,False)
    else:view.inject_pad_key(name,pad,False)
    await asyncio.sleep(.05)

try:
    unreal.GameplayStatics.set_game_paused(w,False)
    for s in [s1,s2]:put(s,'gold',500)
    for c in [c1,c2]:put(c,'shop_open',True);put(c,'help_open',False);put(c,'selected_product',3)
    await key('Enter')
    check('keyboard Enter purchases only P1 selection',get(s1,'levels')[3]==1 and get(s2,'levels')[3]==0)
    await key('Gamepad_FaceButton_Bottom',0)
    check('mixed first pad A purchases only P2 selection',get(s2,'levels')[3]==1 and get(s1,'levels')[3]==1)
    before=(get(s1,'gold'),get(s2,'gold'));await key('M')
    check('keyboard M toggles only P1 effect without charge',get(s1,'modes')[3] and not get(s2,'modes')[3] and before==(get(s1,'gold'),get(s2,'gold')))
    await key('Gamepad_FaceButton_Left',0)
    check('mixed pad X toggles only P2 effect',get(s2,'modes')[3] and before==(get(s1,'gold'),get(s2,'gold')))
    put(c1,'selected_product',6);put(c2,'selected_product',6)
    await key('Q');check('keyboard Q browses C alternative only for P1',get(c1,'selected_product')==20 and get(c2,'selected_product')==6)
    await key('Gamepad_RightThumbstick',0);check('pad R3 browses C alternative for P2',get(c2,'selected_product')==20)
    await key('Enter');await key('Gamepad_FaceButton_Bottom',0)
    check('each player buys own selected C family',get(s1,'defense_choice')==get(s2,'defense_choice')==1 and get(s1,'levels')[6]==get(s2,'levels')[6]==1)
    for c in [c1,c2]:put(c,'selected_product',16)
    await key('Enter');await key('Gamepad_FaceButton_Bottom',0)
    before=(get(s1,'gold'),get(s2,'gold'));await key('Delete')
    check('keyboard Delete sells only P1 item with exact refund',get(s1,'items')[4]==0 and get(s2,'items')[4]==1 and get(s1,'gold')==before[0]+4 and get(s2,'gold')==before[1])
    await key('Gamepad_RightTrigger',0)
    check('pad RT sells P2 item without firing',get(s2,'items')[4]==0 and get(s2,'gold')==before[1]+4 and get(p2,'pending_cast')==-1)
    for c in [c1,c2]:put(c,'selected_product',15)
    await key('Enter');await key('Gamepad_FaceButton_Bottom',0)
    for c in [c1,c2]:put(c,'selected_product',6)
    await key('J');check('unlearned C family cannot consume P1 stone',get(s1,'stone_slot')==-1)
    await key('Gamepad_LeftTrigger',0);check('unlearned C family cannot consume P2 stone',get(s2,'stone_slot')==-1)
    for c in [c1,c2]:put(c,'selected_product',3)
    await key('J');check('keyboard J assigns only P1 stone',get(s1,'stone_slot')==3 and get(s2,'stone_slot')==-1)
    await key('Gamepad_LeftTrigger',0);check('pad LT assigns only P2 stone without Smite',get(s2,'stone_slot')==3 and get(p2,'pending_cast')==-1)
    c1.add_local_player();await asyncio.sleep(.05)
    for c in [c1,c2]:put(c,'shop_open',True);put(c,'selected_product',3)
    old1=get(s1,'modes')[3];old2=get(s2,'modes')[3]
    await key('Gamepad_FaceButton_Left',0);check('two-pad first X changes only P1 mode',get(s1,'modes')[3]!=old1 and get(s2,'modes')[3]==old2)
    await key('Gamepad_FaceButton_Left',1);check('two-pad second X changes only P2 mode',get(s2,'modes')[3]!=old2)
    before=(get(s1,'gold'),get(s2,'gold'));await key('Enter');await key('M')
    check('keyboard cannot buy or change builds in two-pad mode',before==(get(s1,'gold'),get(s2,'gold')) and not get(s1,'modes')[3] and not get(s2,'modes')[3])
    Path(unreal.Paths.project_saved_dir(),'Validation','shop-controls.json').write_text(json.dumps({'passed':len(checks),'checks':checks,'physical_gamepads':'software raw events; hardware not exercised'},indent=2),encoding='utf8')
finally:
    mode.restart_match(False)
    for c in [c1,c2]:put(c,'shop_open',False)
    unreal.GameplayStatics.set_game_paused(w,False)
