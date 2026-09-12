# 클라이언트 이동 코어 실험

이번 구현은 **클라이언트 플레이어의 로컬 이동 테스트**다. 서버 소스, 와이어 프로토콜, 야생 포켓몬의 이동은 변경하지 않는다. 입력 전송·실제 서버 검증은 연결하지 않는다. 최근 600틱의 입력·설정·상태를 보관하고 독립 C++ 프로그램에서 재실행해 비교한다.

## 구조

`AUEPlayerCharacter (APawn)` → `UUECoreMovementComponent (UPawnMovementComponent)` → `hhv::movement::simulate`

플레이어는 `ACharacter`를 상속하지 않으며 `UCharacterMovementComponent`를 생성·상속·호출하지 않는다. 캡슐·메시·카메라를 직접 소유한다. 다른 기존 포켓몬 클래스의 CharacterMovement는 이번 플레이어 실험에서 교체하지 않는다.

`MovementCore.h`는 언리얼 헤더를 포함하지 않는 C++ 코드다. 입력, 설정, 상태, 충돌 질의 인터페이스만 사용하므로 향후 NPC나 포켓몬도 자기 입력을 제공할 수 있다. 길찾기·플레이어 컨트롤러·네트워크·애니메이션은 코어 밖에 있다.

`TriangleWorld.h`도 표준 C++17이다. BVH로 후보 삼각형을 찾고 연속 캡슐 스윕·접촉·침투를 계산한다. **게임 중에도 바로 이 구현을 사용하며 언리얼 스윕이나 Chaos 질의를 호출하지 않는다.** 코어 폴더를 그대로 복사하거나 CMake의 `HHV::MovementCore`에 연결하면 언리얼 설치 없이 서버에서 사용할 수 있다.

`UUECoreCollisionSubsystem`은 월드당 하나의 충돌 스냅샷을 공유한다. UE 컴포넌트는 입력 수집, Details 설정 변환, 결과 위치·애니메이션 상태 표시만 담당한다. 플레이어·NPC 식별자는 코어에 없다.

## 실행과 설정

1. 에디터를 완전히 종료한 상태에서 `HeavenHyperVoiceEditor`를 빌드한다. 기본 클래스 변경이 있으므로 Live Coding만으로 반영하지 않는다.
2. 플레이어가 있는 테스트 레벨을 열어 플레이한다. 기존 `BP_PlayerCharacter`의 네이티브 부모 이름은 유지된다.
3. 플레이어의 **Character → Movement Test → Local Movement Test**는 기본으로 켜져 있다. 이 상태에서는 필드/인스턴스 연결 시작을 건너뛰어 기존 서버 좌표 보정이 로컬 이동 실험을 방해하지 않는다.
4. **도구(Tools) → 공통 이동 충돌 저장**으로 `ServerGround`/`ServerWall` 프리셋 형상을 미리 추출한다. 클라이언트와 서버 데이터 폴더에 같은 파일을 저장한다. **Collision File**을 비워두면 `Content/MovementCollision/<맵 패키지 경로>.hhvcollision`을 자동으로 읽는다. PIE에서도 장면을 자동 추출하지 않는다. [자세한 저장 방법](MAP_EXPORT.md)을 참고한다.
5. 플레이어 블루프린트의 **CoreMovement** 컴포넌트를 선택하고 **Shared Movement** 항목을 수정한다. 캡슐 크기는 **CollisionCylinder**에서 수정한다.

| 설정 | 초기값 | 의미 |
|---|---:|---|
| Max Walk Speed / Core Run Speed | 260 / 390 cm/s | 걷기 / 달리기 |
| Max Acceleration | 2048 cm/s² | 입력 가속 |
| Braking Deceleration Walking / Ground Friction | 2048 / 8 | 지상 감속 |
| Jump Z Velocity / Gravity Acceleration | 420 / 980 | 점프 초기 속도 / 중력 |
| Air Control | 0.35 | 공중 방향 조작 |
| Max Step Height / Walkable Floor Angle | 45 cm / 44° | 계단 / 경사 제한 |
| Floor Snap Distance / Contact Skin | 3 / 0.1 cm | 바닥 탐색 / 접촉 여유 |
| Terminal Fall Speed | 4000 cm/s | 낙하 속도 제한 |
| Rotation Speed | 540°/s | 입력 방향으로 회전 |
| Core Roll Speed / Core Roll Duration | 600 / 0.6초 | 구르기 이동 |

