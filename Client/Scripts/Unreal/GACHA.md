# 포켓몬 캡슐 뽑기

전용 레벨: `/Game/Gacha/Maps/L_GachaStudio`.
기존 플레이 레벨과 서버 설정을 바꾸지 않는 독립된 뽑기방이다.

## 조작

1. 왼쪽에서 불꽃·물·풀·노말·전기 중 하나를 고른다.
2. 카메라 이동이 끝나면 기계 정면의 손잡이 끝을 마우스 왼쪽 버튼으로 잡는다.
3. 손잡이 중심을 기준으로 시계 방향으로 원을 그린다. 한 바퀴는 360도이며 세 바퀴가 필요하다.
4. 중간에 놓으면 진행도는 유지된다. 다시 손잡이를 잡아 이어서 돌릴 수 있다.
5. 세 번째 회전이 끝나면 캡슐이 배출되고 포켓몬 이름과 희귀도가 표시된다. 결과 화면의 ‘한 번 더 뽑기’로 초기화한다.

손잡이 중심에 너무 가까이 붙거나 기계에서 멀리 드래그하면 회전을 중단한다.
ESC는 손잡이를 놓는다. 반대 방향 움직임은 현재 바퀴를 되감으며, 좌우로 흔들기만 해서는 횟수가 늘지 않는다.
추첨은 첫 유효한 회전에서 한 번만 한다. 진행 중 타입 변경과 재추첨은 막는다.

## 데이터 에셋

`/Game/Gacha/Data/DA_Gacha_Fire`, `DA_Gacha_Water`, `DA_Gacha_Grass`, `DA_Gacha_Normal`, `DA_Gacha_Electric`.

각 Entries 항목에서 Species(기존 포켓몬 데이터), Dex Number, Display Name, Rarity, Weight를 편집한다.
Weight는 전체 유효 항목 중 상대 가중치다. **실제 확률 = 해당 Weight / 전체 양수 Weight 합계**.
0인 항목은 나오지 않으며, 음수·NaN·무한대 및 유효하지 않은 도감번호/등급은 제외한다.
우측 목록은 같은 계산으로 실제 확률을 표시한다.
Normal Label / Rare Label / Super Rare Label에서 세 등급의 표시 이름을 바꾼다.

| 타입 | 일반 · 몬스터볼 (각 35%) | 레어 · 슈퍼볼 (각 12.5%) | 슈퍼레어 · 하이퍼볼 (5%) |
|---|---|---|---|
| 불꽃 | 불꽃숭이, 영치코 | 폭타, 파이어로 | 윈디 |
| 물 | 팽도리, 개굴반장 | 블로스터, 누오 | 갸라도스 |
| 풀 | 모부기, 나무돌이 | 버섯모, 눈설왕 | 드레디어 |
| 노말 | 나옹, 노고치 | 게을킹, 잠만보 | 붉은달다투곰 |
| 전기 | 꼬링크, 데덴네 | 파치리스, 전룡 | 로토무 6폼 합계 5% |

사용자 선택에 따라 노말 슈퍼레어의 도감번호 901 항목은 기존 `DA_붉은달다투곰`에 연결했다. 원래 모델과 애니메이션을 재사용하며 일반 폼 데이터는 추가하지 않는다.

로토무 6폼을 포함한 30개 후보 모두 종족 데이터의 `ProfileIcon`을 사용한다. 기존 5종은 재사용하고, 빠진 20종과 로토무 추가 5폼은 같은 512×512 원형 초상화 방식으로 추가했다. 아이콘은 `/Game/UI/PokemonParty/Portraits/<종족>/T_<종족>_Portrait`에서, 연결은 `DA_<종족>`의 `Profile Icon`에서 수정한다.

`connect_gacha_pokemon_portraits.py`는 기존 `import_round_pokemon_portraits.py`를 재사용한다. 이미 지정된 아이콘은 보존하며 비어 있는 종만 임포트한다. PNG를 수정한 뒤 다시 임포트하려면 기존 임포터의 `import_species_portraits(["종족명"])`를 사용한다. 원본 출처는 `SourceArt/Pokemon/Portraits/GACHA_SOURCES.md`에 기록했다.

