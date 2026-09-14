# 맵의 공통 이동 충돌과 서버 NavMesh

## 에디터에서 저장하기

1. 수정할 맵을 연다. 플레이 중이라면 먼저 종료한다.
2. 내보낼 컴포넌트를 선택하고 Details → Collision → Collision Presets를 바닥은 `ServerGround`, 벽은 `ServerWall`로 지정한다. Landscape는 Landscape 액터의 Collision Presets를 지정한다.
3. 에디터 상단 **도구(Tools) → 하이퍼보이스 → 공통 이동 충돌 저장**을 누른다. 하이퍼보이스는 메뉴 안의 구역 이름이다.
4. 완료 알림에서 추출된 삼각형 수와 두 저장 경로를 확인한다.
5. 플레이를 시작한다. `CoreMovement → Collision File`을 비워두면 해당 맵의 저장 파일을 자동으로 읽는다. 이전에 파일 경로를 직접 지정했다면 비워서 자동 경로를 사용한다.

버튼은 변경된 맵 패키지를 저장한 후 현재 로드된 장면에서 충돌 파일을 추출한다. 메시나 블루프린트 에셋 자체를 수정했다면 먼저 해당 에셋도 저장한다. 새 맵은 프로젝트 Content 아래에 저장해야 한다. 서브레벨/월드 파티션을 사용하면 추출할 지형을 모두 로드한 상태에서 저장한다. 내보낸 뒤 맵을 수정하면 버튼을 다시 눌러 갱신한다. PIE는 파일을 다시 만들지 않으므로 새로 플레이를 시작해야 갱신된 파일을 읽는다.

## 저장 위치

맵 `/Game/Level/PlayerTestLevel`의 경우:

- 클라이언트: `Client/Content/MovementCollision/Level/PlayerTestLevel.hhvcollision`
- 서버: `Server/maps/collision/Level/PlayerTestLevel.hhvcollision`

맵 `/Game/Stage/Filed`의 경우에는 각각 `Stage/Filed.hhvcollision`로 저장된다. 폴더까지 유지하므로 서로 다른 폴더에 같은 이름의 맵이 있어도 파일이 겹치지 않는다. PIE의 임시 이름 접두사는 경로에서 제거한다.

두 파일은 같은 형상과 해시를 담은 동일한 바이트다. 양쪽 임시 파일과 기존 파일 백업을 준비한 뒤 교체하며, 저장 실패 시 기존 파일 복원을 시도한다. 폴더 쓰기 권한이나 읽기 전용 파일 때문에 실패하면 알림과 Output Log를 확인한다.

클라이언트의 `Content/MovementCollision` 폴더는 패키징 시 추가 데이터(UFS)로 포함되도록 설정돼 있다. 서버를 다른 컴퓨터로 배포할 때는 `Server/maps/collision` 폴더도 함께 배포한다. 서버는 이 파일에서 NavMesh도 생성한다. 이전 `.hhvmap` 파일이나 별도의 내비게이션 추출기는 사용하지 않는다.

## 무엇을 추출하는가

- 컴포넌트의 Collision Presets 또는 Component Tags에 `ServerGround` / `ServerWall`이 있으면 포함한다. Goldenrod의 네이티브 박스는 태그를 사용한다.
- 이름이나 액터 라벨만 같은 경우는 포함하지 않는다. 부모에 붙어 있다는 이유로 다른 자식 컴포넌트를 포함하지 않는다.
- 충돌이 꺼진 액터/컴포넌트, Pawn, 디버그 표시 컴포넌트는 제외한다.
- Box, StaticMesh 및 인스턴스, Landscape를 지원한다. StaticMesh는 현재 **시각 LOD0 삼각형**을 사용한다. 별도 Simple Collision과 모양이 다를 수 있다. Landscape는 단순 충돌 높이 및 구멍을 사용한다.
- 선택된 프리셋에 지원하지 않는 형상이 있거나 추출 대상이 없으면 오류를 내고 기존 배포 파일을 유지한다.
- 바닥/벽 이름은 포함 대상을 고르는 기준이다. 실제 걸을 수 있는지는 공통 코어의 경사 제한으로 판단한다.
- 정적인 스냅샷이다. 움직이는 플랫폼이나 다른 캐릭터와의 충돌은 아직 동기화하지 않는다.