이동은 1/60초 고정 간격으로 처리한다. 한 프레임의 누적 시간은 최대 0.25초로 제한한다. 심한 멈춤 후 모든 시간을 따라잡는 대신 과도한 반복 계산을 제한하는 초기 정책이다. 현재 별도의 화면 보간은 없으므로 고주사율에서 60Hz 위치 갱신이 보일 수 있다.

## 애니메이션 블루프린트 연결

애니메이션 블루프린트 에셋은 자동 수정하지 않는다. 기존 `Get Character Movement` 또는 `Character` 전용 노드는 직접 교체한다.

`Try Get Pawn Owner` → `Cast to UEPlayerCharacter` → **Get Core Movement**에서 다음 값을 읽을 수 있다.

| 값 / 함수 | 용도 |
|---|---|
| Pawn의 Get Velocity / 컴포넌트의 Velocity | 이동 속도, Z 속도 |
| Get Current Acceleration / Acceleration | 입력 가속 방향·크기 |
| Is Moving On Ground | 접지 여부 |
| Is Falling | 공중 여부: 상승과 하강 모두 true |
| Movement Mode | Grounded / Falling / Disabled |
| Floor Normal | 현재 바닥 방향 |
| Is Wall Sliding | 이번 이동의 벽 미끄러짐 |
| Is Core Rolling | 코어 구르기 진행 여부 |
| On Landed / On Jump Apex / On Movement Updated | 착지 / 정점 / 이동 갱신 이벤트 |

점프 상승은 `Is Falling && Velocity.Z > 0`, 낙하 하강은 `Is Falling && Velocity.Z <= 0`으로 나눈다. 기존 플레이어의 `IsRunning`, `GetMovementInput`, `GetCharacterStateTag`도 유지한다. 네이티브 `UUEAnimInstance`가 값을 읽는 경로만 새 컴포넌트로 바꿨다.

## 테스트와 현재 범위

언리얼 자동 테스트: `HHV.Movement.Core` (평지·점프·30/60/120FPS·벽·모서리·계단·경사·낭떠러지·플레이어 컴포넌트 구성). 기존 `HHV.Movement`의 좌표 보정/보간 테스트도 빌드 대상으로 유지한다.

언리얼 없이 코어만 실행:

```text
cmake -S Client/Source/MovementCore -B Client/Intermediate/CoreMovement
cmake --build Client/Intermediate/CoreMovement --config Debug
ctest --test-dir Client/Intermediate/CoreMovement -C Debug --output-on-failure
```

게임과 독립 실행 테스트는 둘 다 `TriangleWorld.h`의 동일한 구현을 사용한다.

### 클라이언트 기록 재실행

플레이 중 CoreMovement의 Blueprint 함수 **Export Core Replay**를 호출한다. Base Path를 비우면 `Client/Saved/CoreMovement/LastReplay.hhvcollision`과 `.hhvreplay`가 생성된다. 최근 600틱을 보관하며 순간이동·강제 속도/모드 변경·정지·충돌 데이터 변경 시 기록을 초기화한다. 별도 전송은 하지 않는다.

```text
Client/Intermediate/CoreMovement/Debug/MovementReplay.exe Client/Saved/CoreMovement/LastReplay.hhvcollision Client/Saved/CoreMovement/LastReplay.hhvreplay
```

독립 프로그램은 시작 상태에서 모든 입력을 순서대로 시뮬레이션한다. 매 틱의 위치·속도·가속도·바닥 법선·회전·구르기·이동 모드·벽 상태를 기록과 비교하며, 오차 또는 잘못된 버전/맵 해시/입력 순서를 만나면 실패한다. 비교 허용값은 실수 항목 0.001, 열거형·불리언·틱 수는 정확 일치다. 테스트 파일은 서버의 신뢰할 수 없는 입력 처리 프로토콜이 아니다.

### 순수 C++ 호스트에 연결

```cpp
#include "TriangleWorld.h"
#include <fstream>

hhv::movement::TriangleWorld world;
std::ifstream file("PlayerTestLevel.hhvcollision");
if (!world.load(file)) return; // 호스트에서 오류 처리
hhv::movement::Config config;
hhv::movement::State state;
state.position = {0, 0, 88.1f};
hhv::movement::Input input{1, 1.f, 0.f, 0};
hhv::movement::simulate(state, input, config, world); // 정확히 1/60초
```

