"""기존 해안 예제에 실제 바닥/수역을 배치하고 DA와 서버 공통 충돌을 함께 저장해요."""
from pathlib import Path
import sys,json
import unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
from export_environment_profiles import export_profile
from create_environment_scenes import dust

ROOT='/Game/VFX/Weather'
lib=unreal.EditorAssetLibrary
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
dust(lib.load_asset(ROOT+'/MPC_InstanceWeather'))
levels.load_level(ROOT+'/L_Environment_Coast')
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
present=list(actors.get_all_level_actors())
scene=next(a for a in present if isinstance(a,unreal.UEEnvironmentScene))

def block(label,location,scale):
    actor=next((a for a in present if a.get_actor_label()==label),None)
    if not actor:
        actor=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*location));actor.set_actor_label(label)
    actor.set_actor_location(unreal.Vector(*location),False,False);actor.set_actor_scale3d(unreal.Vector(*scale))
    mesh=actor.static_mesh_component;mesh.set_static_mesh(lib.load_asset('/Engine/BasicShapes/Cube'))
    mesh.set_collision_profile_name('ServerGround')
    mesh.set_material(0,lib.load_asset(ROOT+'/Materials/M_Environment_Sand'))
    return actor

# 에디터 배치 좌표예요. 실행 코드에 좌표를 넣지 않고 실제 씬으로 충돌을 내보내요.
block('Ground',(-1750,0,-100),(30,60,2))
block('CoastalSeabed',(1750,0,-600),(45,80,2))
for i in range(25):
    top=-20*(i+1)
    block('CoastalShore_'+str(i),(-230+i*40,0,(top-700)/2),(.4,60,(top+700)/100))
for actor in actors.get_all_level_actors():
    if isinstance(actor,unreal.PlayerStart):actor.set_actor_location(unreal.Vector(-1750,0,150),False,False)
    if isinstance(actor,unreal.WaterBodyOcean):
        spline=actor.get_water_spline()
        spline.set_spline_points([unreal.Vector(x,y,0) for x,y in [(-3250,-3000),(-3250,3000),(-250,3000),(-250,-3000)]],unreal.SplineCoordinateSpace.LOCAL,True)
        for i in range(4):spline.set_spline_point_type(i,unreal.SplinePointType.LINEAR,True)
        spline.set_closed_loop(True,True)
        sea=scene.get_ocean_sea_level_cm()

asset=lib.load_asset(ROOT+'/DA_Environment_Coast')
region=unreal.UEEnvironmentWaterRegion()
region.set_editor_property('minimum',unreal.Vector2D(-250,-3000))
region.set_editor_property('maximum',unreal.Vector2D(4000,3000))
region.set_editor_property('sea_level_cm',sea)
region.set_editor_property('swim_depth_cm',90)
region.set_editor_property('can_swim',True)
asset.set_editor_property('water_regions',[region]);lib.save_loaded_asset(asset)
repo=Path(unreal.Paths.project_dir()).resolve().parent
# 기존 에디터 내보내기 도구가 양쪽 파일을 함께 저장하고 해시도 검사해요.
if not unreal.HHVCoreCollisionExportLibrary.export_current_map_collision(save_map=True):
    raise RuntimeError('Coastal common collision export failed')
out=repo/'Server/maps/collision/VFX/Weather/L_Environment_Coast.hhvcollision'
client=repo/'Client/Content/MovementCollision/VFX/Weather/L_Environment_Coast.hhvcollision'
assert out.read_bytes()==client.read_bytes(),'Client/server collision differs'
for kind in ('Temperate','Coast','Desert'):
    profile=lib.load_asset(ROOT+'/DA_Environment_'+kind)
    lib.save_loaded_asset(profile);export_profile(profile)
report={'coast':asset.get_path_name(),'sea_level_cm':sea,'collision_bytes':out.stat().st_size,'server_collision':str(out)}
(repo/'Client/Saved/environment-gameplay-authored.json').write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('ENVIRONMENT_GAMEPLAY_AUTHORING_OK')
