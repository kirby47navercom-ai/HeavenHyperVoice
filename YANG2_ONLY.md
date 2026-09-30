# Yang2 전용 클라이언트 권위 경계

`Yang2`는 서버를 띄우지 않고 혼자 기능과 연출을 확인하기 위한 별도 브랜치다.
이 브랜치에서만 로그인부터 필드·인스턴스 플레이까지 클라이언트가 직접 진행한다.

## 절대 규칙

- 허용: `main`의 변경을 `Yang2`로 가져오기
- 금지: `Yang2`를 `main`으로 병합하기
- 금지: `YANG2_CLIENT_AUTHORITY_ONLY`가 들어간 파일이나 커밋을 `main`으로 체리픽하기
- 금지: `HHV_YANG2_CLIENT_AUTHORITY_ONLY=1`을 `main` 빌드에 넣기

`main`은 서버 권위를 유지해야 한다. 클라이언트는 서버가 보낸 이동·날씨 결과만 표현한다.
`Yang2`는 혼자 실행하는 시험 브랜치라서 아래 예외를 갖는다.

1. 로그인·가입·닉네임 확인·캐릭터 생성/선택을 로컬 SaveGame으로 처리한다.
2. 필드/인스턴스 서버 연결을 시작하지 않는다.
3. `UUECoreMovementComponent`의 로컬 시뮬레이션을 강제로 허용한다.
4. 로컬 이동 상태에서도 포탈을 활성화해 필드와 인스턴스를 오갈 수 있게 한다.
5. 선택한 스타팅 포켓몬으로 로컬 파티와 파트너를 만들고, 파티 변경을 로컬 저장한다.
6. 데이터 에셋의 확률로 가챠를 실행하고 토큰·해금 상태를 로컬에서 관리한다.
7. 인스턴스에서는 서버의 `InstanceWeather` 구현을 클라이언트 프로세스 안에서 실행한다.
8. 날씨 결과는 서버 패킷을 받았을 때와 같은 `OnInstanceWeatherChanged` 이벤트로 전달한다.
9. 채팅은 다른 사용자에게 보내지 않고 자기 화면에만 에코해 UI 입력을 시험한다.

`UEFieldServerBridgeComponent`의 `Yang2WeatherProfiles`는 인스턴스 타입별 로컬 기후다.
값을 넣지 않은 타입은 서버 `InstanceWeatherProfile` 기본값을 사용한다. 값을 넣으면
서버의 `--instance-weather`와 같은 기온, 습도, 기압, 지표수, 토양수와 시간배율로
클라이언트 프로세스 안에서 계산한다. 이 설정도 Yang2 전용이며 main에 보내지 않는다.

멀티플레이어 동기화, 실제 계정 DB, 다른 사용자와의 채팅·파티는 Yang2에서 만들지 않는다.
그 기능들은 서버 권위 검증 대상이며 `main`의 서버 실행 환경에서 확인한다.

## Yang2 전용 파일과 수정 지점

- `YANG2_ONLY.md`
- `.githooks/pre-commit`
- `.githooks/pre-push`
- `Client/Source/HeavenHyperVoice/Yang2/`
- `Client/Source/HeavenHyperVoice/HeavenHyperVoice.Build.cs`의
  `HHV_YANG2_CLIENT_AUTHORITY_ONLY=1`
- `UEFieldServerBridgeComponent.h/.cpp`의 `YANG2_CLIENT_AUTHORITY_ONLY` 구간
- `UEGameInstance.h/.cpp`, `UEInstancePortal.cpp`, `UEPlayerController.h/.cpp`,
  `UEFieldPartnerSyncComponent.h/.cpp`의 `YANG2_CLIENT_AUTHORITY_ONLY` 구간

## main을 Yang2로 가져오는 방법

```powershell
git switch Yang2
git fetch origin
git merge origin/main
```

병합 뒤에도 이 문서와 `Client/Source/HeavenHyperVoice/Yang2/`가 남아 있어야 한다.
`HeavenHyperVoice.Build.cs`의 `HHV_YANG2_CLIENT_AUTHORITY_ONLY=1`도 확인한다.

## 반대 방향은 금지

다음 명령은 실행하지 않는다.

```powershell
git switch main
git merge Yang2
```

Yang2에서 만든 정상적인 공용 기능을 main에 옮겨야 한다면 클라이언트 권위 변경과 분리된
커밋만 선별하거나, main에서 서버 권위 구조에 맞춰 다시 구현한다.

## Git 차단 장치

이 저장소는 `core.hooksPath=.githooks`를 사용한다.

- `pre-commit`: main에서 `YANG2_ONLY.md` 또는 Yang2 전용 표식이 보이면 커밋을 거절한다.
- `pre-merge-commit`: Yang2 전용 표식이 main에 들어간 병합 커밋을 거절한다.
- `pre-push`: main으로 보낼 커밋 안에 `YANG2_ONLY.md` 또는
  `YANG2_CLIENT_AUTHORITY_ONLY` 표식이 있으면 푸시를 거절한다.

