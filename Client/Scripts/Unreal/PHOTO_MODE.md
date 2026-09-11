# 사진 모드

- `WBP_PhotoMode` (`/Game/UI/Options`)의 디자이너 트리에 프레임, 촬영/종료 버튼, 줌 슬라이더, 상태 문구가 저장된다. C++은 저장된 WBP를 화면에 띄우고 BindWidget으로 동작을 연결하며, 시각 요소의 트리나 배치를 생성하지 않는다.
- 메뉴의 `CameraCard`는 `UUEOptionsCardWidget` 기반이며 ActionId는 `Camera`다. 기존 메뉴 스크롤 그리드의 두 번째 줄에 배치한다.
- `T`: `IA_ToggleChat` / `Input.Action.ToggleChat`으로 채팅 전체 표시 전환. 작성 중인 메시지의 T는 문자로 입력한다. 숨겨도 채팅 수신/기록은 유지하며 Enter로 다시 열 수 있다.
- 사진 모드: WASD 걷기, 우클릭 드래그 시점, 휠/슬라이더/± 버튼 줌, Space/촬영 버튼 저장, ESC/나가기 종료.
- `IMC_PhotoMode`는 사진 모드에서만 우선순위 100으로 적용한다. 기존 이동/시점 IA는 재사용하고 점프/달리기/구르기/소환/기술/채팅 및 포탈 이동은 차단한다.
- 촬영 이미지는 Windows Known Folder API로 찾은 `사진/HeavenHyperVoice`에 PNG로 저장한다. OneDrive 등으로 이동된 사진 폴더도 따른다. 사진 폴더를 찾지 못하면 프로젝트 Screenshots 경로를 사용한다. 파일에는 카메라 UI와 게임 HUD가 들어가지 않는다.
- UI 표시 상태, 창 순서, FOV, HUD 상태는 종료 때 복원한다. 로컬 캐릭터는 촬영자의 뷰에서만 숨긴다.
- Goldenrod 경계 카메라 가중치가 0.8 이상인 곳에서는 진입을 막고, 촬영 중 접근하면 종료한다. 피격/사망 또는 Pawn 변경 시에도 종료한다.

## 이후 전투 / NPC 연결

전투 시스템은 전투 시작 시 `AUEPlayerController::SetInCombat(true)`, 완전히 종료될 때 `SetInCombat(false)`를 호출해야 한다. 시작 호출에서 즉시 사진 모드를 종료하고, 전투 중 재진입도 차단한다. 전투 시스템 자체는 아직 구현하지 않았다.

NPC 및 다른 상호작용은 실행 직전에 `CanInteractWithWorld()`를 확인해야 한다. 사진 모드에서는 false다. NPC 시스템 자체는 아직 구현하지 않았다.

## 바다

바다는 독립된 `Goldenrod_Ocean` 액터 하나다. 기존 수면과 사방 100000cm 확장 영역을 `SM_Goldenrod_Ocean` 평면 하나가 덮는다. 액터의 `StaticMeshComponent > Materials > Element 0`만 교체하면 전체 바다가 바뀐다. 도시는 수면을 제거한 `SM_Goldenrod_City_Land_R03`을 사용한다. 이전 `OceanExtension`과 네 개의 스트립은 제거했다. 콜리전·그림자·안개는 없다. `separate_goldenrod_ocean.py`가 이 구성을 저장한다.

에셋 작성 스크립트: `create_photo_mode.py`. 기존 WBP는 덮어쓰지 않는다. 게임 실행에는 스크립트나 SourceArt 폴더가 필요 없다.
