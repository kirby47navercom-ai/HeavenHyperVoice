# HeavenHyperVoice 클라이언트 구조 문서

## 1. 문서 목적

이 문서는 `HeavenHyperVoice` 언리얼 클라이언트의 현재 구조와 실행 흐름을 설명한다.

팀원이 다음 내용을 빠르게 파악할 수 있도록 작성되었다.

- C++ 클래스와 언리얼 에셋의 연결 관계
- 로그인 및 회원가입 흐름
- 캐릭터 이동과 Enhanced Input 구조
- 애니메이션 C++ 데이터 구조
- Gameplay Tag의 현재 사용 범위
- 블루프린트에서 수정할 수 있는 캐릭터 수치
- 현재 완료된 부분과 추가 작업이 필요한 부분

서버 코드는 이 구조에 포함되지 않는다. 현재 로그인과 회원가입은 클라이언트 로컬 기능이다.

---

## 2. 핵심 설계 원칙

### 2.1 C++은 공통 뼈대를 담당한다

캐릭터 동작, 입력 처리, 로그인 검증처럼 여러 블루프린트가 공통으로 사용할 기능은 C++ 부모 클래스에 작성한다.

### 2.2 블루프린트는 실제 게임 에셋과 조정값을 담당한다

`BP_PlayerCharacter`는 `AUEPlayerCharacter`를 부모로 사용한다. 이후 캐릭터 종류가 늘어나면 `BP_PlayerCharacter`를 다시 부모로 삼아 자식 블루프린트를 만들 수 있다.

예시:

```text
AUEPlayerCharacter (C++)
└─ BP_PlayerCharacter
   ├─ BP_Player_Knight
   ├─ BP_Player_Mage
   └─ BP_Player_Rogue
```

### 2.3 역할에 따라 폴더를 분리한다

캐릭터, 애니메이션, 입력 데이터, UI, 로그인 저장 기능을 하나의 폴더에 섞지 않는다.

### 2.4 X축을 캐릭터의 앞 방향으로 사용한다

이 프로젝트에서 캐릭터의 앞은 반드시 `+X`다.

- `W`: `+X`
- `S`: `-X`
- `D`: `+Y`
- `A`: `-Y`

캐릭터 메시를 추가할 때도 메시의 정면이 언리얼 기준 `+X`를 바라보도록 맞춰야 한다.

---

## 3. 전체 폴더 구조

```text
Client/
├─ Source/HeavenHyperVoice/
│  ├─ AbilitySystem/
│  │  ├─ UEAbilitySystemComponent.h / .cpp
│  │  ├─ UEGameplayAbility.h / .cpp
│  │  └─ AttributeSet/
│  │     └─ UEAttributeSet.h / .cpp
│  ├─ AI/
│  │  └─ UEAIController.h / .cpp
│  ├─ Animation/
│  │  └─ UEAnimInstance.h / .cpp
│  ├─ Character/
│  │  └─ UEPlayerCharacter.h / .cpp
│  ├─ Data/
│  │  ├─ UEDataAsset.h / .cpp
│  │  └─ UEPrimaryDataAsset.h / .cpp
│  ├─ GameMode/
│  │  └─ UEGameModeBase.h / .cpp
│  ├─ Player/
│  │  └─ UEPlayerController.h / .cpp
│  ├─ System/
│  │  ├─ UEAssetManager.h / .cpp
│  │  ├─ UEGameInstance.h / .cpp
│  │  └─ Account/
│  │     ├─ UEAccountSaveGame.h / .cpp
│  │     └─ UEAccountSubsystem.h / .cpp
│  ├─ UI/
│  │  └─ Login/
│  │     └─ UELoginWidget.h / .cpp
│  ├─ UEGameplayTags.h / .cpp
│  └─ HeavenHyperVoice.Build.cs
│
└─ Content/
   ├─ Blueprints/Login/
   │  ├─ BP_LoginGameMode
   │  └─ BP_LoginPlayerController
   ├─ Character/Player/
   │  └─ BP_PlayerCharacter
   ├─ Data/Input/
   │  └─ DA_PlayerInput
   ├─ Input/Actions/
   │  ├─ IA_MoveForward
   │  ├─ IA_MoveBackward
   │  ├─ IA_MoveRight
   │  ├─ IA_MoveLeft
   │  ├─ IA_LookYaw
   │  ├─ IA_LookPitch
   │  ├─ IA_Run
   │  ├─ IA_Roll
   │  └─ IA_Jump
   ├─ Input/Mapping/
   │  └─ IMC_Player
   └─ UI/Login/
      └─ WBP_Login
```

