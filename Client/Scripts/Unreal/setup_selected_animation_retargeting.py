"""선택한 외부 애니메이션 스켈레톤을 HHV 남녀 플레이어에 연결한다.

언리얼 콘텐츠 브라우저에서 AnimSequence 또는 그 원본 SkeletalMesh를 선택한 뒤
이 스크립트를 실행한다. 같은 스켈레톤의 애니메이션들은 생성된 소스 IK Rig와
Retargeter를 계속 재사용하므로, 의상마다 애니메이션을 다시 만들 필요가 없다.
"""

import hashlib
import os
import re

import unreal


RETARGET_ROOT = "/Game/Data/Animation/HeavenHyperVoice/Player/Retargeting"
SOURCE_ROOT = f"{RETARGET_ROOT}/Sources"
TARGETS = {
    "Female": f"{RETARGET_ROOT}/IK_HHV_Player_Female",
    "Male": f"{RETARGET_ROOT}/IK_HHV_Player_Male",
}


def _safe_name(value):
    """에셋 이름에 쓸 수 없는 문자를 제거하고 경로 충돌을 막는다."""
    name = re.sub(r"[^A-Za-z0-9_]", "_", str(value)).strip("_")
    return name or "Source"


def _source_key(mesh):
    """이름이 같은 외부 스켈레톤도 서로 덮어쓰지 않게 짧은 경로 해시를 붙인다."""
    mesh_path = mesh.get_path_name()
    path_hash = hashlib.sha1(mesh_path.encode("utf-8")).hexdigest()[:8]
    return f"{_safe_name(mesh.get_name())}_{path_hash}"


def _mesh_from_animation(animation):
    skeleton = animation.get_editor_property("skeleton")
    if not skeleton:
        return None
    return skeleton.get_skeleton_preview_mesh(True)


def _source_assets():
    """에디터 선택 또는 자동 검증용 환경 변수에서 입력 에셋을 가져온다."""
    explicit_paths = os.environ.get("HHV_RETARGET_SOURCE_ASSETS", "")
    if explicit_paths:
        assets = []
        for asset_path in explicit_paths.split(";"):
            asset = unreal.load_asset(asset_path.strip())
            if not asset:
                raise RuntimeError(f"입력 에셋을 찾지 못했습니다: {asset_path}")
            assets.append(asset)
        return assets
    return unreal.EditorUtilityLibrary.get_selected_assets()


def _selected_source_meshes():
    """선택한 메쉬와 애니메이션의 프리뷰 메쉬를 중복 없이 모은다."""
    meshes = {}
    for asset in _source_assets():
        mesh = None
        if isinstance(asset, unreal.SkeletalMesh):
            mesh = asset
        elif isinstance(asset, unreal.AnimSequence):
            mesh = _mesh_from_animation(asset)

        if mesh:
            meshes[mesh.get_path_name()] = mesh
        elif isinstance(asset, unreal.AnimSequence):
            unreal.log_warning(
                f"HHV_RETARGET: {asset.get_name()}의 프리뷰 SkeletalMesh를 찾지 못했습니다. "
                "원본 SkeletalMesh도 함께 선택해 주세요."
            )
    return list(meshes.values())


def _create_or_update_source_rig(mesh):
    source_key = _source_key(mesh)
    asset_name = f"IK_Source_{source_key}"
    asset_path = f"{SOURCE_ROOT}/{source_key}/{asset_name}"
    folder = f"{SOURCE_ROOT}/{source_key}"
    unreal.EditorAssetLibrary.make_directory(folder)

    rig = unreal.load_asset(asset_path)
    if not rig:
        rig = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name,
            folder,
            unreal.IKRigDefinition,
            unreal.IKRigDefinitionFactory(),
        )
    if not rig:
        raise RuntimeError(f"소스 IK Rig를 만들지 못했습니다: {asset_path}")

    controller = unreal.IKRigController.get_controller(rig)
    if not controller.set_skeletal_mesh(mesh):
        raise RuntimeError(f"소스 IK Rig에 메쉬를 연결하지 못했습니다: {asset_path}")
    if not controller.apply_auto_generated_retarget_definition():
        raise RuntimeError(
            f"{mesh.get_name()}에서 인간형 본 체인을 자동 인식하지 못했습니다. "
            "이 메쉬의 IK Rig 체인을 수동으로 지정해야 합니다."
        )

    unreal.EditorAssetLibrary.save_loaded_asset(rig, only_if_is_dirty=False)
    return source_key, folder, rig


def _create_or_update_retargeter(source_key, folder, source_rig, target_label, target_path):
    target_rig = unreal.load_asset(target_path)
    if not target_rig:
        raise RuntimeError(f"HHV {target_label} 타깃 IK Rig를 찾지 못했습니다: {target_path}")

    asset_name = f"RTG_{source_key}_To_HHV_{target_label}"
    asset_path = f"{folder}/{asset_name}"
    retargeter = unreal.load_asset(asset_path)
    if not retargeter:
        retargeter = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name,
            folder,
            unreal.IKRetargeter,
            unreal.IKRetargetFactory(),
        )
    if not retargeter:
        raise RuntimeError(f"IK Retargeter를 만들지 못했습니다: {asset_path}")

    source_or_target = unreal.RetargetSourceOrTarget
    controller = unreal.IKRetargeterController.get_controller(retargeter)
    controller.set_ik_rig(source_or_target.SOURCE, source_rig)
    controller.set_ik_rig(source_or_target.TARGET, target_rig)
    controller.add_default_ops()
    controller.assign_ik_rig_to_all_ops(source_or_target.SOURCE, source_rig)
    controller.assign_ik_rig_to_all_ops(source_or_target.TARGET, target_rig)
    controller.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)

    source_mesh = unreal.IKRigController.get_controller(source_rig).get_skeletal_mesh()
    target_mesh = unreal.IKRigController.get_controller(target_rig).get_skeletal_mesh()
    controller.set_preview_mesh(source_or_target.SOURCE, source_mesh)
    controller.set_preview_mesh(source_or_target.TARGET, target_mesh)
    controller.auto_align_all_bones(source_or_target.TARGET)

    unreal.EditorAssetLibrary.save_loaded_asset(retargeter, only_if_is_dirty=False)
    unreal.log(
        f"HHV_RETARGET_OK source={source_mesh.get_path_name()} "
        f"target={target_label} retargeter={asset_path}"
    )


def setup_selected_assets():
    meshes = _selected_source_meshes()
    if not meshes:
        raise RuntimeError(
            "콘텐츠 브라우저에서 AnimSequence 또는 원본 SkeletalMesh를 하나 이상 선택해 주세요."
        )

    for mesh in meshes:
        source_key, folder, source_rig = _create_or_update_source_rig(mesh)
        for target_label, target_path in TARGETS.items():
            _create_or_update_retargeter(
                source_key,
                folder,
                source_rig,
                target_label,
                target_path,
            )


setup_selected_assets()
