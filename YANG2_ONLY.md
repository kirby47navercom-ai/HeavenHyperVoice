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