훅을 일부러 건너뛰는 `--no-verify`는 이 브랜치 경계에는 사용하지 않는다.

## 비·눈 공간 연출 연결

`BP_InstanceWeatherDirector`는 인스턴스 진입 시 공통 데이터 에셋에서 자동 생성된다.
Yang2에서는 기존 `Yang2LocalWeather` 결과를 브릿지 이벤트로 받아 비·눈 낙하, 피격,
노출 표면의 젖음·적설을 표시한다. 서버 연결이나 실제 계정이 없어도 이 경로는 작동한다.
main에서는 같은 연출이 실제 `S_InstanceWeather` 패킷만 받아서 작동한다.

공용 BP/나이아가라/Environment 코드는 main에서 가져와도 된다. 이 문서와 Yang2의
브릿지·계산기 연결·빌드 매크로는 main으로 보내지 않는다.
설명과 아트 조절 항목은 `Client/Source/HeavenHyperVoice/Environment/INSTANCE_WEATHER_VISUALS.md` 참고.

`Tests/Yang2WeatherTests.cpp`의 `Heaven.Weather.Yang2LocalSource`도 Yang2 전용이다.
서버 없이 계산한 눈이 공통 연출에 전달되는지 검사하며, main에는 이 파일을 보내지 않는다.

공용 지면 함수, 웅덩이 마스크, 벽 물보라, 젖은 자국, 눈 덩어리도 같은 로컬 날씨 이벤트로
작동한다. main은 계속 서버 패킷만 입력받는다. 이 확장에는 새로운 권위 우회 코드를 추가하지 않았다.
실내가 생기면 제외 BP를 배치한다. 현재 맵에는 없는 실내나 수면을 임의로 만들지 않는다.

## 시간·해안·사막 로컬 실행

환경 확장 공통 코드는 main에서 가져왔다. 새 예제 수면은 `L_Environment_Coast`에만 있으며
기존 도시 물 메시는 교체하지 않는다. `BP_EnvironmentScene.Profile`에 연결한 DA를
`UEYang2Environment.cpp`가 읽어 로컬 계산기에 넣는다. 종류별 `Yang2WeatherProfiles`에
`EnvironmentProfile`을 지정하면 레벨 참조보다 우선한다. 양쪽 모두 비었으면 기존 여섯 설정을 사용한다.

`UEYang2InstanceWeatherImplementation.cpp`는 서버의 시간/대기/물순환/조석/사막 cpp를
그대로 포함한다. 결과는 `MakeYang2EnvironmentState()`를 거쳐 공통 연출에 들어간다.
서버용 ini 파서는 클라이언트에 포함하지 않는다. 두 Yang2 어댑터 파일과 브릿지 연결 변경은
main으로 보내면 안 된다. 기존 훅과 컴파일 매크로 차단을 유지한다.

전체 기능/파일 설명: `Server/InstanceServer/EARTH_ENVIRONMENT_GUIDE.md`.
추가 검사는 실제 DA 세 개의 로컬 입력 변환, 시간 전달, 사막 먼지 발생을 확인한다.


## 공용 시간·열/수분·게임 규칙 보완

main의 환경 변경을 병합한 뒤 Yang2 전용 어댑터에 새 14개 설정과 7개 결과값, SpawnRules 배열을 연결했다. 지표 에너지/외부 수분 교환/게임 배율 계산 CPP도 기존 단일 구현 TU에 포함한다. main에는 이 로컬 TU와 어댑터를 보내지 않는다.

UEYang2WorldClock.h/.cpp는 GameInstance별 steady_clock 시계를 유지한다. 필드부터 시작하고 레벨 전환 중에도 흐르며, 첫 인스턴스의 시간 배속·하루·연중 주기·시작 시각을 이후 인스턴스에 유지한다. 새 플레이 세션은 Saved/Yang2/environment.state에서 마지막 시계와 기후를 복구한다. 저장 파일이 없을 때만 새 시계를 시작한다. 지역 기후/위도는 각 DA를 사용한다.

로컬 날씨 결과는 기존 브릿지 이벤트를 거쳐 같은 BP 연출로 간다. 연결 초기화도 공통 ClearInstanceWeatherState를 사용해 오래된 낮밤/수면이 남지 않게 한다.

**적용 범위:** Yang2는 기후와 플레이어의 젖음·얼음·수영 이동을 로컬에서 계산한다. 야생 생성·리스폰은 현재 날씨의 종족 가중치를 사용한다. 서버의 WildBt/WildAi/FSM/행동 코드와 Recast/Detour를 재사용하여 배회·탐지·추적·공격 판단을 서버 없이 실행한다. 실제 전투 대미지·포획·다른 플레이어 동기화는 아직 연결하지 않는다.