`AbilitySystem`과 `AI` 폴더는 현재 프로젝트에 존재하지만, 이번 플레이어 이동 및 로그인 구조에 완전히 연결된 상태는 아니다.

---

## 4. 게임 시작 흐름

현재 기본 게임 모드는 `DefaultEngine.ini`에서 지정된다.

```text
DefaultEngine.ini
→ BP_LoginGameMode
→ BP_LoginPlayerController
→ WBP_Login
→ 로그인 성공
→ UI 입력 모드 해제
→ BP_PlayerCharacter 조작 시작
```

### 실제 클래스 연결

| 위치 | 설정값 |
|---|---|
| `DefaultEngine.ini` | `GlobalDefaultGameMode = BP_LoginGameMode` |
| `BP_LoginGameMode.DefaultPawnClass` | `BP_PlayerCharacter` |
| `BP_LoginGameMode.PlayerControllerClass` | `BP_LoginPlayerController` |
| `BP_LoginPlayerController.LoginWidgetClass` | `WBP_Login` |
| `BP_LoginPlayerController.ShowLoginOnBeginPlay` | `true` |

현재 `GameDefaultMap`은 프로젝트 전용 맵이 아니라 엔진의 `OpenWorld` 템플릿 맵이다. 실제 게임 맵이 생기면 프로젝트 맵으로 교체해야 한다.

---

## 5. 캐릭터 클래스

### 5.1 C++ 부모 클래스

파일:

- `Client/Source/HeavenHyperVoice/Character/UEPlayerCharacter.h`
- `Client/Source/HeavenHyperVoice/Character/UEPlayerCharacter.cpp`

클래스:

```cpp
AUEPlayerCharacter : public ACharacter
```

담당 기능:

- 걷기
- 달리기
- 정지 상태
- 구르기
- 점프
- 마우스 카메라 회전
- Enhanced Input Mapping Context 등록
- Input Action 바인딩
- 블루프린트용 이동 상태 제공

### 5.2 실제 블루프린트 캐릭터

에셋:

```text
/Game/Character/Player/BP_PlayerCharacter
```

부모 클래스:

```text
AUEPlayerCharacter
```

`BP_PlayerCharacter.InputData`에는 `/Game/Data/Input/DA_PlayerInput`이 연결되어 있다.

### 5.3 블루프린트에서 변경 가능한 수치

| 항목 | C++ 변수 | 기본값 |
|---|---|---:|
| 걷기 속도 | `WalkSpeed` | `260.0` |
| 달리기 속도 | `RunSpeed` | `560.0` |
| 구르기 속도 | `RollSpeed` | `820.0` |
| 구르기 시간 | `RollDuration` | `0.45`초 |
| 구르기 재사용 대기시간 | `RollCooldown` | `0.25`초 |
| 점프 속도 | `JumpVelocity` | `520.0` |
| 공중 제어력 | `AirControl` | `0.35` |
| 카메라 좌우 감도 | `LookYawRate` | `1.0` |
| 카메라 상하 감도 | `LookPitchRate` | `1.0` |

모두 `UPROPERTY`로 노출되어 있으므로 C++을 다시 수정하지 않고 캐릭터 블루프린트에서 변경할 수 있다.

### 5.4 블루프린트에서 읽을 수 있는 상태

| 변수 | 의미 |
|---|---|
| `MovementInput` | 현재 X/Y 이동 입력값 |
| `RollDirection` | 구르기가 진행되는 방향 |
| `bIsRunning` | 달리기 입력 상태 |
| `bIsRolling` | 구르기 진행 상태 |

---

## 6. Enhanced Input 구조

입력은 다음 단계를 거쳐 캐릭터 C++ 함수로 전달된다.

```text
키보드 또는 마우스
→ IMC_Player
→ Input Action
→ DA_PlayerInput
→ AUEPlayerCharacter::BindInputActions()
→ 실제 캐릭터 동작
```

### 키 바인딩

| 키 | Input Action | 결과 |
|---|---|---|
| `W` | `IA_MoveForward` | `MovementInput.X` 증가, `+X` 이동 |
| `S` | `IA_MoveBackward` | `MovementInput.X` 감소, `-X` 이동 |
| `D` | `IA_MoveRight` | `MovementInput.Y` 증가, `+Y` 이동 |
| `A` | `IA_MoveLeft` | `MovementInput.Y` 감소, `-Y` 이동 |
| `MouseX` | `IA_LookYaw` | 카메라 좌우 회전 |
| `MouseY` | `IA_LookPitch` | 카메라 상하 회전 |
| `LeftShift` | `IA_Run` | 달리기 시작 및 종료 |
| `LeftControl` | `IA_Roll` | 구르기 시작 |
| `SpaceBar` | `IA_Jump` | 점프 시작 및 종료 |

