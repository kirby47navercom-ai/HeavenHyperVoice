# 공통 이동 코어

플레이어, 야생 포켓몬, 파트너, 보스의 위치는 `MovementCore.h`의 `simulate`로 계산한다. 클라이언트와 서버가 같은 소스를 직접 포함한다. 플레이어와 포켓몬은 `APawn`이며 CharacterMovement를 생성하지 않는다.

## 입력부터 화면까지

1. 클라이언트 `UECoreMovementComponent`가 방향·점프·구르기를 1/60초 단위 입력으로 만든다. 입력이 없어도 정지 입력을 만들어 중력과 감속을 계산한다.
2. `PredictionQueue`가 입력에 순번을 붙이고 공통 코어를 실행한다. 입력과 계산 결과를 큐에 남기고, 결과 위치를 즉시 화면에 표시한다.
3. `UEFieldServerBridgeComponent`가 아직 전송하지 않은 입력과 예측 좌표를 묶어 보낸다. 한 묶음은 최대 15틱이다.
4. 서버 `AuthoritativeQueue`가 순번과 유효값을 검사하고 같은 충돌 파일·설정·입력으로 다시 계산한다. 좌표는 비교용이다. 잘못된 좌표를 보내도 서버 위치를 그 좌표로 옮기지 않는다. 처리량은 서버 시간으로 제한한다.
5. 서버가 확인한 순번과 전체 상태를 돌려준다. 위치뿐 아니라 속도, 낙하 여부, 바닥 법선, 구르기 잔여 시간도 포함한다.
6. 클라이언트는 확인된 입력을 큐에서 빼고, 서버 상태에서 미확인 입력을 다시 실행한다. 응답을 기다리느라 조작이 늦어지는 문제와 서로 다른 상태에서 계산하는 문제를 함께 다룬다.

큐는 최대 600틱이다. 확인받지 못한 입력을 조용히 버리지 않고 큐가 가득 차면 예측을 멈춘다. 중복·누락 순번이나 잘못된 값은 서버가 거절한다. TCP 연결을 새로 열면 입장 상태와 순번도 새로 시작한다.

## 서버 개체와 길찾기

Lua AI는 목적지와 행동을 고른다. `Server/Movement`의 길찾기는 공통 충돌 데이터에서 캡슐이 이동할 수 있는 경로를 찾는다. `Map::advance`는 경로 방향을 입력으로 바꾸고 같은 60Hz 코어를 실행한다. 파트너도 이 경로를 사용한다. 짧은 경유점에 접근할 때 감속해 관성으로 모서리 밖으로 나가지 않도록 한다.

다른 플레이어와 포켓몬은 서버 상태를 시간순으로 보관하고 위치·회전을 보간해 표시한다. 낙하와 구르기 상태도 서버에서 받는다. 원격 개체의 클라이언트 이동 컴포넌트는 별도로 물리 시뮬레이션하지 않는다.

## 데이터와 설정

- `TriangleWorld.h`는 표준 C++ 충돌 구현이며 언리얼 Chaos 질의를 사용하지 않는다.
- 클라이언트 `Content/MovementCollision`과 서버 `maps/collision`에는 같은 `.hhvcollision` 파일을 배포한다. [추출 방법](MAP_EXPORT.md).
- 입장 시 코어 버전과 맵 해시를 확인한다. 구버전 클라이언트와 새 서버는 함께 사용할 수 없다.
- 네트워크 플레이어는 양쪽에서 기본 `Config`를 사용한다. Blueprint의 로컬 이동 시험용 속도·중력 변경은 네트워크 규칙을 바꾸지 않는다. 규칙을 바꾸려면 공통 설정과 양쪽 실행 파일을 함께 갱신한다.
- 단위는 cm, Z-up, 캡슐 중심 좌표다. 저장/시야 계산의 XY 오프셋은 외부 어댑터에서만 적용한다. 필드 25600, 인스턴스 153600이다.
- 정적 지형만 포함한다. 이동 발판, 동적 장애물, 다른 Pawn 충돌, 수영·비행·루트 모션 이동은 아직 지원하지 않는다.
- float 계산이므로 다른 CPU/컴파일러 사이의 비트 단위 일치를 보장하지 않는다. 새로운 서버 플랫폼에서는 실제 재실행 검증이 필요하다.

## 코드 위치

| 파일 | 역할 |
|---|---|
| MovementCore.h | 고정 시간 이동, 접지, 점프, 구르기 |
| TriangleWorld.h | 삼각형 충돌과 바닥 질의 |
| MovementPrediction.h | 클라이언트 예측 큐와 서버 검증 큐 |
| MovementWire.h | 전체 상태와 입력의 공통 직렬화 |
| MovementReplay.h | 저장된 기록의 독립 재실행 |
| Server/Movement/src/Map.cpp | 서버 좌표 변환과 AI 입력 시뮬레이션 |
| Server/Movement/src/Path.cpp | 같은 충돌 형상을 사용하는 길찾기 |

## 검증 실행

```text
cmake -S Client/Source/MovementCore -B Client/Intermediate/CoreMovement
cmake --build Client/Intermediate/CoreMovement --config Debug
ctest --test-dir Client/Intermediate/CoreMovement -C Debug --output-on-failure
```

언리얼 자동 테스트 이름은 `HHV.Movement`다. `Export Core Replay`로 저장한 `.hhvreplay`와 `.hhvcollision` 쌍은 독립 `MovementReplay` 실행 파일로 검증할 수 있다.