CMake에서 `add_subdirectory(path/to/MovementCore)` 후 `target_link_libraries(host PRIVATE HHV::MovementCore)`를 사용한다. 서버에 넣을 때 테스트 실행 파일이 필요 없다면 `HHV_MOVEMENT_BUILD_TESTS=OFF`로 설정한다. 코어는 네트워크나 OS API, UE 타입, 물리 엔진 라이브러리를 요구하지 않는다. 엔티티마다 `State`를 소유하고 여러 엔티티가 읽기 전용 `TriangleWorld`를 공유할 수 있다. `build/load`는 시뮬레이션 스레드와 동시에 호출하지 않는다.

### 충돌 파일과 좌표 규약

- `.hhvcollision`은 버전·삼각형 수·형상 해시와 월드 좌표 삼각형을 담는다. float 왕복 정밀도를 보존하는 텍스트이며 잘못된 파일을 읽으면 기존 월드를 유지한다.
- 단위 cm, Z 위쪽, 위치는 캡슐 중심이다. **기존 서버의 X/Y +25600 오프셋을 코어 안에서 적용하지 않는다.** 실제 서버 연결 시 외부 어댑터에서 입력/출력 좌표를 변환하거나 호스트 모두 동일한 원점을 사용해야 한다. 기존 navmesh `.hhvmap`은 이 파일과 서로 다른 형식이다.
- 같은 버전의 코드·충돌 파일·설정·초기 상태·틱별 입력이 재실행 조건이다. float 기반이므로 서로 다른 CPU/컴파일러의 비트 단위 결정성을 보장하지 않는다. fast-math를 끄고 실제 서버 플랫폼에서도 재실행 허용오차를 검증해야 한다.
- 에디터 변환은 Pawn을 제외한 `ServerGround`/`ServerWall` Collision Presets 컴포넌트만 대상으로 한다. 이름·태그로 대상을 확장하지 않는다. Box, StaticMesh LOD0(인스턴스별 변환 포함), Landscape의 단순 충돌 높이/구멍을 지원한다. StaticMesh는 **시각 LOD0 형상**을 공통 충돌 형상으로 쓰므로 기존 UE의 단순 충돌 모양과 다를 수 있다. 지원하지 않는 차단 컴포넌트는 오류를 내고 변환을 중단한다. 필요하면 Box/StaticMesh 프록시를 배치한다.
- 스냅샷은 정적 지형이다. 이동 발판·동적 장애물·다른 Pawn은 포함하지 않는다. 장면 변경 후 **공통 이동 충돌 저장** 메뉴로 다시 추출한다. 클라이언트 파일은 패키징 데이터에 포함되고 서버 파일은 `Server/maps/collision`에 함께 저장한다. 서버 실행부 연결은 별도 단계다.

초기 범위는 정적 지형의 걷기·달리기·가속·감속·점프·낙하·착지·벽 미끄러짐·모서리·계단·경사·간단한 침투 복구다. 이동 발판 운반, 수영/비행/웅크리기, 물체 밀기, 루트 모션 이동, 네트워크 재시뮬레이션은 구현하지 않았다. 기존 구르기 몽타주는 표시용이며 위치는 코어가 결정한다.

## 검증

`HHV.Movement` 자동 테스트는 이동 기능과 실제 컴포넌트 연결을 검사한다. `PortableCollisionReplay`는 형상 변환 후 언리얼 바닥/벽 충돌을 비활성화하고도 코어가 충돌하는지 검증하며, 독립 실행 검사용 기록을 저장한다. `Wall`, `Step`, `Slope`, `PortableReplay`의 `.hhvcollision`/`.hhvreplay` 쌍은 `Client/Saved/CoreMovement`에 생성된다.

자동 테스트는 화면을 렌더링하지 않는 환경에서 실행한다. 최종 애니메이션 블루프린트 연결과 화면에서의 조작감 조정은 별도로 진행한다.

검증 기록 (2026-09-11, Windows/MSVC):