### 입력 데이터 에셋

에셋:

```text
/Game/Data/Input/DA_PlayerInput
```

이 에셋은 다음 항목을 참조한다.

- `IMC_Player`
- 이동 Input Action 4개
- 카메라 Input Action 2개
- 달리기, 구르기, 점프 Input Action 3개

캐릭터가 Input Action 경로를 직접 하드코딩하지 않고 데이터 에셋을 거쳐 찾도록 구성되어 있다.

---

## 7. 캐릭터 동작 상세

### 7.1 걷기와 정지

`MovementInput`이 0이 아니면 입력 방향으로 이동한다. 입력이 모두 0이면 별도 버튼 없이 정지 상태가 된다.

현재 정지 상태는 이동 입력과 속도가 0인 상태로 표현된다. 별도의 Idle 함수가 계속 실행되는 구조는 아니다.

### 7.2 달리기

`LeftShift`가 눌리면 `bIsRunning`이 `true`가 되고 최대 이동 속도가 `RunSpeed`로 변경된다.

키를 놓으면 `bIsRunning`이 `false`가 되고 최대 이동 속도가 `WalkSpeed`로 돌아간다.

### 7.3 구르기

구르기 입력 시 현재 이동 입력 방향을 우선 사용한다. 이동 입력이 없으면 캐릭터의 앞 방향 `+X`를 사용한다.

구르기 중에는 `RollSpeed`를 사용하고 `RollDuration`이 끝나면 원래 이동 속도로 복귀한다. 연속 구르기를 제한하기 위해 `RollCooldown`이 존재한다.

현재 구르기는 이동 기능만 구현되어 있다. 다음 기능은 아직 포함되지 않는다.

- 구르기 몽타주
- Root Motion
- 무적 프레임
- 스태미나 소모
- 피격 취소 규칙

### 7.4 점프

점프 입력은 `ACharacter::Jump()`와 `StopJumping()`을 사용한다.

점프 높이에 영향을 주는 `JumpVelocity`와 공중 이동에 영향을 주는 `AirControl`은 블루프린트에서 변경할 수 있다.

---

## 8. 애니메이션 구조

파일:

- `Client/Source/HeavenHyperVoice/Animation/UEAnimInstance.h`
- `Client/Source/HeavenHyperVoice/Animation/UEAnimInstance.cpp`

클래스:

```cpp
UUEAnimInstance : public UAnimInstance
```

이 클래스는 캐릭터 이동 코드를 직접 실행하지 않는다. 캐릭터 상태를 읽어서 Animation Blueprint에 전달하는 데이터 연결부다.

### Animation Blueprint에 제공하는 값

| 값 | 의미 |
|---|---|
| `OwnerCharacter` | 현재 애니메이션을 소유한 캐릭터 |
| `GroundSpeed` | 수평 이동 속도 |
| `DirectionAngle` | 캐릭터 앞 방향을 기준으로 한 이동 각도 |
| `MovementInput` | 현재 이동 입력 |
| `bIsMoving` | 이동 중인지 여부 |
| `bIsRunning` | 달리기 중인지 여부 |
| `bIsRolling` | 구르기 중인지 여부 |
| `bIsFalling` | 공중에 있는지 여부 |

### 현재 애니메이션 상태

`BP_PlayerCharacter`에는 아직 다음 값이 연결되지 않았다.

- Skeletal Mesh
- Skeleton
- Animation Blueprint
- Idle, Walk, Run, Roll, Jump 애니메이션 에셋

따라서 `UUEAnimInstance` C++ 뼈대와 상태값 계산은 존재하지만, 실제 애니메이션은 아직 화면에서 재생되지 않는다.

캐릭터 메시와 스켈레톤이 결정된 후 해당 스켈레톤을 부모로 하는 `ABP_Player`를 만들고 `UUEAnimInstance`를 부모 클래스로 지정해야 한다.

---

## 9. Gameplay Tag 구조

파일:

- `Client/Source/HeavenHyperVoice/UEGameplayTags.h`
- `Client/Source/HeavenHyperVoice/UEGameplayTags.cpp`

### 입력 태그

