"""Run inside UE editor Python. Creates missing assets only; preserves later edits."""
import math
import unreal

ROOT = '/Game/WarlockArena'
M = unreal.MaterialEditingLibrary
ASSETS = unreal.AssetToolsHelpers.get_asset_tools()
CREATED = []


def constant(mat, value, prop):
    node = M.create_material_expression(mat, unreal.MaterialExpressionConstant)
    node.set_editor_property('r', value)
    assert M.connect_material_property(node, '', prop)


def material(name, code=None, emissive=False, translucent=False):
    path = ROOT + '/Materials/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        mat = unreal.EditorAssetLibrary.load_asset(path)
        if unreal.EditorAssetLibrary.get_metadata_tag(mat, 'WarlockBootstrap') == 'complete':
            return mat
        M.delete_all_material_expressions(mat)
    else:
        mat = ASSETS.create_asset(name, ROOT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    if translucent:
        mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
        mat.set_editor_property('two_sided', True)
        constant(mat, .13, unreal.MaterialProperty.MP_OPACITY)
    if emissive:
        mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    tint = M.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, -150)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(.12, .19, .25, 1))
    if code:
        node = M.create_material_expression(mat, unreal.MaterialExpressionCustom, -250, 0)
        node.set_editor_property('code', code)
        node.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        inputs = []
        for name_ in ['P', 'T', 'Tint']:
            entry = unreal.CustomInput()
            entry.set_editor_property('input_name', name_)
            inputs.append(entry)
        node.set_editor_property('inputs', inputs)
        pos = M.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -600, 150)
        time = M.create_material_expression(mat, unreal.MaterialExpressionTime, -600, 300)
        assert M.connect_material_expressions(pos, '', node, 'P')
        assert M.connect_material_expressions(time, '', node, 'T')
        assert M.connect_material_expressions(tint, '', node, 'Tint')
    else:
        node = tint
    assert M.connect_material_property(node, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR if emissive else unreal.MaterialProperty.MP_BASE_COLOR)
    constant(mat, .72 if name == 'M_Stone' else .38, unreal.MaterialProperty.MP_ROUGHNESS)
    constant(mat, .25 if name == 'M_Stone' else .12, unreal.MaterialProperty.MP_METALLIC)
    errors = M.recompile_material(mat)
    if errors:
        raise RuntimeError(str(errors))
    unreal.EditorAssetLibrary.set_metadata_tag(mat, 'WarlockBootstrap', 'complete')
    CREATED.append(path)
    return mat


stone = material('M_Stone', '''
float2 uv = P.xy / 110;
float2 tile = floor(uv); float2 f = frac(uv);
float edge = smoothstep(0.012,0.035,min(min(f.x,1-f.x),min(f.y,1-f.y)));
float hash = frac(sin(dot(tile,float2(127.1,311.7)))*43758.5453);
float grain = frac(sin(dot(P.xy,float2(12.9898,78.233)))*43758.5453);
float r=length(P.xy); float ring=1-smoothstep(2,5,abs(r-925));
float spokes=pow(saturate(cos(atan2(P.y,P.x)*24)),32)*(1-smoothstep(10,80,abs(r-900)));
return Tint.rgb*(0.55+hash*0.6+grain*0.12)*edge+float3(.21,.13,.05)*(ring+spokes);
''')
wizard = material('M_Wizard', '''
float weave = .82+.12*sin(P.z*1.8)*sin(P.x*1.8);
float hem = 1-smoothstep(0,4,abs(fmod(abs(P.z),30)-2));
return Tint.rgb*weave*.65+float3(.32,.24,.10)*hem;
''')
glow = material('M_Glow', 'return Tint.rgb*2.5;', emissive=True)
shield = material('M_Shield', 'return Tint.rgb*1.8;', emissive=True, translucent=True)
lava = material('M_Lava', '''
float2 p=P.xy/160; p+=float2(sin(p.y*.6+T*.12),cos(p.x*.7+T*.09))*.28;
float2 cell=floor(p);float2 f=frac(p);float dist=2;float second=2;
for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++){
float2 g=float2(x,y);float2 h=frac(sin(float2(dot(cell+g,float2(127.1,311.7)),dot(cell+g,float2(269.5,183.3))))*43758.5453);
float d=length(g+h-f);if(d<dist){second=dist;dist=d;}else{second=min(second,d);}}
float heat=1-smoothstep(.008,.065,second-dist);
float crust=.012+.012*frac(sin(dot(floor(P.xy*.25),float2(12.9898,78.233)))*43758.5453);
return float3(crust,crust*.65,crust*.6)+float3(1.4,.15,.008)*heat*(.8+.2*sin(T*.3+p.x));
''', emissive=True)

