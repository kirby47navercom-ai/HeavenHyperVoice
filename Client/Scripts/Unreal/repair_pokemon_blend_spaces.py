"""Repair saved locomotion samples/caches and missing battle idle references. No PIE."""
import json
from pathlib import Path
import unreal


def main():
    assets = unreal.EditorAssetLibrary
    helper = unreal.PokemonAnimationEditorLibrary
    report = {"inspected": 0, "rebuilt": [], "battle_idle_fixed": [], "unresolved": [], "clips": []}
    for asset_path in assets.list_assets("/Game/Pokemon/SpeciesData", recursive=True, include_folder=False):
        if "/DA_" not in asset_path:
            continue
        data = assets.load_asset(asset_path)
        if not isinstance(data, unreal.UEPokemonSpeciesData):
            continue
        name = data.get_name().removeprefix("DA_")
        mesh = data.get_editor_property("skeletal_mesh")
        cls = data.get_editor_property("anim_instance_class")
        if not mesh or not cls:
            report["unresolved"].append({"name": name, "reason": "Missing mesh or AnimBP"})
            continue
        defaults = unreal.get_default_object(cls)
        blend = defaults.get_editor_property("locomotion_blend_space")
        if not isinstance(blend, unreal.BlendSpace1D):
            report["unresolved"].append({"name": name, "reason": "Missing locomotion BlendSpace1D"})
            continue
        report["inspected"] += 1
        skeleton = mesh.get_editor_property("skeleton")
        if blend.get_editor_property("skeleton") != skeleton:
            report["unresolved"].append({"name": name, "reason": "BlendSpace skeleton mismatch"})
            continue
        clips = [data.get_editor_property(k) for k in ("idle", "walk", "run")]
        clip_errors = []
        for clip in clips:
            if not clip:
                clip_errors.append("Missing locomotion sequence")
                continue
            frames = unreal.AnimationLibrary.get_num_frames(clip)
            tracks = unreal.AnimationLibrary.get_animation_track_names(clip)
            report["clips"].append({"name": name, "clip": clip.get_path_name(), "frames": frames,
                                    "tracks": len(tracks), "length": clip.get_play_length()})
            if clip.get_editor_property("skeleton") != skeleton or frames < 2 or not tracks:
                clip_errors.append(clip.get_path_name())
        if clip_errors:
            report["unresolved"].append({"name": name, "reason": "Invalid clip data", "clips": clip_errors})
            continue
        speeds = [0.0, defaults.get_editor_property("walk_animation_speed"),
                  defaults.get_editor_property("run_animation_speed")]
        samples = list(blend.get_editor_property("sample_data"))
        samples_changed = len(samples) != 3
        if len(samples) == 3:
            samples_changed = any(s.get_editor_property("animation") != clip or
                                  abs(s.get_editor_property("sample_value").x - speed) > 0.01
                                  for s, clip, speed in zip(samples, clips, speeds))
        before = helper.get_blend_space_segment_count(blend)
        stale = helper.needs_blend_space_rebuild(blend)
        if samples_changed:
            rebuilt_samples = []
            for clip, speed in zip(clips, speeds):
                sample = unreal.BlendSample()
                sample.set_editor_property("animation", clip)
                sample.set_editor_property("sample_value", unreal.Vector(speed, 0, 0))
                rebuilt_samples.append(sample)
            blend.set_editor_property("sample_data", rebuilt_samples)
        if samples_changed or stale:
            if not helper.rebuild_blend_space(blend):
                report["unresolved"].append({"name": name, "reason": "Cannot rebuild interpolation data"})
                continue
            assets.save_loaded_asset(blend, only_if_is_dirty=False)
            report["rebuilt"].append({"name": name, "samples_changed": samples_changed,
                                      "segments_before": before, "segments_after": helper.get_blend_space_segment_count(blend)})
        if not data.get_editor_property("battle_idle"):
            # Prefer an existing battle idle of this skeleton; otherwise use its own regular idle.
            animation_dir = clips[0].get_path_name().rsplit("/", 1)[0]
            battle_idle = None
            for p in assets.list_assets(animation_dir, recursive=False, include_folder=False):
                if "battlewait01_loop" in p:
                    candidate = assets.load_asset(p)
                    if candidate.get_editor_property("skeleton") == skeleton:
                        battle_idle = candidate
                        break
            data.set_editor_property("battle_idle", battle_idle or clips[0])
            assets.save_loaded_asset(data, only_if_is_dirty=False)
            report["battle_idle_fixed"].append(name)
    out = Path(unreal.Paths.project_saved_dir()) / "Codex/CityPolish"
    out.mkdir(parents=True, exist_ok=True)
    (out / "animation_repairs.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("[POKEMON BLENDS] " + json.dumps({k:v for k,v in report.items() if k != "clips"}, ensure_ascii=False))


if __name__ == "__main__":
    main()
