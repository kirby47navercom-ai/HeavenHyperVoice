"""푸케문 몽뭐2 리타겟 묶음의 .blend에서 언리얼용 애니메이션 FBX만 분리한다.

원본 애니메이션 FBX에는 메시와 LOD가 함께 들어 있어 언리얼의
Interchange 해석이 불필요하게 느리다. 이 스크립트는 아마추어와 현재
액션만 FBX로 내보내며, 원본 파일은 수정하지 않는다.
"""

import argparse
from pathlib import Path
import sys

import bpy


def parse_arguments():
    arguments = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", required=True)
    return parser.parse_args(arguments)


def main():
    args = parse_arguments()
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    armatures = [obj for obj in bpy.data.objects if obj.type == "ARMATURE"]
    if len(armatures) != 1:
        raise RuntimeError(f"아마추어는 정확히 하나여야 합니다: {len(armatures)}개")

    armature = armatures[0]
    if armature.animation_data is None:
        armature.animation_data_create()

    # 메시가 FBX에 섞이지 않도록 아마추어만 선택한다.
    bpy.ops.object.select_all(action="DESELECT")
    armature.select_set(True)
    bpy.context.view_layer.objects.active = armature

    actions = sorted(bpy.data.actions, key=lambda action: action.name)
    if not actions:
        raise RuntimeError("내보낼 액션이 없습니다.")

    for action in actions:
        armature.animation_data.action = action
        frame_start, frame_end = action.frame_range
        bpy.context.scene.frame_start = int(frame_start)
        bpy.context.scene.frame_end = int(frame_end)

        destination = output_dir / f"{action.name}.fbx"
        result = bpy.ops.export_scene.fbx(
            filepath=str(destination),
            use_selection=True,
            object_types={"ARMATURE"},
            add_leaf_bones=False,
            use_armature_deform_only=False,
            bake_anim=True,
            bake_anim_use_all_bones=True,
            bake_anim_use_nla_strips=False,
            bake_anim_use_all_actions=False,
            bake_anim_force_startend_keying=True,
            bake_anim_simplify_factor=0.0,
        )
        if "FINISHED" not in result:
            raise RuntimeError(f"FBX 내보내기 실패: {action.name}")

    print(f"[MONGME2] {Path(bpy.data.filepath).name}: {len(actions)}개 액션 분리 완료")


if __name__ == "__main__":
    main()
