"""Editor commandlet regression: export saved Filed landscape without modifying map assets.
Run with -run=pythonscript -script=<absolute path to this file>.
Writes the normal client/server collision artifacts, just like the editor export action.
"""
import unreal
from pathlib import Path
world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/Stage/Filed')
assert world
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
landscapes = [a for a in actors if isinstance(a, unreal.LandscapeProxy)]
assert landscapes, 'No loaded landscapes'
marked = []
for actor in landscapes:
    profile = str(actor.get_editor_property('body_instance').get_editor_property('collision_profile_name'))
    components = actor.get_components_by_class(unreal.LandscapeComponent)
    unreal.log('LANDSCAPE_CHECK label=' + actor.get_actor_label() + ' profile=' + profile + ' render_components=' + str(len(components)))
    if profile.lower() in ('serverground','serverwall'):
        marked.append(actor)
        assert components
        assert all(c.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION for c in components), 'Fixture must have render-only landscape components'
assert marked, 'Saved map has no selected Landscape instance preset'
assert unreal.HHVCoreCollisionExportLibrary.export_current_map_collision(save_map=False), 'Landscape export failed'
client=Path(unreal.Paths.project_content_dir())/'MovementCollision/Stage/Filed.hhvcollision'
server=Path(unreal.Paths.project_dir()).resolve().parent/'Server/maps/collision/Stage/Filed.hhvcollision'
assert client.read_bytes()==server.read_bytes()
header=client.read_text().splitlines()[0]
assert int(header.split()[2]) > 0
unreal.log('LANDSCAPE_EXPORT_PASS '+header+' bytes='+str(client.stat().st_size))
