# HHV Toon 재질

UE 5.8 Substrate Toon BSDF를 사용한다. 사용자 원본 `/Game/Toon/M_ToonCharacter`,
`/Game/Toon/TP_Test` 및 엔진 플러그인의 FBX 원본 머티리얼은 유지한다.

## 편집 위치

- `/Game/Toon/HHV/Profiles/TP_HHV_*`: 대상별 그림자 색, 명암 단계, 반사 램프.
- `/Game/Toon/HHV/Masters/M_HHV_ToonCharacter`: 기존 캐릭터 커스터마이징 재질 기반.
- `/Game/Toon/HHV/Masters/M_HHV_ToonImported`: 기존 FBX 재질 기반. 텍스처, UV, 마스크 파라미터 유지.
- `/Game/Toon/HHV/Presets/MI_HHV_*`: 새 대상에 복제해서 사용할 예시 인스턴스.
- `/Game/Toon/HHV/Environment/MI_HHV_R02_*`: Goldenrod 메시에 연결된 인스턴스.
- `/Game/Toon/HHV/Masters/M_HHV_Outline`: 얇은 메시 외곽선. `OutlineWidthCM`, `OutlineColor` 편집 가능.

대상은 Face, Skin, Hair, Eyes, Cloth, Pokemon, Metal, Foliage, Environment,
Water, Emission의 11종이다. 얼굴은 부드러운 램프와 약한 노멀을 사용하며,
별도 얼굴 SDF 텍스처를 생성하거나 연결한 구현은 아니다.

실제 적용된 캐릭터/포켓몬 인스턴스에서 **HHV Toon** 파라미터를 수정한다.
예시 Presets를 바꾸어도 이미 적용된 인스턴스의 값이 일괄 변경되지는 않는다.
공통 명암을 바꾸려면 연결된 Toon Profile을 수정한다.

| 파라미터 | 의미 |
| --- | --- |
| ToonNormalStrength | 노멀 맵의 세기 |
| ToonSpecularScale | 기존 반사 세기의 배율 |
| ToonRoughness | Toon용 거칠기 |
| ToonRoughnessOverride | 기존 거칠기와 Toon 거칠기의 혼합 비율 |
| ToonAnisotropy | 머리카락 등에 쓰는 방향성 반사 |
| ToonRimStrength / ToonRimExponent | 가장자리 밝기와 폭 |
| ToonEmissionStrength | 기존 발광에 더하는 기본색 발광 |

외곽선은 Skeletal Mesh의 Overlay Material에 연결한다. 얼굴/눈 전용 캐릭터
메시 및 불꽃 슬롯을 가진 포켓몬은 제외한다. 800~2200cm 거리에서 두께가
줄어든다. 메시의 Overlay Material을 비우면 해당 대상의 외곽선만 해제된다.

## 적용 범위와 보존

- 캐릭터의 기존 수작업/FBX 재질 인스턴스를 새 마스터에 연결한다.
  MorphSafe 하위 인스턴스는 상속한다. 보관용 glTF Substrate 그래프는 유지한다.
- SpeciesData에 연결된 포켓몬의 몸/눈/불꽃을 구분한다.
  자망칼과 디아루가의 몸은 Metal을 사용한다.
- Goldenrod는 기존 44개 재질 그래프에 Toon BSDF를 연결한다.
  물은 기존 월드 좌표 기반의 연속적인 표면을 유지한다.
- 이후 바다를 `Goldenrod_Ocean` 독립 액터/단일 슬롯으로 분리했다.
  현재 도시 메시는 `SM_Goldenrod_City_Land_R03`, 바다 기본 재질은
  `/Game/Environments/Goldenrod_R03/Materials/MI_Goldenrod_Ocean`이다.
  바다 전체 교체는 액터의 Element 0에서 한다. Toon 스크립트 재적용은
  독립 바다의 사용자 머티리얼을 덮어쓰지 않는다.
- 텍스처/메시 자체, UI, 서버 접속 설정 및 서버 충돌 데이터는 이 작업에서 바꾸지 않는다.

## 제작 스크립트와 복구

`build_hhv_toon.py`는 기본 실행 시 새 프로필/마스터/예시만 만든다.
Python의 `sys.argv`에 `--apply`를 넣어 실행하면 기존 에셋에도 적용한다.
재적용은 인스턴스의 Toon 값을 스크립트 기본값으로 덮어쓰므로, 에디터에서
직접 조정한 뒤에는 단순 갱신 목적으로 재실행하지 않는다.
중단된 적용을 재개할 때는 `--apply`, `--resume`을 함께 사용한다.
대량 저장에는 `UnrealEditor-Cmd.exe -nullrhi -ExecutePythonScript=...`를 사용해
캐릭터 썸네일 렌더링이 GPU 메모리를 누적 점유하지 않도록 한다.

수정 전 기존 에셋 사본은 `Client/Saved/Codex/ToonBuild/Before`에 저장된다.
이 사본은 Git에 포함되지 않는다. 복구할 때는 에디터를 닫고 필요한 파일을
동일한 상대 경로의 `Client/Content`로 복사한다. 생성된 Toon 폴더를 먼저
삭제하면 기존 에셋 참조가 끊어지므로 원본 복구가 먼저다.

결과 목록과 정적 확인 기록은 `Client/Saved/Codex/ToonBuild`에 남긴다.
플레이 테스트는 실행하지 않는다.

2026-09-11 적용: 캐릭터 재질 863개, 포켓몬 73종/재질 슬롯 307개,
Goldenrod 재질 44개, 외곽선 메시 236개. 저장 후 재로드하여 프로필 참조,
도시 슬롯 연결, 바다의 월드 노멀 설정 및 머티리얼 셰이더 통계를 확인했다.
