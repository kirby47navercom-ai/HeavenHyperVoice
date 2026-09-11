# Goldenrod R03 필드

- 첫 입장: `WBP_CharacterSelection.GameplayLevel` → `/Game/Environments/Goldenrod_R03/Maps/L_Goldenrod`.
- 인스턴스 복귀: `DefaultGame.ini`의 `FieldLevel`도 같은 맵을 사용한다.
- `BP_GoldenrodCity`는 `AUEGoldenrodCity`를 상속하고 `CityMesh`에 수면이 제거된 `SM_Goldenrod_City_Land_R03`을 연결한다. 원본 합본 `SM_Goldenrod_City_R03`은 보존한다. 메시 컴포넌트는 충돌하지 않는다.
- 바닥 45개와 건물·기둥·해안 경계 75개는 `Source/HeavenHyperVoice/World/GoldenrodCollision.inl`의 **C++ BoxComponent**다. 메시 에셋에 충돌을 굽지 않는다. 건물은 외곽 경계 상자로 막으며, 출입 가능한 실내는 아직 없다.
- 소품 충돌 274개는 `GoldenrodPropCollision.inl`에서 추가한다. 나무 줄기 108개, 전등 30개의 받침·기둥·등, 화분 10개, 벤치 6개의 좌판·등받이, 해안 말뚝 50개, 분수 2개의 그릇·중앙 기둥이다. R03 원본 메시의 연결된 부분별 실제 경계를 사용하며 나뭇잎은 막지 않는다. 소품 위로 자동 올라서기를 막고 모두 `ServerWall`로 내보낸다.
- 원본이 바뀌면 Blender에서 `Scripts/Blender/extract_goldenrod_prop_collision.py`로 소품 충돌을 다시 생성하고 C++을 빌드한다. 원본 blend는 읽기만 하며 SourceArt를 Git에 포함할 필요가 없다.
- 좌표는 R03 원본 지형 기준 UE cm다. 도시 액터의 위치/회전은 0, 스케일은 1로 유지한다. 바닥은 도로 Z=0, 보도 Z=12다.
- PlayerStart는 `(0, 0, 90)`에 두어 캡슐이 바닥 위에 놓인다. 기존 인스턴스 포탈은 `(0, -700, 0)`이며 기존 목적지 설정을 유지한다.

## 서버 충돌

네이티브 박스의 `ServerGround` / `ServerWall` 태그를 기존 `HHVMapExport`가 읽어 `Server/maps/Goldenrod.hhvmap`을 만든다. 서버 XY는 UE XY에 25600을 더한다. 별도의 로컬 테스트 서버가 필요하지 않다.