- Unreal Engine 5.8 Development Editor 빌드 성공.
- `HHV.Movement` 6개 성공, 경고/실패 0개.
- 독립 CMake/CTest 성공. 클라이언트가 내보낸 Wall 120틱, Step 100틱, Slope 130틱, PortableReplay 130틱을 독립 실행 파일로 검증해 전부 일치.
- 실제 `PlayerTestLevel`에서 624개 삼각형을 변환하고 독립 실행 파일에서 같은 해시로 로드 확인.
- `Server/` 소스 변경 없음. Linux 등 다른 호스트에서의 빌드/재실행은 아직 실행하지 않았음.

### PIE 시작 시 정지 문제 회귀 검사

`HHV.Movement.Core.IgnoreGameplayDebuggerRendering`은 PIE에서 생성되는 실제 GameplayDebugger 렌더링 컴포넌트를 추가한 상태로 충돌 스냅샷 준비와 접지·이동·회전·점프를 검사한다. 디버그 표시 컴포넌트는 차단 플래그가 있더라도 지형 수집에서 제외한다. 지원하지 않는 실제 장애물의 오류 처리는 유지한다.

2026-09-12 검증: 수정 전 재현 테스트 실패 확인, 수정 후 Editor 빌드 성공 및 `HHV.Movement` 7개 성공(경고/실패 0개).

### 경사와 모서리 접지 개선 (이동 코어 버전 2)

경사 입구에서 옆면이 먼저 잡히면 발을 받치는 윗면이나 평지를 놓치는 문제가 있었다. 이제 일반 충돌 질의로 이동을 막는 면을 확인하고, 필요할 때 `CollisionWorld::sweepFloor`로 지지면을 별도로 찾는다. 지지면이 있더라도 실제 위치를 내리는 거리는 최초 장애물까지로 제한한다. 따라서 다른 면 뒤의 바닥을 발견했다는 이유로 장애물을 통과하지 않는다.

`TriangleWorld`는 같은 모서리의 거의 동시 접촉을 0.001cm의 월드 거리 허용오차로 비교한다. 바닥 판정에서는 실제 지형 면의 각도와 캡슐 아래쪽 접촉 여부를 확인한다. 캡슐의 둥근 바닥이 계단 끝에 닿으면 접촉 방향이 계단 윗면보다 가팔라질 수 있으므로, 접촉 방향만으로 계단을 거부하지 않는다.

접지 상태에서는 경사를 따라 이동한 높이와 점프의 Z 속도를 구분한다. 충돌로 위치가 올라가더라도 지상 Z 속도는 0으로 유지하고, 공중에서는 충돌이 기존 상승 속도보다 큰 상승을 만들지 못하게 한다. 실제 점프는 기존 초기 속도로 정상 상승한다. 급경사에서는 지상 이동을 막는 처리와 공중에서 아래로 미끄러지는 처리를 구분해, 위치가 멈춘 채 낙하 속도만 쌓이는 상황을 방지한다.

`TerrainContactTests`는 경사에서의 Z 속도, 공중 상승 증폭, 실제 점프, 지지 없는 급경사 낙하, 장애물을 통과하는 바닥 보정 여부를 검사한다. CMake가 PlayerTestLevel 충돌 파일을 찾으면 `PlayerTestLevelMovement`도 실행한다. 이 검사는 6개 각도 × 17개 진입 위치 × 3개 속도의 306가지 조합을 매 틱 확인하고, 40° 가장자리 구르기와 실제 문제 좌표의 바닥 질의도 검사한다. 급경사 접근은 조합마다 10초 동안 접지가 유지되는지 확인한다.

독립 코어를 다른 저장소에 복사한 경우 `HHV_MOVEMENT_TEST_MAP`에 충돌 파일 경로를 지정할 수 있다. 파일이 없으면 실제 맵 검사는 등록하지 않고 일반 지형 테스트만 실행한다.

이동 결과가 달라졌으므로 재실행 버전은 **2**로 올렸다. 버전 1의 기존 `.hhvreplay`는 새 코어에서 검증하지 않으며, 새 기록을 내보내야 한다. `.hhvcollision`의 형식과 맵 데이터는 변경하지 않았다.

검증 결과: Debug·Release CTest 통과, PlayerTestLevel 전체 566개 주행 기록(66,631틱)의 독립 재실행 일치, Unreal Engine 5.8 Editor 빌드 성공, `HHV.Movement` 9개 통과. 언리얼에서 새로 내보낸 Wall·Step·Slope·PortableReplay 기록도 독립 실행 프로그램에서 모두 일치했다.