## 서버에서 읽기

서버에 언리얼은 필요 없다. `MovementCore` 폴더의 표준 C++17 코드를 연결하고 동일 파일을 `TriangleWorld::load`로 읽은 뒤, 엔티티별 `State`에 `simulate`를 호출한다. 데이터 경로는 서버 실행 위치에 맞춰 지정한다.

```cpp
hhv::movement::TriangleWorld collision;
std::ifstream file("maps/collision/Level/PlayerTestLevel.hhvcollision");
if (!collision.load(file)) {
    // 시작 중단 또는 맵 로드 실패 처리
}
// simulate(entityState, input, config, collision);
```

필드·인스턴스 서버는 이 파일을 직접 읽어 플레이어 검증, 야생 이동, 파트너 이동에 사용한다. 서버 어댑터가 저장 좌표와 코어 좌표를 변환한다. 입장 시 클라이언트 파일 해시가 다르면 연결을 중단하므로 양쪽 파일을 함께 배포해야 한다.

## 서버 길찾기

`Server/Movement/src/NavigationMesh.cpp`가 맵 로드 때 **Recast로 NavMesh를 생성**하고, 이후 **Detour로 경로를 조회**한다. 바닥과 벽을 포함한 동일한 충돌 삼각형을 사용한다. 서버 실행에 Unreal 엔진이나 Unreal의 NavMesh 데이터는 필요 없다.

1. `ServerGround` / `ServerWall` 프리셋 또는 태그에 해당하는 형상을 기존 저장 버튼으로 추출한다.
2. 서버가 `.hhvcollision`을 읽는다. Recast가 경사, 단차, 머리 위 공간과 캡슐 반지름을 반영해 걸을 수 있는 영역을 만든다. 프리셋 이름 자체가 통행 가능 여부를 결정하지는 않는다.
3. 야생 포켓몬 추격과 파트너 따라오기는 연결된 폴리곤에서 경로를 찾는다. 기존의 자체 격자 A* 및 경로 검사 중 공통 코어로 미리 걷게 하던 반복 시뮬레이션은 제거했다. Detour 내부의 폴리곤 그래프 탐색은 계속 사용한다.
4. 경로는 이동 방향을 정하는 데 사용한다. 실제 좌표와 높이, 충돌, 중력은 계속 공통 이동 코어가 계산한다. NavMesh 높이를 매 틱 캐릭터 좌표에 덮어쓰지 않는다.

NavMesh는 맵 객체별로 한 번 만들고 방들에서 공유한다. 틱이나 경로 요청마다 다시 생성하지 않는다. 조회마다 별도의 Detour 검색 상태를 사용하므로 서로 다른 방이 동시에 경로를 요청해도 검색 상태를 공유하지 않는다. 재탐색 간격과 기존 경로 재사용은 기존 AI 정책을 따른다. 배회는 기존에 정한 **랜덤 시간 동안 랜덤 방향 이동 → 랜덤 시간 대기**를 유지하며 길찾기를 호출하지 않는다.

기본 캡슐은 반지름 34cm, 반높이 88cm, 최대 단차 45cm, 경사 44도다. NavMesh는 수평 20cm / 높이 2cm 셀, 한 변 51.2m 타일로 만든다. 캡슐 규격이 다른 요청은 기존 NavMesh를 그대로 사용하지 않으며, 다른 크기의 개체를 지원하려면 해당 규격의 NavMesh를 준비해야 한다. 작은 통로나 경계는 셀 해상도 때문에 공통 코어보다 보수적으로 제외될 수 있다.

층이 겹친 지형은 캡슐 중심을 발바닥 높이로 변환해 같은 높이의 폴리곤을 찾는다. 직선 검사는 수평 통과 여부와 도착 높이를 함께 확인한다. 연결이 끊겼거나 검색 한도를 넘겨 목적지까지 도달하지 못한 부분 경로는 성공으로 처리하지 않는다. 모서리에는 연결이 검증되는 범위 안에서 최대 10cm 여유를 둬 실제 이동 관성으로 경계를 스치는 경우를 줄인다.

