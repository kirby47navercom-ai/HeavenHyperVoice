import json
import os

import unreal


ASSETS = {
    "female_anim_bp": "/Game/Data/Animation/HeavenHyperVoice/Player/ABP_UEAnimInstance",
    "male_anim_bp": "/Game/Data/Animation/HeavenHyperVoice/Player/ABP_UEAnimInstance_Male",
    "female_blend_space": "/Game/Data/Animation/HeavenHyperVoice/Player/BS_Player_Female_Locomotion_1D",
    "male_blend_space": "/Game/Data/Animation/HeavenHyperVoice/Player/BS_Player_Male_Locomotion_1D",
    "animation_data": "/Game/Data/Animation/DA_PlayerAnimation",
}
REPORT_PATH = os.path.join(
    unreal.Paths.project_saved_dir(), "HHVQA", "animation_asset_audit.json"
)


def object_path(value):
    return value.get_path_name() if value else ""


def asset_dependencies(asset_path):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    options = unreal.AssetRegistryDependencyOptions(
        include_soft_package_references=True,
        include_hard_package_references=True,
        include_searchable_names=False,
        include_soft_management_references=False,
        include_hard_management_references=False,
    )
    return sorted(str(value) for value in registry.get_dependencies(asset_path, options))


def read_blend_space(asset):
    result = {"samples": [], "errors": []}
    if not asset:
        result["errors"].append("asset_missing")
        return result

    try:
        result["skeleton"] = object_path(asset.get_editor_property("skeleton"))
    except Exception as error:
        result["errors"].append(f"skeleton_read_failed:{error}")

    try:
        samples = list(asset.get_editor_property("sample_data"))
        for sample in samples:
            animation = sample.get_editor_property("animation")
            value = sample.get_editor_property("sample_value")
            result["samples"].append(
                {
                    "animation": object_path(animation),
                    "speed": value.x,
                }
            )
    except Exception as error:
        result["errors"].append(f"sample_read_failed:{error}")

    if len(result["samples"]) != 3:
        result["errors"].append(f"sample_count:{len(result['samples'])}")
    return result


def main():
    report = {}
    for name, path in ASSETS.items():
        asset = unreal.load_asset(path)
        entry = {
            "path": path,
            "class": asset.get_class().get_name() if asset else "",
            "dependencies": asset_dependencies(path) if asset else [],
            "errors": [] if asset else ["asset_missing"],
        }
        if "blend_space" in name:
            entry.update(read_blend_space(asset))
        report[name] = entry

    os.makedirs(os.path.dirname(REPORT_PATH), exist_ok=True)
    with open(REPORT_PATH, "w", encoding="utf-8") as report_file:
        json.dump(report, report_file, ensure_ascii=False, indent=2)

    error_count = sum(len(entry["errors"]) for entry in report.values())
    unreal.log(f"HHV_ANIMATION_QA_REPORT={REPORT_PATH}")
    unreal.log(f"HHV_ANIMATION_QA_ERROR_COUNT={error_count}")


main()