```text
Input.Action.MoveForward
Input.Action.MoveBackward
Input.Action.MoveRight
Input.Action.MoveLeft
Input.Action.LookYaw
Input.Action.LookPitch
Input.Action.Run
Input.Action.Roll
Input.Action.Jump
```

입력 태그는 `DA_PlayerInput`에서 알맞은 Input Action을 찾을 때 실제로 사용된다.

### 캐릭터 상태 태그

```text
State.Character.Idle
State.Character.Walk
State.Character.Run
State.Character.Roll
State.Character.Jump
State.Character.Fall
```

상태 태그는 선언되어 있지만 현재 캐릭터의 Ability System Component에 추가하거나 제거하는 코드에는 아직 연결되지 않았다.

즉, 현재 상태는 다음과 같다.

| 태그 종류 | 현재 상태 |
|---|---|
| `Input.Action.*` | 실제 입력 검색에 사용 중 |
| `State.Character.*` | 이름과 구조만 준비됨 |

---

## 10. 로그인 및 회원가입 구조

### 10.1 UI 계층

```text
UUELoginWidget (C++)
└─ WBP_Login (Widget Blueprint)
```

파일:

- `Client/Source/HeavenHyperVoice/UI/Login/UELoginWidget.h`
- `Client/Source/HeavenHyperVoice/UI/Login/UELoginWidget.cpp`
- `Client/Content/UI/Login/WBP_Login.uasset`

UI는 다음 두 화면 모드를 가진다.

- 로그인
- 회원가입

### 10.2 회원가입 입력 항목

회원가입에는 다음 항목이 필요하다.

- 닉네임
- 아이디
- 아이디 중복확인
- 비밀번호
- 비밀번호 확인

### 10.3 회원가입 규칙

| 항목 | 규칙 |
|---|---|
| 닉네임 | 2~16자 |
| 아이디 | 4~20자 |
| 아이디 문자 | 영문, 숫자, `_` |
| 비밀번호 | 6자 이상 |
| 비밀번호 확인 | 비밀번호와 동일해야 함 |
| 중복확인 | 통과해야 회원가입 가능 |

중복확인을 통과한 뒤 아이디를 수정하면 확인 상태는 즉시 해제된다.

회원가입 버튼을 누를 때 계정 시스템이 중복 여부를 다시 검사한다. 중복확인 이후 상태가 바뀌어도 중복 계정이 생성되지 않게 하기 위한 처리다.

### 10.4 계정 저장과 검증

파일:

- `Client/Source/HeavenHyperVoice/System/Account/UEAccountSaveGame.h`
- `Client/Source/HeavenHyperVoice/System/Account/UEAccountSubsystem.h`
- `Client/Source/HeavenHyperVoice/System/Account/UEAccountSubsystem.cpp`

역할:

| 클래스 | 책임 |
|---|---|
| `UUEAccountSaveGame` | 로컬 계정 데이터 형식 |
| `UUEAccountSubsystem` | 중복확인, 회원가입, 로그인 검증, 저장 및 불러오기 |
| `UUELoginWidget` | 사용자 입력과 결과 메시지 표시 |
| `AUEPlayerController` | 로그인 성공 후 UI 제거와 게임 입력 전환 |

런타임 계정 데이터는 다음 슬롯에 저장된다.

```text
Client/Saved/SaveGames/HV_LocalAccounts.sav
```

`Saved` 폴더는 Git에 포함되지 않는다.

비밀번호 평문은 저장하지 않는다. 계정별 salt와 BLAKE3 해시값만 저장한다.

단, 이 기능은 클라이언트 로컬 프로토타입이다. 클라이언트 파일은 사용자가 수정할 수 있으므로 실제 온라인 서비스의 보안 인증으로 사용할 수 없다. 정식 서비스에서는 서버 인증 API로 교체해야 한다.

### 10.5 로그인 실패 처리

다음 경우에는 로그인이 처리되지 않는다.

- 아이디가 존재하지 않음
- 비밀번호가 일치하지 않음
- 아이디 또는 비밀번호가 비어 있음

아이디 존재 여부를 외부에 노출하지 않기 위해 없는 아이디와 틀린 비밀번호는 같은 실패 메시지를 사용한다.

로그인 성공 이벤트가 발생해야만 `AUEPlayerController`가 로그인 UI를 닫고 입력 모드를 게임으로 전환한다.

---

## 11. 현재 완료 범위

### 완료됨

