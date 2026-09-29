"""저장된 연결만 검사한다. 로그인/서버/PIE는 실행하지 않는다."""
from pathlib import Path
import json
import unreal
ROOT='/Game/VFX/Weather'
lib=unreal.EditorAssetLibrary
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
bp=lib.load_asset(ROOT+'/BP_EnvironmentScene')
assert bp and bp.generated_class()
cdo=unreal.get_default_object(bp.generated_class())
assert cdo.get_editor_property('profile') and cdo.get_editor_property('parameters')
dust=cdo.get_editor_property('dust_system')
assert dust and unreal.UEWaterVFXEditorLibrary.finish_water_system(dust)
checks=[]
for path in ['/Game/InstanceMap/Plain/Plain',ROOT+'/L_Environment_Coast',ROOT+'/L_Environment_Desert']:
    assert levels.load_level(path)
    scenes=[a for a in actors.get_all_level_actors() if isinstance(a,unreal.UEEnvironmentScene)]
    assert len(scenes)==1,(path,len(scenes))
    scene=scenes[0]
    for field in ['profile','sun','moon','sky','fog','clouds','dust_system','parameters']:
        assert scene.get_editor_property(field),(path,field)
    assert scene.get_editor_property('cloud_density_scale')>0
    profile=scene.get_editor_property('profile')
    if path.endswith('Coast'):
        assert scene.get_editor_property('ocean')
        assert profile.get_editor_property('tide_amplitude_m')>0
    if path.endswith('Desert'):assert profile.get_editor_property('sand_availability')>0
    checks.append(dict(map=path,profile=profile.get_path_name(),cloud_density=scene.get_editor_property('cloud_density_scale')))
out=Path(unreal.Paths.project_saved_dir())/'environment-verified.json'
out.write_text(json.dumps(checks,indent=2),encoding='utf8')