LEVEL = ROOT + '/Maps/Arena'
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def actor(cls, label, xyz, rotation=(0, 0, 0)):
    for existing in actors.get_all_level_actors():
        if existing.get_actor_label() == label:
            return existing
    obj = actors.spawn_actor_from_class(cls, unreal.Vector(*xyz), unreal.Rotator(pitch=rotation[0],yaw=rotation[1],roll=rotation[2]))
    obj.set_actor_label(label)
    return obj


def mesh(label, shape, xyz, scale, mat, rotation=(0, 0, 0)):
    a = actor(unreal.StaticMeshActor, label, xyz, rotation)
    a.static_mesh_component.set_static_mesh(unreal.EditorAssetLibrary.load_asset('/Engine/BasicShapes/' + shape))
    a.static_mesh_component.set_material(0, mat)
    a.static_mesh_component.set_editor_property('cast_shadow', True)
    a.set_actor_scale3d(unreal.Vector(*scale))
    return a


level_exists = unreal.EditorAssetLibrary.does_asset_exist(LEVEL)
level_asset = unreal.EditorAssetLibrary.load_asset(LEVEL) if level_exists else None
loaded_actors = actors.get_all_level_actors()
current_map = loaded_actors[0].get_world().get_path_name().split('.')[0] if loaded_actors else ''
if level_exists and current_map != LEVEL:
    # Default policy refuses to discard unsaved edits in another map.
    assert levels.load_level(LEVEL)
if not level_exists or unreal.EditorAssetLibrary.get_metadata_tag(level_asset, 'WarlockBootstrap') != 'complete':
    if not level_exists:
        assert levels.new_level(LEVEL)
    mesh('Lava sea', 'Plane', (0, 0, -90), (140, 140, 1), lava)
    actor(unreal.PlayerStart, 'Player start', (700, 0, 80))
    sun = actor(unreal.DirectionalLight, 'Cold moon - key light', (0, 0, 1500), (-55, -35, 0))
    sun.light_component.set_editor_property('intensity', .7)
    sun.light_component.set_editor_property('light_color', unreal.Color(r=210,g=225,b=255,a=255))
    sun.light_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    sky = actor(unreal.SkyLight, 'Ambient fill', (0, 0, 1200))
    sky.light_component.set_editor_property('intensity', .6)
    sky.light_component.set_editor_property('lower_hemisphere_color', unreal.LinearColor(.06, .1, .18))
    fog = actor(unreal.ExponentialHeightFog, 'Volcanic atmosphere', (0, 0, -200))
    fog.component.set_editor_property('fog_density', .018)
    fog.component.set_editor_property('fog_height_falloff', .12)
    fog.component.set_editor_property('fog_inscattering_luminance', unreal.LinearColor(.035, .025, .04))
    for i in range(12):
        a = i * math.tau / 12
        r = 1900 + (i % 3) * 150
        h = 220 + (i % 4) * 90
        mesh('Obsidian formation %02d' % i, 'Cone', (math.cos(a)*r, math.sin(a)*r, -80+h/2), (3+(i%2), 3, h/100), stone, (0, i*31, 0))
        if i % 3 == 0:
            light = actor(unreal.PointLight, 'Lava glow %02d' % i, (math.cos(a)*1550, math.sin(a)*1550, 100))
            light.point_light_component.set_editor_property('intensity', 18000.0)
            light.point_light_component.set_editor_property('attenuation_radius', 1300.0)
            light.point_light_component.set_editor_property('light_color', unreal.Color(r=255,g=75,b=12,a=255))
            light.point_light_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    unreal.EditorAssetLibrary.set_metadata_tag(unreal.EditorAssetLibrary.load_asset(LEVEL), 'WarlockBootstrap', 'complete')
    assert levels.save_current_level()
    CREATED.append(LEVEL)
unreal.EditorAssetLibrary.save_directory(ROOT, only_if_is_dirty=True)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(1700, 2100, 2200), unreal.Rotator(-45, -125, 0))
RESULT = {'created': CREATED, 'map': LEVEL}
RESULT
