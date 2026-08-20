import unreal


TARGET_FOLDER = "/Game/Data/Animation/HeavenHyperVoice/Player/Retargeting"
TARGETS = {
    "IK_HHV_Player_Female": (
        "/Game/CharacterCustomization/Palworld/AssetsFBX/Pal/Model/Character/Player/Outfit/"
        "SK_Player_Female_Outfit_OldCloth001/SK_Player_Female_Outfit_OldCloth001/"
        "SK_Player_Female_Outfit_OldCloth001"
    ),
    "IK_HHV_Player_Male": (
        "/Game/CharacterCustomization/Palworld/AssetsFBX/Pal/Model/Character/Player/Outfit/"
        "SK_Player_Male_Outfit_OldCloth001/SK_Player_Male_Outfit_OldCloth001/"
        "SK_Player_Male_Outfit_OldCloth001"
    ),
}


def create_or_update_ik_rig(asset_name, mesh_path):
    asset_path = f"{TARGET_FOLDER}/{asset_name}"
    mesh = unreal.load_asset(mesh_path)
    if not mesh:
        raise RuntimeError(f"기준 스켈레탈 메쉬를 찾지 못했습니다: {mesh_path}")

    ik_rig = unreal.load_asset(asset_path)
    if not ik_rig:
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        ik_rig = asset_tools.create_asset(
            asset_name,
            TARGET_FOLDER,
            unreal.IKRigDefinition,
            unreal.IKRigDefinitionFactory(),
        )
    if not ik_rig:
        raise RuntimeError(f"IK Rig를 만들지 못했습니다: {asset_path}")

    controller = unreal.IKRigController.get_controller(ik_rig)
    if not controller.set_skeletal_mesh(mesh):
        raise RuntimeError(f"IK Rig에 기준 메쉬를 연결하지 못했습니다: {asset_path}")

    if not controller.apply_auto_generated_retarget_definition():
        raise RuntimeError(f"자동 리타게팅 체인을 만들지 못했습니다: {asset_path}")

    if not unreal.EditorAssetLibrary.save_loaded_asset(ik_rig, only_if_is_dirty=False):
        raise RuntimeError(f"IK Rig를 저장하지 못했습니다: {asset_path}")

    chains = [str(chain.chain_name) for chain in controller.get_retarget_chains()]
    unreal.log(
        f"HHV_RETARGET_TARGET_OK asset={asset_path} mesh={mesh_path} "
        f"root={controller.get_retarget_root()} chains={','.join(chains)}"
    )


for target_name, target_mesh_path in TARGETS.items():
    create_or_update_ik_rig(target_name, target_mesh_path)

