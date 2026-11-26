"""MCP editor Python, on a fresh PIE run. Mutates only the transient play world."""
import asyncio
import json
from pathlib import Path
import unreal

world = unreal.EditorLevelLibrary.get_game_world()
assert world and 'UEDPIE_' in world.get_path_name(), 'Requires a fresh PIE world'
actors = unreal.GameplayStatics.get_all_actors_of_class
mode = actors(world, unreal.WarlockMode)[0]
match = actors(world, unreal.WarlockMatchState)[0]
assert match.get_editor_property('phase') == 'SHOP' and match.get_editor_property('round') == 0
pawns = list(actors(world, unreal.WarlockPawn))
assert len(pawns) == 4
c1 = unreal.GameplayStatics.get_player_controller(world, 0)
s1 = c1.get_editor_property('player_state')
p1 = next(p for p in pawns if p.get_editor_property('player_state') == s1)
checks = []


def check(name, condition):
    assert condition, name
    checks.append(name)


check('human starts with 20 gold', s1.get_editor_property('gold') == 20)
mode.buy(p1, 3)
check('Blink learned for 11 gold', s1.get_editor_property('gold') == 9 and s1.get_editor_property('levels')[3] == 1)
mode.buy(p1, 5)
check('insufficient funds is atomic', s1.get_editor_property('gold') == 9 and s1.get_editor_property('levels')[5] == 0)
c1.add_mixed_player()
await asyncio.sleep(.25)
c2 = unreal.GameplayStatics.get_player_controller(world, 1)
s2 = c2.get_editor_property('player_state')
pawns = list(actors(world, unreal.WarlockPawn))
p2 = next(p for p in pawns if p.get_editor_property('player_state') == s2)
check('local player replaces one bot; total remains four', len(pawns) == 4 and sum(not p.get_editor_property('player_state').get_editor_property('bot') for p in pawns) == 2)
check('mixed keyboard/gamepad routing is selected', unreal.GameMapsSettings.get_game_maps_settings().get_skip_assigning_gamepad_to_player1())
check('each controller has its own Enhanced Input subsystem', c1.local_input() != c2.local_input())
c1.add_local_player()
check('two-pad routing selects first pad for P1', not unreal.GameMapsSettings.get_game_maps_settings().get_skip_assigning_gamepad_to_player1())
c1.add_mixed_player()
check('switching input routes preserves four seats and both local players', len(actors(world, unreal.WarlockPawn)) == 4 and unreal.GameplayStatics.get_player_controller(world, 1) == c2 and unreal.GameMapsSettings.get_game_maps_settings().get_skip_assigning_gamepad_to_player1())
mode.buy(p2, 2)
check('second player purchase does not change first player build', s2.get_editor_property('levels')[2] == 1 and s1.get_editor_property('levels')[2] == 0)
c1.toggle_shop()
c2.toggle_shop()
selected2 = c2.get_editor_property('selected_product')
c1.set_editor_property('selected_product', 8)


def action(controller, name):
    return next(a for a in controller.get_editor_property('actions') if a.get_name() == name)


def inject(controller, name, xyz=(1, 0, 0)):
    controller.local_input().inject_input_vector_for_action(action(controller, name), unreal.Vector(*xyz), [], [])


inject(c1, 'Cast3')  # A: confirm purchase, not Blink, while shopping.
await asyncio.sleep(.12)
check('shop A buys without casting Blink', s1.get_editor_property('life_mastery') == 1 and p1.get_editor_property('cooldowns')[3] == 0)
check('shop focus and selection stay per player', c2.get_editor_property('shop_open') and c2.get_editor_property('selected_product') == selected2)
c1.toggle_shop()
c2.toggle_shop()
mode.start_round()
await asyncio.sleep(.1)
before1 = p1.get_actor_location()
before2 = p2.get_actor_location()
view=next(v for v in unreal.ObjectIterator(unreal.WarlockViewport) if not v.get_name().startswith('Default__'))
view.inject_local_key('Right',True)
try:
    await asyncio.sleep(.2)
finally:
    view.inject_local_key('Right',False)
await asyncio.sleep(.12)
after1 = p1.get_actor_location()
after2 = p2.get_actor_location()
check('P1 injected move reaches pawn', after1.x > before1.x + 1)
check('P1 move does not move P2', abs(after2.x - before2.x) + abs(after2.y - before2.y) < .2)
inject(c1, 'Cast3')
await asyncio.sleep(.36)
check('combat A casts learned Blink', p1.get_editor_property('cooldowns')[3] > 15)
check('combat A does not consume P2 cooldown', p2.get_editor_property('cooldowns')[3] == 0)
await asyncio.sleep(4.05)
unreal.GameplayStatics.set_game_paused(world, True)
p1.set_editor_property('health', 100)
p1.set_editor_property('knockback', 50)
p2.set_editor_property('health', 100)
p2.set_editor_property('knockback', 50)
p2.hit(p1, 7, unreal.Vector(1, 0, 0))
check('direct 7 damage deducts HP and adds K', abs(p2.get_editor_property('health') - 93) < .001 and abs(p2.get_editor_property('knockback') - 57) < .001)
check('full-health lifesteal still clears nominal half-heal K', abs(p1.get_editor_property('knockback') - 49.65) < .002)
gold = s1.get_editor_property('gold')
for p in pawns:
    if p != p1 and p.get_editor_property('alive'):
        p.hit(p1, 1000, unreal.Vector(1, 0, 0), 0)
check('kills award no gold in standard mode', s1.get_editor_property('gold') == gold)
unreal.GameplayStatics.set_game_paused(world, False)
await asyncio.sleep(.15)
check('one survivor ends the round', match.get_editor_property('phase') == 'RESULT')
await asyncio.sleep(4.15)
check('next shop grants exactly ten gold', match.get_editor_property('phase') == 'SHOP' and s1.get_editor_property('gold') == gold + 10)
report = {'map': world.get_path_name(), 'checks': checks, 'passed': len(checks), 'hardware_gamepads': 'not tested', 'network': 'separate check'}
path = Path(unreal.Paths.project_saved_dir()) / 'Validation' / 'pie.json'
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text(json.dumps(report, indent=2), encoding='utf8')
report
