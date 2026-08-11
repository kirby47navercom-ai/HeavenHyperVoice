from __future__ import annotations

import unreal


SOURCE = "/Game/VRoidGenerated/SK_BodySkin_Skeleton"
DESTINATION = "/Game/CharacterCustomization/Assets/VRoid/Skeletons/SK_VRoidCommon"


def run() -> None:
    if unreal.EditorAssetLibrary.does_asset_exist(DESTINATION):
        unreal.log(f"VRoid skeleton already organized: {DESTINATION}")
        return
    if not unreal.EditorAssetLibrary.does_asset_exist(SOURCE):
        raise RuntimeError(f"Common VRoid skeleton is missing: {SOURCE}")

    unreal.EditorAssetLibrary.make_directory(
        "/Game/CharacterCustomization/Assets/VRoid/Skeletons"
    )
    if not unreal.EditorAssetLibrary.rename_asset(SOURCE, DESTINATION):
        raise RuntimeError(f"Failed to move VRoid skeleton to {DESTINATION}")
    unreal.EditorAssetLibrary.save_directory(
        "/Game/CharacterCustomization/Assets/VRoid/Skeletons"
    )
    unreal.log(f"VRoid skeleton organized: {DESTINATION}")


if __name__ == "__main__":
    run()
