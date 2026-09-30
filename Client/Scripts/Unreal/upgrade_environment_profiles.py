"""기존 환경 DA에 새 필드를 저장하고 강수 이미터를 지속형으로 바꿔요. 플레이를 실행하지 않아요."""
import sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
import create_instance_weather as weather
from export_environment_profiles import export_profile

def main():
    weather.main()
    # 예시 가중치는 에셋에만 저장해요. 서버의 실제 종족 목록 밖에 있는 종족을 추가 생성하지 않아요.
    samples={'Temperate':[(393,1.5,1,1),(712,1,1.5,1),(401,1,1,1.5)],
             'Coast':[(393,1.5,1,1),(7,1.3,1,1)],'Desert':[(624,1,1,1.4)]}
    for kind,rows in samples.items():
        asset=unreal.EditorAssetLibrary.load_asset(weather.ROOT+'/DA_Environment_'+kind)
        if not asset: raise RuntimeError('Missing environment profile: '+kind)
        if not asset.get_editor_property('spawn_rules'):
            rules=[]
            for dex,rain,snow,night in rows:
                r=unreal.UEEnvironmentSpawnRule()
                for name,value in dict(pokemon_dex=dex,base_weight=1,rain_multiplier=rain,
                                       snow_multiplier=snow,night_multiplier=night).items(): r.set_editor_property(name,value)
                rules.append(r)
            asset.set_editor_property('spawn_rules',rules)
        weather.save(asset)
        export_profile(asset)
    for kind in ('Rain','Snow'):
        system=unreal.EditorAssetLibrary.load_asset(weather.ROOT+'/NS_Weather_'+kind)
        if not weather.water.EDIT.finish_water_system(system): raise RuntimeError('Niagara not ready: '+kind)
        emitter=weather.water.EDIT.water_layer(system,kind,False)
        # SpawnInfos는 Python에서 보호되어 있어 기존 reflection 제작 함수로 구조/바인딩을 확인해요.
        if not weather.water.EDIT.bind_weather_spawn_rate(system,emitter):
            raise RuntimeError('Continuous weather binding is missing: '+kind)
        if not weather.water.EDIT.finish_water_system(system): raise RuntimeError('Niagara binding compile failed: '+kind)
        weather.save(system)
    unreal.log('ENVIRONMENT UPGRADE READY: profiles exported, continuous rain/snow saved')

if __name__=='__main__': main()
