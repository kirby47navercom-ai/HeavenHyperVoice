# 공통 에셋 연결

콘텐츠 브라우저에서 `/Game/Blueprints/DA_ProjectAssets`를 연다.
기존 `Blueprints` 폴더를 사용하며 새 Content 최상위 폴더는 만들지 않는다.

- **Levels**: 캐릭터 선택 화면, 기본 필드, 인스턴스 맵. Field Game Mode가 비어 있으면 대상 맵의 World Settings를 따른다.
- **Pokemon**: 종족 카탈로그와 야생/파트너 포켓몬 블루프린트.
- **UI**: 파티 화면, 로딩 화면, 타입별 아이콘.
- **Gacha**: 뽑기 화면, 기계, 5종 후보표, 시연 맵. 후보표의 서버 타입/DisplayOrder는 서버 정의와 일치시킨다.
- **Character**: 메시별 머티리얼 보정 및 체형 모프용 머티리얼 교체 참조.
- **Collision**: 맵 에셋과 공유 충돌 파일의 연결. 파일명은 `Content/MovementCollision` 기준 상대 경로이며 서버에도 같은 충돌 데이터를 배포한다.

프로젝트 설정 → Game → **공통 에셋 연결**에서 이 데이터 에셋 하나를 지정한다.
블루프린트에서는 **Get Project Assets** 노드로 연결표를 가져올 수 있다.
뽑기 컴포넌트의 Asset Overrides, 기존 위젯/컴포넌트의 개별 에셋 설정은 필요한 경우에만 사용한다.
개별 설정을 비우면 공통 연결표를 따른다.

현재 Field Level은 복원 후 이동한 `/Game/Fab/Environments/Goldenrod_R03/Maps/L_Goldenrod`,
Instance Level은 `/Game/InstanceMap/Plain/Plain`이다.
필드 충돌 파일은 `Environments/Goldenrod_R03/Maps/L_Goldenrod.hhvcollision`로 연결했고,
서버 기본 충돌 파일과 동일하다. 맵과 Goldenrod 메시·머티리얼·텍스처·블루프린트는
모두 `Fab/Environments`에 있으며 이전 에셋 경로는 이 위치로 리디렉트한다.

시작 맵·기본 게임모드·게임 인스턴스 클래스는 엔진 부팅에 쓰이므로
Project Settings → Maps & Modes에서 관리한다. 시작 맵을 바꾸면 연결표의 Frontend Level도 맞춘다.
공통 연결표는 Asset Manager의 AlwaysCook 대상으로 등록되어 있으며,
연결된 soft reference를 쿠커가 추적한다.

서버 주소나 인증 설정은 이 데이터 에셋에 넣지 않는다. 외부 C++ 서버는
언리얼 에셋을 직접 읽지 않으며 기존 종족·뽑기 타입 ID와 공유 충돌 데이터를 사용한다.

폴더 이동은 언리얼 콘텐츠 브라우저에서 진행하고 리디렉터와 참조를 정리한다.
현재 DefaultEngine.ini의 CoreRedirects는 이번 폴더 정리 이전 참조를 위한 호환 설정이다.
새 런타임 에셋을 추가할 때 이 목록에 경로를 추가하는 방식으로 사용하지 않는다.

`configure_project_assets.py`는 이번 이전을 위한 에디터 제작 스크립트다.
다시 실행하면 연결표를 재설정하므로, 이후 수동 편집한 연결표에 임의로 실행하지 않는다.
