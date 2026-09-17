"""타이틀·서버 접속·로그인 화면 리소스 PNG를 언리얼 텍스처로 임포트한다.

원본은 Client/UI/Images/Frontend 에 있고, TitleLogo.png 를 뺀 나머지는
Client/Scripts/Art/Frontend/build_frontend_ui_textures.py 가 만든다.
배치는 하지 않는다. 9-slice 여백과 크기는 Client/UI/Images/Frontend/README.md 를 본다.

실행 (에디터를 닫은 상태):
    UnrealEditor-Cmd.exe HeavenHyperVoice.uproject -run=pythonscript
        -script="Client/Scripts/Unreal/import_frontend_ui_textures.py" -unattended -nosplash
에디터 안에서는 Tools > Execute Python Script 로 같은 파일을 실행해도 된다.
"""

from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve().parent
SOURCE_ROOT = PROJECT_ROOT / "Client" / "UI" / "Images" / "Frontend"
DESTINATION_PATH = "/Game/Frontend/UI/Textures"


def import_texture(source_path: Path):
    destination_name = f"T_Frontend_{source_path.stem}"
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source_path))
    task.set_editor_property("destination_path", DESTINATION_PATH)
    task.set_editor_property("destination_name", destination_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture_path = f"{DESTINATION_PATH}/{destination_name}"
    texture = unreal.EditorAssetLibrary.load_asset(texture_path)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"Texture2D 임포트 실패: {texture_path}")

    # 9-slice 테두리와 하늘 그라디언트가 압축 블록으로 뭉개지지 않게 원본 그대로 쓴다.
    # ponytail: 전부 비압축이라 합계 약 20MB. 메모리가 문제되면 로고·구름·섬만 BC7 로 바꾼다.
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("never_stream", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    return texture_path


def main():
    sources = sorted(SOURCE_ROOT.glob("*.png"))
    if not sources:
        raise FileNotFoundError(f"PNG 를 찾지 못함: {SOURCE_ROOT}")
    for source in sources:
        unreal.log(f"[FRONTEND UI] {import_texture(source)}")
    unreal.log(f"[FRONTEND UI] 텍스처 {len(sources)}개 임포트 완료")


if __name__ == "__main__":
    main()
