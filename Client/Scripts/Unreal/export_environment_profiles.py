"""환경 DA를 서버의 이름=값 설정으로 내보낸다. 서버는 uasset을 읽지 않는다.
에디터 Python에서 실행하면 기존 DA에 저장한 값을 그대로 내보낸다.
"""
from pathlib import Path
import math
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
        # 숫자/연속 대문자가 있는 단위 이름은 Python의 snake_case 변환과 달라질 수 있어요.
        # 헤더의 원래 reflection 이름으로 읽으면 Wm2K 같은 단위도 정확히 가져와요.
        lines.append(name[0].lower()+name[1:]+'='+format(asset.get_editor_property(name),'.12g'))
    # 같은 DA 안의 퍼지 구조체도 내보내요. PC 절대 주소를 게임 코드에 넣지 않아요.
    fuzzy_header=header.with_name('UEFuzzyWeatherProfile.h')
    fuzzy_fields=re.findall(r'UPROPERTY[^\n]+\bdouble (\w+)\s*=',fuzzy_header.read_text(encoding='utf8'))
    fuzzy=asset.get_editor_property('FuzzyWeather')
    values={name:float(fuzzy.get_editor_property(name)) for name in fuzzy_fields}
    bounds=[('HumidStartPct','HumidFullPct',0,100), ('FogHumidStartPct','FogHumidFullPct',0,100),
            ('FullSnowTemperatureC','FullRainTemperatureC',-60,60), ('FogCalmStartMps','FogCalmEndMps',0,200),
            ('SoilDryStart','SoilDryFull',0,1)]
    if not all(math.isfinite(v) for v in values.values()): raise ValueError('Fuzzy values must be finite')
    for start,full,low,high in bounds:
        if not low<=values[start]<values[full]<=high: raise ValueError('Invalid fuzzy bounds: '+start+'/'+full)
    for name,low,high in [('LowPressureFullDeficitHpa',.01,100),('CloudFullCover',.001,1),('FogCoolingFullC',.01,60)]:
        if not low<=values[name]<=high: raise ValueError('Invalid fuzzy value: '+name)
    for name,value in values.items(): lines.append('fuzzy.'+name[0].lower()+name[1:]+'='+format(value,'.12g'))
    for name in ('RainRuleOutputs','FogRuleOutputs','DustRuleOutputs'):
        rules=list(fuzzy.get_editor_property(name))
        if len(rules)!=8 or not all(math.isfinite(v) and 0<=v<=1 for v in rules):
            raise ValueError('Expected eight fuzzy rule outputs in [0,1]: '+name)
        lines.append('fuzzy.'+name[0].lower()+name[1:]+'='+','.join(format(v,'.12g') for v in rules))
    for rule in asset.get_editor_property('spawn_rules'):
        dex=rule.get_editor_property('pokemon_dex')
        weights=[rule.get_editor_property(p) for p in ('base_weight','rain_multiplier','snow_multiplier','night_multiplier')]
        lines.append('spawn.'+str(dex)+'='+','.join(format(v,'.12g') for v in weights))
    for i,region in enumerate(asset.get_editor_property('water_regions')):
        lo=region.get_editor_property('minimum');hi=region.get_editor_property('maximum')
        values=[lo.x,lo.y,hi.x,hi.y,region.get_editor_property('sea_level_cm'),region.get_editor_property('swim_depth_cm'),int(region.get_editor_property('can_swim'))]
        lines.append('water.'+str(i)+'='+','.join(format(v,'.12g') for v in values))
    out.write_text('\n'.join(lines)+'\n',encoding='utf8')
    return str(out)


if __name__=='__main__':
    for path in unreal.EditorAssetLibrary.list_assets(ROOT,recursive=True,include_folder=False):
        asset=unreal.EditorAssetLibrary.load_asset(path)
        if isinstance(asset,unreal.UEEnvironmentProfile): export_profile(asset)