Recast/Detour는 `Server/vcpkg.json`에 등록돼 있으며 서버 빌드 시 설치된다. 시작 로그에 NavMesh 생성 시작과 완료한 폴리곤 수가 표시된다. 큰 맵은 시작 시 생성 시간이 추가되므로 서버의 준비 완료 로그 이후 접속한다. 현재 디스크 캐시는 사용하지 않아 서버를 다시 시작하면 재생성한다. 맵 변경 후에는 충돌 파일을 다시 저장하고 서버를 재시작한다.

검증은 서버의 `FieldMovementTests`, `WildPokemonAITests`로 실행한다. Filed/Goldenrod 실제 파일, 겹친 층, 벽 우회, 타일 사이 연결, 단차, 동시 조회, 파트너 모서리 이동, 실패한 맵 재로드를 포함한다. `FieldMovementTests`는 실제 맵의 로드·생성 시간과 반복 경로 조회 평균 시간도 출력한다. 이 값은 테스트 경로의 비용이며 전체 서버 틱 지연이나 네트워크 지연을 나타내지는 않는다.

2026-09-14 로컬 Windows 검사에서 Filed의 삼각형 164,834개로 NavMesh 폴리곤 16,986개가 생성됐다. 로드·생성에는 Release 약 45초, Debug 약 254초가 걸렸다. 생성 완료 뒤 같은 10m 경로를 200번 조회한 평균은 Release 약 0.005ms, Debug 약 0.020ms였다. 서버를 시작할 때마다 생성하므로 특히 Debug에서는 `InstanceServer listening` 로그까지 수 분을 기다려야 할 수 있다.

기존 HEAD `d7477156`과 새 코드를 같은 Release 컴파일 설정 및 동일한 시작점·목표점으로 별도 비교한 200회 측정에서는 기존 평균 4.841ms, NavMesh 평균 0.00355ms였고 양쪽 모두 경로를 찾았다. 이 비교는 Filed 시작점에서 X 방향 10m 경로 한 개에 대한 결과이며, 모든 지형이나 전체 서버 부하에 동일한 개선 비율을 보장하지 않는다.

## 자동화

에디터 Python에서도 `unreal.HHVCoreCollisionExportLibrary.export_current_map_collision()`을 호출할 수 있다. 버튼과 같은 저장 경로 및 추출 기준을 사용한다. `save_map=False`는 이미 저장한 맵을 검사/일괄 추출할 때만 사용한다.

에디터를 종료한 상태에서 `UnrealEditor-Cmd.exe <프로젝트 절대 경로> -run=HHVCoreCollisionExport -Map=/Game/Level/PlayerTestLevel -unattended -nop4 -NullRHI`로도 같은 경로에 저장할 수 있다.

Goldenrod는 `/Game/Environments/Goldenrod_R03/Maps/L_Goldenrod`를 지정한다. 4,728개 삼각형을 양쪽에 동일하게 저장했다.

## 현재 생성된 데이터

2026-09-12 `PlayerTestLevel`에서 912개 삼각형을 추출해 양쪽 경로에 저장했다. 바이트 일치와 독립 C++ 로더의 파일 읽기를 검증했다. `Filed`의 이전 추출 실패는 Landscape 표시 컴포넌트의 NoCollision을 먼저 검사했던 버그 때문이다. Landscape 액터의 (Instance) Collision Presets를 사용해야 하며 RootComponent에 설정할 필요는 없다. 이 검사를 수정하고 빌드 및 실제 맵 재추출을 완료했다. `Landscape_Cheongsando`의 Instance 프리셋 `ServerGround`를 확인했으며 삼각형 164,834개를 추출했다. 클라이언트/서버 파일 바이트 일치와 순수 C++ 로더 검증을 통과했다. `Stage/Filed.hhvcollision` 파일이 양쪽 배포 폴더에 생성돼 있다.

Editor 빌드 및 `HHV.Movement` 9개 테스트 통과. 프리셋 필터, 미지원 선택 형상의 실패 처리, PIE 이름에서 저장 경로를 찾는 동작, 저장된 파일 로드를 포함한다.

랜드스케이프 재현 검사는 `Client/Source/HeavenHyperVoiceEditor/Tests/VerifyLandscapeCollisionExport.py`에 보관했다. 에디터 Python commandlet에서 실행하면 표시 컴포넌트가 NoCollision인 상태에서도 Instance 프리셋으로 추출되는지, 양쪽 파일이 일치하는지 검사한다. 맵 에셋을 수정하지 않으며 공통 충돌 파일은 갱신한다.
