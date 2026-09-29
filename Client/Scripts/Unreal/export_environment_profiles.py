"""환경 DA를 서버의 이름=값 설정으로 내보낸다. 서버는 uasset을 읽지 않는다.
에디터 Python에서 실행하면 기존 DA에 저장한 값을 그대로 내보낸다.
"""
from pathlib import Path
import re
import unreal

ROOT='/Game/VFX/Weather'


def export_profile(asset):
    # 공개된 double 필드만 헤더에서 찾는다. 에셋 값이 원본이며 숫자를 중복 관리하지 않는다.
    repo=Path(unreal.Paths.project_dir()).resolve().parent
    header=repo/'Client/Source/HeavenHyperVoice/Environment/UEEnvironmentProfile.h'
    fields=re.findall(r'UPROPERTY[^\n]+\bdouble (\w+)\s*=',header.read_text(encoding='utf8'))
    out=repo/'Server/InstanceServer/config/environment'/f'{asset.get_name()}.ini'
    out.parent.mkdir(parents=True,exist_ok=True)
    lines=['# Generated from '+asset.get_path_name(), '# Edit the data asset, then run export_environment_profiles.py.']
    for name in fields:
        prop=re.sub(r'(?<!^)(?=[A-Z])','_',name).lower()
        lines.append(name[0].lower()+name[1:]+'='+format(asset.get_editor_property(prop),'.12g'))
    out.write_text('\n'.join(lines)+'\n',encoding='utf8')
    return str(out)


if __name__=='__main__':
    for path in unreal.EditorAssetLibrary.list_assets(ROOT,recursive=True,include_folder=False):
        asset=unreal.EditorAssetLibrary.load_asset(path)
        if isinstance(asset,unreal.UEEnvironmentProfile): export_profile(asset)
