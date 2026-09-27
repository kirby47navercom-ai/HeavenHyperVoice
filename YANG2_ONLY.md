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
