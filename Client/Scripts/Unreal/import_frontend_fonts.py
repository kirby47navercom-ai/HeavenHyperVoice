"""프론트엔드 UI 글꼴(TTF)을 Font Face 로 임포트하고 글꼴 묶음(Font) 에셋을 만든다.

원본은 Client/UI/Fonts/<묶음>/ 에 있고, 셋 다 OFL 이라 같은 폴더의 OFL.txt 를 함께 둔다.
결과:
    /Game/UI/Fonts/<묶음>/FF_<파일 이름>   굵기별 Font Face
    /Game/UI/Fonts/<묶음>/F_<묶음>          UMG 글꼴 칸에 고르는 Font (Typeface = 굵기 이름)

실행: 에디터 안에서 Tools > Execute Python Script 로 이 파일을 연다.
UnrealEditor-Cmd 커맨드렛에서는 Slate 가 없어 Font Face 임포트가 어서션으로 멈춘다.
"""

from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve().parent
SOURCE_ROOT = PROJECT_ROOT / "Client" / "UI" / "Fonts"
DESTINATION_ROOT = "/Game/UI/Fonts"

# 묶음 이름: [(Typeface 이름, TTF 파일)]. 첫 항목이 기본 Typeface 다.
FAMILIES = {
    "DoHyeon": [("Regular", "DoHyeon-Regular.ttf")],
    "IBMPlexSansKR": [
        ("Regular", "IBMPlexSansKR-Regular.ttf"),
        ("Medium", "IBMPlexSansKR-Medium.ttf"),
        ("SemiBold", "IBMPlexSansKR-SemiBold.ttf"),
        ("Bold", "IBMPlexSansKR-Bold.ttf"),
    ],
    "IBMPlexMono": [
        ("Regular", "IBMPlexMono-Regular.ttf"),
        ("Medium", "IBMPlexMono-Medium.ttf"),
        ("SemiBold", "IBMPlexMono-SemiBold.ttf"),
    ],
}


def import_font_face(source_path: Path, destination_path: str):
    destination_name = "FF_" + source_path.stem.replace("-", "_")
    factory = unreal.FontFileImportFactory()
    # 굵기별로 Font 가 따로 생기지 않게 한다. 묶음 Font 는 아래에서 한 번에 만든다.
    factory.set_editor_property("batch_create_font_asset", unreal.BatchCreateFontAsset.NO)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source_path))
    task.set_editor_property("destination_path", destination_path)
    task.set_editor_property("destination_name", destination_name)
    task.set_editor_property("factory", factory)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    asset_path = f"{destination_path}/{destination_name}"
    face = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not isinstance(face, unreal.FontFace):
        raise RuntimeError(f"Font Face 임포트 실패: {asset_path}")
    return face


def build_font(family: str, faces):
    destination_path = f"{DESTINATION_ROOT}/{family}"
    font_name = f"F_{family}"
    font_path = f"{destination_path}/{font_name}"
    font = unreal.EditorAssetLibrary.load_asset(font_path)
    if font is None:
        font = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            font_name, destination_path, unreal.Font, unreal.FontFactory())
    if not isinstance(font, unreal.Font):
        raise RuntimeError(f"Font 생성 실패: {font_path}")

    # 파이썬에 Typeface 구조체가 노출되지 않아 텍스트로 채운다.
    entries = ",".join(
        f'(Name="{typeface}",Font=(FontFaceAsset="/Script/Engine.FontFace\'{face.get_path_name()}\'"))'
        for typeface, face in faces
    )
    composite = font.get_editor_property("composite_font")
    composite.import_text(f"(DefaultTypeface=(Fonts=({entries})))")
    font.set_editor_property("font_cache_type", unreal.FontCacheType.RUNTIME)
    font.set_editor_property("composite_font", composite)

    exported = font.get_editor_property("composite_font").export_text()
    missing = [face.get_name() for _, face in faces if face.get_name() not in exported]
    if missing:
        raise RuntimeError(f"{font_path} 에 Font Face 가 안 들어감: {missing}")
    unreal.EditorAssetLibrary.save_loaded_asset(font, only_if_is_dirty=False)
    return font_path


def main():
    for family, files in FAMILIES.items():
        faces = []
        for typeface, file_name in files:
            source = SOURCE_ROOT / family / file_name
            if not source.is_file():
                raise FileNotFoundError(f"글꼴 파일을 찾지 못함: {source}")
            faces.append((typeface, import_font_face(source, f"{DESTINATION_ROOT}/{family}")))
        unreal.log(f"[FRONTEND FONT] {build_font(family, faces)} ({', '.join(t for t, _ in faces)})")


if __name__ == "__main__":
    main()
