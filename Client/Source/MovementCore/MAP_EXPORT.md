# 맵의 공통 이동 충돌 저장

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

클라이언트의 `Content/MovementCollision` 폴더는 패키징 시 추가 데이터(UFS)로 포함되도록 설정돼 있다. 서버를 다른 컴퓨터로 배포할 때는 `Server/maps/collision` 폴더도 함께 배포한다. 기존 `.hhvmap` 파일은 별도 용도이며 덮어쓰지 않는다.

## 무엇을 추출하는가

- 컴포넌트의 실제 Collision Presets가 **정확히 `ServerGround` 또는 `ServerWall`**인 경우만 포함한다. 부모 블루프린트에서 상속받은 프리셋도 실제 컴포넌트에 적용돼 있으면 포함된다.
- 이름, 액터 라벨, 태그만 같은 경우는 포함하지 않는다. 다른 프리셋의 자식 컴포넌트까지 부모에 붙어 있다는 이유로 포함하지 않는다.
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

이 작업은 공통 파일 생성·배포와 클라이언트 파일 로드를 연결한다. 기존 서버의 이동 루프·네트워크·내비게이션 코드는 변경하지 않는다. 실제 서버 이동에 적용할 때는 공통 코어 호출을 그 루프에 연결해야 한다. 단위 cm/Z-up, 캡슐 중심 좌표이며 기존 서버의 X/Y +25600 좌표 변환은 외부 어댑터에서 맞춰야 한다.

## 자동화

에디터 Python에서도 `unreal.HHVCoreCollisionExportLibrary.export_current_map_collision()`을 호출할 수 있다. 버튼과 같은 저장 경로 및 추출 기준을 사용한다. `save_map=False`는 이미 저장한 맵을 검사/일괄 추출할 때만 사용한다.

## 현재 생성된 데이터

2026-09-12 `PlayerTestLevel`에서 912개 삼각형을 추출해 양쪽 경로에 저장했다. 바이트 일치와 독립 C++ 로더의 파일 읽기를 검증했다. `Filed`의 이전 추출 실패는 Landscape 표시 컴포넌트의 NoCollision을 먼저 검사했던 버그 때문이다. Landscape 액터의 (Instance) Collision Presets를 사용해야 하며 RootComponent에 설정할 필요는 없다. 이 검사를 수정하고 빌드 및 실제 맵 재추출을 완료했다. `Landscape_Cheongsando`의 Instance 프리셋 `ServerGround`를 확인했으며 삼각형 164,834개를 추출했다. 클라이언트/서버 파일 바이트 일치와 순수 C++ 로더 검증을 통과했다. `Stage/Filed.hhvcollision` 파일이 양쪽 배포 폴더에 생성돼 있다.

Editor 빌드 및 `HHV.Movement` 9개 테스트 통과. 프리셋 필터, 미지원 선택 형상의 실패 처리, PIE 이름에서 저장 경로를 찾는 동작, 저장된 파일 로드를 포함한다.

랜드스케이프 재현 검사는 `Client/Source/HeavenHyperVoiceEditor/Tests/VerifyLandscapeCollisionExport.py`에 보관했다. 에디터 Python commandlet에서 실행하면 표시 컴포넌트가 NoCollision인 상태에서도 Instance 프리셋으로 추출되는지, 양쪽 파일이 일치하는지 검사한다. 맵 에셋을 수정하지 않으며 공통 충돌 파일은 갱신한다.