- C++ 플레이어 캐릭터 부모 클래스
- `BP_PlayerCharacter` 블루프린트 자식
- W/S/A/D 이동
- X축 앞 방향 규칙
- 걷기와 달리기
- 코드 기반 구르기
- 점프
- 마우스 카메라 회전
- Enhanced Input Action 9개
- `IMC_Player`
- `DA_PlayerInput`
- 입력 Gameplay Tag
- 캐릭터 상태 Gameplay Tag 선언
- 애니메이션 데이터용 `UUEAnimInstance`
- 로그인 UI
- 회원가입 UI
- 아이디 중복확인
- 로컬 계정 저장과 로그인 검증
- GameMode, PlayerController, Pawn, Login Widget 연결
- UE 5.8 Development Editor 빌드

### 추가 작업 필요

- 실제 플레이어 Skeletal Mesh와 Skeleton
- 실제 Animation Blueprint
- Idle, Walk, Run, Roll, Jump 애니메이션
- 구르기 몽타주와 Root Motion
- 구르기 무적 프레임
- 스태미나 시스템
- 캐릭터 상태 태그의 ASC 연결
- 공격, 회피, 피격, 사망 상태
- 다크소울식 타깃 고정과 카메라
- 프로젝트 전용 게임 맵
- 서버 기반 계정 인증

---

## 12. 새 캐릭터를 추가하는 방법

1. `BP_PlayerCharacter`를 부모로 하는 자식 블루프린트를 만든다.
2. `Content/Character/Player` 아래에서 캐릭터 종류별 하위 폴더를 만든다.
3. 자식 블루프린트에 Skeletal Mesh를 지정한다.
4. 해당 Skeleton용 Animation Blueprint를 지정한다.
5. 걷기, 달리기, 구르기, 점프 수치를 캐릭터에 맞게 변경한다.
6. 공통 동작을 바꿔야 할 때만 `AUEPlayerCharacter` C++을 수정한다.

권장 예시:

```text
Content/Character/Player/
├─ Base/
│  └─ BP_PlayerCharacter
├─ Knight/
│  ├─ BP_Player_Knight
│  └─ Animation/
│     └─ ABP_Player_Knight
└─ Mage/
   ├─ BP_Player_Mage
   └─ Animation/
      └─ ABP_Player_Mage
```

캐릭터별 차이가 단순 수치와 메시 정도라면 C++ 클래스를 새로 만들지 않고 자식 블루프린트로 처리한다. 완전히 다른 이동 규칙이나 능력 구조가 필요할 때만 별도 C++ 자식 클래스를 만든다.

---

## 13. 팀 작업 시 주의사항

- `Client/Plugins/MCPUnreal`은 로컬 자동화 도구이며 Git에 포함하지 않는다.
- `.uproject`에 MCP 플러그인 의존성을 추가하지 않는다.
- `Source`, `Content`, `Config` 변경 파일은 팀에서 함께 받아야 한다.
- `Binaries`, `Intermediate`, `Saved`, `DerivedDataCache`는 Git에 포함하지 않는다.
- `BP_PlayerCharacter`의 앞 방향은 항상 `+X`를 유지한다.
- 애니메이션 상태와 게임플레이 상태를 같은 코드에 중복 구현하지 않는다.
- 로그인 UI가 계정 저장 로직을 직접 소유하지 않도록 `UUEAccountSubsystem`을 통해 접근한다.

---

## 14. 주요 파일 바로가기

- [플레이어 캐릭터 헤더](../Client/Source/HeavenHyperVoice/Character/UEPlayerCharacter.h)
- [플레이어 캐릭터 구현](../Client/Source/HeavenHyperVoice/Character/UEPlayerCharacter.cpp)
- [애니메이션 인스턴스 헤더](../Client/Source/HeavenHyperVoice/Animation/UEAnimInstance.h)
- [애니메이션 인스턴스 구현](../Client/Source/HeavenHyperVoice/Animation/UEAnimInstance.cpp)
- [입력 데이터 에셋 클래스](../Client/Source/HeavenHyperVoice/Data/UEDataAsset.h)
- [Gameplay Tag 선언](../Client/Source/HeavenHyperVoice/UEGameplayTags.h)
- [Gameplay Tag 정의](../Client/Source/HeavenHyperVoice/UEGameplayTags.cpp)
- [게임 모드](../Client/Source/HeavenHyperVoice/GameMode/UEGameModeBase.cpp)
- [플레이어 컨트롤러](../Client/Source/HeavenHyperVoice/Player/UEPlayerController.cpp)
- [로그인 위젯](../Client/Source/HeavenHyperVoice/UI/Login/UELoginWidget.cpp)
- [계정 서브시스템](../Client/Source/HeavenHyperVoice/System/Account/UEAccountSubsystem.cpp)