공통 기능/에셋과 수치 의미는 Server/InstanceServer/EARTH_ENVIRONMENT_UPDATES.md에 설명했다.


## 재실행 복구·수영·야생 AI (이번 추가)

main의 공통 계산과 프로토콜 변경은 main에서 병합한다. 아래 로컬 연결만 Yang2에 커밋한다.

- `UEYang2WorldClock.cpp`: 30초마다 공용 시계와 방별 기후를 저장하고 종료/레벨 전환 시 즉시 저장한다. 서버와 같은 체크섬·임시 파일 교체·정상 이전 파일 백업을 사용한다. 저장 파일이나 DA 설정이 호환되지 않으면 오류를 표시하고 원본 파일을 덮어쓰지 않는다. 프로그램이 꺼진 시간 동안 기후를 임의로 진행하지 않는다.
- `UEYang2Environment.cpp`: DA의 IceTractionMultiplier, SwimSpeedCmPerSecond, WildRespawnSeconds, WaterRegions를 공통 모델에 전달한다. 도시처럼 물이 없는 곳은 WaterRegions를 비워 둔다. 해안 예제 DA에는 실제 해수면/해변/해저 구역을 넣었다.
- 브릿지 `PublishYang2ClientWeather()`: 로컬 기후의 이동 환경을 공통 MovementCore에 전달한다. main에서는 이 값이 서버 CoreState 패킷에서만 온다.
- `UEYang2WildSimulation.h/.cpp`: 소켓/UObject 없이 서버 Lua AI와 공통 충돌/이동을 실행한다. 야생 개체의 HP가 0이 되면 제거하고 지정한 대기 시간 후 현재 날씨로 다시 선택한다.
- `UEYang2WildBridge.cpp`: 플레이어가 쓰는 동일한 충돌 파일/해시로 길찾기를 백그라운드에서 준비한다. 준비되면 기존 WildPokemonSyncComponent로 모델/애니메이션/공격 신호를 표시한다. Lua 파일 오류나 해시 불일치가 있으면 실패 로그를 남긴다.
- `UEYang2ServerWildAI/ServerWildBT/ServerNavigation.cpp`: 실제 서버 소스를 포함하여 한 번 컴파일한다. main에서 AI를 고치면 Yang2도 동일한 소스가 갱신된다.

브릿지 BP의 `Yang2|Wild AI`에서 아래 값을 편집한다.

| 속성 | 역할 |
|---|---|
| Yang2WildAiScript | 모듈 실행 폴더를 기준으로 한 Lua 파일. 기본 Config는 Yang2AI/wild_ai.lua이며 빌드 시 실제 서버 스크립트가 복사된다. |
| Yang2WildPerRoom | 로컬 야생 슬롯 수. 기본 12, 범위 0~64. |
| Yang2WildAreaCm | 진입 플레이어 위치 주변 생성/배회 반경. 고정 좌표를 사용하지 않는다. |
| Yang2WildSpeciesDex | 생성 후보 도감 번호. 비우면 프로토콜의 일반 야생 종족 전체를 사용한다. |
| Yang2WeatherProfiles / EnvironmentProfile | 각 인스턴스의 기후·생성 가중치·물 구역 DA. 레벨의 BP_EnvironmentScene.Profile도 사용할 수 있다. |

`SetYang2WildHealth(EntityId, HP)`는 BP에서 로컬 사망/리스폰을 확인할 입력이며 전투 대미지 공식을 대신하지 않는다. `GetYang2WildCount()`는 현재 살아 있는 로컬 야생 수를 돌려준다. 이동·야생 클래스/모델은 기존 공통 데이터 에셋의 참조를 사용한다.

서버와 같은 Lua/네이티브 라이브러리를 사용하므로 빌드 전 기존 `Server/build/vs2022/vcpkg_installed/x64-windows` 의존성이 있어야 한다. 새 패키지는 추가하지 않았다. Lua DLL/스크립트는 빌드/패키징 결과 폴더로 함께 복사한다. 서버 OpenSSL 헤더가 UE OpenSSL 버전을 가리지 않도록 AI 헤더만 Intermediate에 준비한다. 큰 맵의 최초 NavMesh 생성이 끝나기 전에는 야생이 표시되지 않으며 준비 로그가 나온다.

검증: Editor 빌드, 공통 환경 검사, `Heaven.Weather.Yang2LocalSource`, `Heaven.Weather.Yang2WildAI`, 로컬 저장 복구 검사. 자동 검사는 작은 충돌 평면에서 실제 Lua의 추적·공격·사망·날씨별 리스폰을 확인한다. 실제 게임 접속/플레이 및 멀티플레이 부하 검증은 별도다.