위 확률은 초기값이다. 등급 합계는 70% / 25% / 5%이고 자유롭게 변경할 수 있다.

| 캡슐 | 첫 바퀴 | 두 번째 | 세 번째 |
|---|---|---|---|
| 몬스터볼 | 파랑 | 파랑 | 파랑 |
| 슈퍼볼 | 파랑 | 보라 | 보라 |
| 하이퍼볼 | 파랑 | 보라 | 금색 |

## C++ 부모와 편집 가능한 화면

- `AUEGachaMachine` → `BP_GachaMachine`: 손잡이 각도, 3단계 진행, 구 안의 공 충돌/흔들림, 배출 동작. 부품과 30개 캡슐은 BP의 Components에 저장된다. 메시를 교체해도 태그 `GachaHandle`, `GachaCapsule`, `StageGlow`와 HandlePivot을 유지한다.
- `UUEGachaPool` → 타입별 DataAsset: 후보, 가중치, 등급 이름.
- `UUEGachaStudioWidget` → `WBP_GachaStudio`: 배치·글자·패널·버튼은 UMG Designer에서 편집한다. BindWidget 이름을 유지한다.
- `AUEGachaStudioController` → `BP_GachaStudioController`: 손잡이 마우스 입력과 카메라 선택.
- `AUEGachaStudioGameMode` → `BP_GachaStudioGameMode`: 이 레벨 전용 모드. 기존 로그인/플레이 컨트롤러와 분리한다.

## 로토무 폼

기본 로토무와 히트·워시·프로스트·스핀·커트로토무가 전기 뽑기에 각각 등록되어 있다.
희귀도는 모두 슈퍼레어이며, 기존 로토무의 가중치 5를 6개로 나눠 각 `0.833333`으로 시작한다.
다른 후보가 초기값일 때 폼별 약 0.8333%, 로토무 합계 5%다. 각 Entries의 Weight를 독립적으로 편집할 수 있다.

각 폼의 DA·ABP·BS는 `/Game/Pokemon/SpeciesData/<폼 이름>/`에 있다.
기본 로토무는 기존 `로토무` 폴더를 사용한다. 새 폼은 자체 스켈레톤의 애니메이션과 이동 블렌드스페이스를 사용하며, 기본 폼의 이동/능력치/울음 설정을 기준으로 시작한다.
전국도감 번호는 모두 479이고 `SpeciesId`는 Rotom, RotomHeat, RotomWash, RotomFrost, RotomFan, RotomMow로 구분한다.

`connect_rotom_forms.py`로 누락된 폼 에셋을 만들고 전기 후보에 연결한다. 이미 존재하는 폼 데이터는 재사용하며, 6폼이 모두 등록된 뒤에는 재실행해도 수동 설정한 가중치를 보존한다.
처음 에셋을 제작할 때는 `HHVGachaBuild` 이후 이 연결 스크립트를 실행한다.
후보가 늘어나도 목록을 볼 수 있도록 `WBP_GachaStudio`의 PoolText를 Designer의 PoolScroll 안에 배치했다.

## 보상 연결

현재 범위는 독립된 클라이언트 뽑기 연출과 결과 표시다. 계정 포켓몬 보유 목록을 변경하거나 재화를 소비하지 않는다.
`OnRewardRevealed`에 결과 구조체가 한 번 전달되므로 이후 전용 서버의 확정 결과/지급 흐름을 연결할 수 있다.
실서비스 확률 판정과 지급 권한은 서버에서 처리해야 한다.

## 에셋 제작 도구

`HHVGachaBuild`는 편집기 모듈의 일회성 에셋 제작 Commandlet이다. 실행 시 BP/WBP/DataAsset/메시/재질/레벨을 저장한다.
기존 `BP_GachaMachine`이 있으면 사용자의 편집을 보호하기 위해 중단한다. 재실행 대신 만들어진 에셋을 편집한다.
게임 런타임에는 이 제작 도구나 Python/MCP가 필요하지 않다.
사용자 요청에 따라 플레이 테스트와 자동화 테스트는 실행하지 않는다.