서버를 `BUILD_TESTING=ON`으로 빌드하면 `FieldMovementTests`와 `MovementNetworkTest`가 생성된다. 후자는 별도로 실행한 로컬 `--dev-no-auth` 서버를 대상으로 한다.

```text
FieldMovementTests.exe maps/collision/Stage/Filed.hhvcollision
MovementNetworkTest.exe 9200 maps/collision/Environments/Goldenrod_R03/Maps/L_Goldenrod.hhvcollision
MovementNetworkTest.exe 9300 maps/collision/Stage/Filed.hhvcollision 1
```

네트워크 검사는 360틱의 입력 재현, 점프·구르기 상태, 위조 좌표 무시, 확인 큐 제거, 중복 입력 거절을 확인한다.

## main 통합 검증 (2026-09-13)

- Windows/MSVC 서버 Debug·Release 빌드와 UE 5.8 Development Editor 빌드 성공.
- 독립 코어 CTest 4개, 언리얼 `HHV.Movement` 9개 통과. PlayerTestLevel 경사와 모서리 검사를 포함한다.
- 실제 Filed·Goldenrod 충돌 파일에서 경로·파트너 추적 검사 통과.
- 필드·인스턴스 실제 TLS 접속에서 360틱 재현, 점프·구르기, 위조 좌표, 순번 확인, 중복 입력 거절 통과.
- 12마리 야생 포켓몬의 실제 이동과 전체 코어 상태 수신 확인.
- 인스턴스 방 분할·격리·종류 분리·잘못된 종류 거절·빈 방 회수 통과.
- 플레이어·포켓몬·애니메이션 Blueprint 5개 재컴파일 및 저장. 플레이어와 포켓몬의 CharacterMovement 부재 확인.

포탈은 서버 위치로 입장한 뒤 한 번 트리거 밖에 있어야 활성화된다. 복귀 위치가 포탈 안이어도 즉시 재입장하지 않으며, 임의 순간이동 좌표를 서버에 보내지 않는다.

이 검사는 화면을 렌더링하지 않는 자동 검사다. 실제 화면에서의 조작감·애니메이션 평가는 별도로 필요하다.

### 경사와 모서리 접지 개선 (이동 코어 버전 2)

경사 입구에서 옆면이 먼저 잡히면 발을 받치는 윗면이나 평지를 놓치는 문제가 있었다. 이제 일반 충돌 질의로 이동을 막는 면을 확인하고, 필요할 때 `CollisionWorld::sweepFloor`로 지지면을 별도로 찾는다. 지지면이 있더라도 실제 위치를 내리는 거리는 최초 장애물까지로 제한한다. 따라서 다른 면 뒤의 바닥을 발견했다는 이유로 장애물을 통과하지 않는다.

`TriangleWorld`는 같은 모서리의 거의 동시 접촉을 0.001cm의 월드 거리 허용오차로 비교한다. 바닥 판정에서는 실제 지형 면의 각도와 캡슐 아래쪽 접촉 여부를 확인한다. 캡슐의 둥근 바닥이 계단 끝에 닿으면 접촉 방향이 계단 윗면보다 가팔라질 수 있으므로, 접촉 방향만으로 계단을 거부하지 않는다.

접지 상태에서는 경사를 따라 이동한 높이와 점프의 Z 속도를 구분한다. 충돌로 위치가 올라가더라도 지상 Z 속도는 0으로 유지하고, 공중에서는 충돌이 기존 상승 속도보다 큰 상승을 만들지 못하게 한다. 실제 점프는 기존 초기 속도로 정상 상승한다. 급경사에서는 지상 이동을 막는 처리와 공중에서 아래로 미끄러지는 처리를 구분해, 위치가 멈춘 채 낙하 속도만 쌓이는 상황을 방지한다.

`TerrainContactTests`는 경사에서의 Z 속도, 공중 상승 증폭, 실제 점프, 지지 없는 급경사 낙하, 장애물을 통과하는 바닥 보정 여부를 검사한다. CMake가 PlayerTestLevel 충돌 파일을 찾으면 `PlayerTestLevelMovement`도 실행한다. 이 검사는 6개 각도 × 17개 진입 위치 × 3개 속도의 306가지 조합을 매 틱 확인하고, 40° 가장자리 구르기와 실제 문제 좌표의 바닥 질의도 검사한다. 급경사 접근은 조합마다 10초 동안 접지가 유지되는지 확인한다.

독립 코어를 다른 저장소에 복사한 경우 `HHV_MOVEMENT_TEST_MAP`에 충돌 파일 경로를 지정할 수 있다. 파일이 없으면 실제 맵 검사는 등록하지 않고 일반 지형 테스트만 실행한다.

이동 결과가 달라졌으므로 재실행 버전은 **2**로 올렸다. 버전 1의 기존 `.hhvreplay`는 새 코어에서 검증하지 않으며, 새 기록을 내보내야 한다. `.hhvcollision`의 형식과 맵 데이터는 변경하지 않았다.

검증 결과: Debug·Release CTest 통과, PlayerTestLevel 전체 566개 주행 기록(66,631틱)의 독립 재실행 일치, Unreal Engine 5.8 Editor 빌드 성공, `HHV.Movement` 9개 통과. 언리얼에서 새로 내보낸 Wall·Step·Slope·PortableReplay 기록도 독립 실행 프로그램에서 모두 일치했다.