맵이나 C++ 충돌을 바꿨다면 에디터 모듈을 빌드하고 레벨을 저장한 뒤 다시 내보낸다. PowerShell에서 실행:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\git\HeavenHyperVoice\Client\HeavenHyperVoice.uproject' -run=HHVMapExport -Map=/Game/Environments/Goldenrod_R03/Maps/L_Goldenrod '-Out=C:\git\HeavenHyperVoice\Server\maps\Goldenrod.hhvmap' -OriginOffset=25600 -unattended -nop4 -nosplash
```

FieldServer는 기본으로 `maps/Goldenrod.hhvmap`을 읽는다. CMake 빌드는 maps 폴더를 실행 파일 옆으로 복사한다. 운영 서버에도 새 실행 파일과 맵을 함께 반영하고 재시작해야 한다. 기존 실행 인자에 `--map` 또는 Launcher의 `--field-map`이 있다면 그것이 기본값보다 우선한다.

기존 저장 위치가 새 맵에서 걸어 다닐 수 없는 곳이면 중앙 시작점으로 보정한다. 인스턴스 서버의 전투 맵은 별도로 유지한다.

`create_goldenrod_field.py`는 최초 제작용이다. 기존 레벨의 수동 편집을 보호하기 위해 레벨이 존재하면 중단한다. 이후 배치는 Unreal 레벨과 Blueprint에서 편집한다.

## 해안 연출과 Nanite

- 기존 바다와 확장 바다는 별도 `Goldenrod_Ocean` 액터의 평면 하나로 통합했다. `SM_Goldenrod_Ocean`의 머티리얼 슬롯은 하나이며, 기본 재질은 `MI_Goldenrod_Ocean`이다. 아웃라이너에서 `Goldenrod_Ocean`을 선택한 뒤 `StaticMeshComponent > Materials > Element 0`에 원하는 머티리얼을 넣으면 전체 바다만 바뀐다. 크기는 해당 액터의 X/Y Scale로 조절한다. `BP_GoldenrodCity`의 옛 OceanExtension은 제거했다.
- `separate_goldenrod_ocean.py`는 원본에서 높이 -175cm의 Water 수면만 제거한 도시 사본을 만들고, 기존 영역과 사방 100000cm 확장을 하나의 수면으로 덮는다. 도시 C++ 충돌 394개는 유지한다. 분리 전 맵/BP는 `Saved/Codex/SeparateOcean/Before`에 백업된다. 원본 합본을 재임포트한 경우 도시 사본도 다시 분리해서 갱신해야 한다.
- 독립 바다는 단순 평면이며 그림자와 충돌을 끈다. 반투명 물 머티리얼도 교체할 수 있도록 이 평면만 Nanite를 끈다. 도시 Nanite는 유지한다.

- 도시 합본 메시의 Nanite를 켰다. 바닥·건물·도시 소품을 포함한 합본 전체에 적용되며, 기존 C++ 충돌은 그대로 사용한다.
- `M_R02_Accent_Water`의 금속 패널 BaseColor/Normal 텍스처를 제거했다. 텍스처에 그려져 있던 사각 테두리가 물에 반복되던 문제였다. 색상은 `WaterColor`, 거칠기는 `WaterRoughness`로 조절하며 원본·확장 수면 모두 같은 월드 좌표 기반 잔물결과 월드 노멀을 사용한다. 수면별 UV 크기가 달라도 경계의 색상·법선이 이어진다. `fix_goldenrod_water_props.py`로 작성한다.
- 사용자 요청으로 안개를 제거했다. 도시 BP와 레벨 액터의 `BoundaryWalls` 메시·머티리얼 연결을 비우고 숨겼으며, LocalFogVolume과 ExponentialHeightFog 액터도 제거했다. 이동을 막는 C++ 충돌은 유지한다.
- `BP_GoldenrodCity`의 `Goldenrod | Boundary Camera`에서 전환 거리(기본 2600 cm), 최대 효과 거리(350 cm), 내려다보는 각도(-72도), 추가 카메라 거리(220 cm)를 조절한다.
- `UUEGoldenrodSpringArm`이 해안 충돌 상자와의 거리를 사용한다. 안쪽으로 돌아오면 원래 시점으로 부드럽게 복귀하며, 마우스 회전 입력·이동 방향·스프링암 벽 충돌은 유지한다.
- `remove_goldenrod_fog.py`가 현재 설정을 적용한다. `polish_goldenrod_field.py`도 안개 제거 상태를 유지한다. `build_goldenrod_fog_wall.py`는 사용 중단한 이전 제작 스크립트이므로 다시 실행하면 안개 벽을 되살린다.

## 포켓몬 Blend Space 제작

Python에서 `sample_data`를 설정하는 것만으로는 실행 시 쓰는 보간 데이터가 생성되지 않는다. `import_mongme2_pokemon.py`는 이제 `PokemonAnimationEditorLibrary.rebuild_blend_space()`를 호출하고 저장한다. 기존 에셋은 `repair_pokemon_blend_spaces.py`로 누락된 보간 데이터와 전투 대기 연결을 보정했다. 결과 기록은 로컬 `Saved/Codex/CityPolish/animation_repairs.json`에 있다.
