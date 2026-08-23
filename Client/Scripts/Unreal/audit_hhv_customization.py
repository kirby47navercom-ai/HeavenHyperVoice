import json
import os

import unreal


CATALOG_PATH = "/Game/CharacterCustomization/HHV/Data/DA_HHVCustomizationCatalog"
REPORT_PATH = os.path.join(
    unreal.Paths.project_saved_dir(), "HHVQA", "customization_asset_audit.json"
)


def object_path(value):
    return value.get_path_name() if value else ""


def option_mesh(option, gender):
    field = "female_mesh" if gender == "female" else "male_mesh"
    return option.get_editor_property(field)


def audit_mesh(mesh, expected_gender):
    result = {
        "mesh": object_path(mesh),
        "materials": [],
        "errors": [],
        "warnings": [],
    }
    if not mesh:
        result["errors"].append("mesh_missing")
        return result

    result["skeleton"] = object_path(mesh.get_editor_property("skeleton"))

    materials = mesh.get_editor_property("materials")
    if not materials:
        result["errors"].append("material_slots_empty")
        return result

    wrong_gender_markers = (
        ("/male/", "_male_")
        if expected_gender == "female"
        else ("/female/", "_female_")
    )
    for index, skeletal_material in enumerate(materials):
        material = skeletal_material.get_editor_property("material_interface")
        slot_name = str(skeletal_material.get_editor_property("material_slot_name"))
        material_path = object_path(material)
        result["materials"].append(
            {
                "index": index,
                "slot": slot_name,
                "material": material_path,
            }
        )
        if not material:
            result["errors"].append(f"material_missing:{index}:{slot_name}")
        elif any(marker in material_path.lower() for marker in wrong_gender_markers):
            # 원본 Palworld 에셋도 남녀가 일부 재질을 공유한다. 참조는 유지하고
            # 이름만 반대 성별인 경우에는 누락 오류가 아닌 검토 경고로 남긴다.
            result["warnings"].append(
                f"wrong_gender_material:{index}:{slot_name}:{material_path}"
            )
    return result


def audit_options(options, limit=None, first_index=0):
    results = []
    selected = options if limit is None else options[:limit]
    for index, option in enumerate(selected, start=first_index):
        entry = {
            "index": index,
            "id": option.get_editor_property("id"),
            "display_name": option.get_editor_property("display_name"),
            "female": audit_mesh(option_mesh(option, "female"), "female"),
            "male": audit_mesh(option_mesh(option, "male"), "male"),
        }
        results.append(entry)
    return results


def audit_material_options(options):
    results = []
    for index, option in enumerate(options):
        material = option.get_editor_property("material")
        errors = [] if material else ["material_missing"]
        results.append(
            {
                "index": index,
                "id": option.get_editor_property("id"),
                "display_name": option.get_editor_property("display_name"),
                "material": object_path(material),
                "errors": errors,
            }
        )
    return results


def main():
    catalog = unreal.load_asset(CATALOG_PATH)
    if not catalog:
        raise RuntimeError(f"카탈로그를 불러오지 못했습니다: {CATALOG_PATH}")

    outfits = list(catalog.get_editor_property("body_equipment_options"))
    bodies = list(catalog.get_editor_property("body_options"))
    heads = list(catalog.get_editor_property("head_options"))
    eyes = list(catalog.get_editor_property("eye_options"))
    hairs = list(catalog.get_editor_property("hair_options"))
    report = {
        "catalog": CATALOG_PATH,
        "bodies": audit_options(bodies),
        "heads": audit_options(heads),
        # 0번 추출용 베이스를 제외한 실제 UI 의상 1~14번을 검사한다.
        "outfits": audit_options(outfits[1:15], first_index=1),
        "eyes": audit_material_options(eyes),
        "hairs": audit_options(hairs),
    }

    os.makedirs(os.path.dirname(REPORT_PATH), exist_ok=True)
    with open(REPORT_PATH, "w", encoding="utf-8") as report_file:
        json.dump(report, report_file, ensure_ascii=False, indent=2)

    error_count = 0
    warning_count = 0
    for category in ("bodies", "outfits", "heads", "hairs"):
        for option in report[category]:
            error_count += len(option["female"]["errors"])
            error_count += len(option["male"]["errors"])
            warning_count += len(option["female"]["warnings"])
            warning_count += len(option["male"]["warnings"])
    error_count += sum(len(option["errors"]) for option in report["eyes"])
    unreal.log(f"HHV_QA_REPORT={REPORT_PATH}")
    unreal.log(f"HHV_QA_ERROR_COUNT={error_count}")
    unreal.log(f"HHV_QA_WARNING_COUNT={warning_count}")


main()
