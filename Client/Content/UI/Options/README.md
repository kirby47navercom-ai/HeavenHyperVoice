# 플레이 중 통합 메뉴
화면 구성은 UMG 디자이너에 저장한다. C++에서는 위젯 배치를 생성하지 않는다.
통합 메뉴는 각 기능의 전용 UI로 들어가는 입구다. 설정 조작은 독립된 WBP에서만 한다.

- WBP_OptionsMenu → UUEOptionsMenuWidget: 전용 화면으로 이동하는 왼쪽 사이드 메뉴.
- WBP_OptionsCard → UUEOptionsCardWidget: 아이콘과 제목으로 구성한 공통 카드.
- WBP_OptionsHUD → UUEOptionsHUDWidget: 플레이 화면 왼쪽 위 메뉴 아이콘.
- WBP_GameSettings → UUEGameSettingsWidget: 화면 중앙의 독립된 설정 화면. 왼쪽 분류, 오른쪽 스크롤 페이지, 고정된 하단 버튼으로 구성한다.
- WBP_OptionsConfirm → UUEOptionsConfirmWidget: 캐릭터 선택/로그아웃 확인 화면.

BP_LoginPlayerController의 Options 분류에 위젯 클래스와 돌아갈 Frontend Level을 지정한다.
다른 플레이 컨트롤러를 추가할 때도 OptionsMenuClass, OptionsHUDClass, GameSettingsClass,
OptionsConfirmClass, FrontendLevel 기본값을 지정한다.

## 조작과 연결

- ESC: 메뉴 열기/닫기. 채팅 입력 중에는 채팅을 먼저 닫는다.
- 마우스: 기존 커서 표시 키 G를 누른 채 왼쪽 위 아이콘을 클릭한다.
- 계속하기: 메뉴를 닫고 플레이 입력을 복원한다.
- 설정: 통합 메뉴를 숨기고 중앙의 WBP_GameSettings를 연다. 화면/그래픽 분류를 클릭하면 오른쪽 페이지가 전환된다. 페이지를 바꿔도 편집 값은 유지하며, 적용 시 두 페이지의 값을 UGameUserSettings에 저장한다. 돌아가기/ESC는 적용하지 않은 변경을 버리고 통합 메뉴로 돌아간다.
- 캐릭터 선택: 별도 확인창을 연다. 확인하면 플레이 연결과 인스턴스 목적지를 정리하고 프런트엔드로 돌아간다. 현재 온라인 프로토콜은 선택 응답 뒤 로그인 연결을 닫으므로, 온라인에서는 재로그인 후 캐릭터를 선택한다. 비밀번호를 저장하거나 자동 재전송하지 않는다.
- 로그아웃: 별도 확인창을 연다. 확인하면 필드/채팅/로그인 연결, 서비스 티켓과 현재 세션을 정리하고 타이틀로 돌아간다. 취소/ESC는 세션을 유지하고 통합 메뉴로 돌아간다.

오른쪽 게임 화면은 덮거나 어둡게 하지 않는다. 메뉴를 조작하는 동안 로컬 플레이 입력만 차단하며 서버와 월드를 일시정지하지 않는다.
전용 화면에서 ESC/돌아가기/취소는 메뉴로 돌아간다. 메뉴에서 ESC/뒤로 버튼은 계속하기와 같다.

## 카드 확장

1. WBP_OptionsMenu의 Designer에서 MenuScroll > MenuGrid 아래 카드를 복제한다.
2. Grid Slot의 Row/Column을 지정한다. 기본 4열은 Column 0~3, 다음 줄은 Row + 1이다.
3. Details의 Options > Content에서 ActionId, Title, IconTexture를 바꾼다.
4. 기능별 C++ 부모와 WBP를 만들고 HandleOptionsAction에서 해당 전용 화면을 연다.
   실제 변경/실행은 전용 화면에서 사용자가 조작하거나 확인한 뒤 처리한다.

기본 ActionId는 Resume, Settings, CharacterSelect, Logout이다.
복제한 카드도 클릭 이벤트가 부모 메뉴로 전달되며, 목록이 길어지면 스크롤한다.
SectionText, DescriptionText 및 관련 데이터 필드는 제거했다.

## 디자인 편집

밝은 전체 스타일 변경은 취소하고 변경 전 디자인으로 복원했다.
새 공통 디자인은 사용자가 나중에 정의한다. Client/Content/UI/UI_STYLE.md를 따른다.

- 카드 크기/배치/버튼 스타일: WBP_OptionsCard.
- 카드 아이콘: 텍스트 글리프 대신 IconImage와 IconTexture를 사용한다.
- 재생, 톱니바퀴, 인물, 로그아웃, 메뉴, 뒤로, 발자국, 나침반: Icons/T_Options*.
- 설정 배치: WBP_GameSettings의 SettingsPages(WidgetSwitcher). DisplayScroll/GraphicsScroll 안의 VerticalBox에 설정 행을 넣는다. 통합 메뉴에는 설정 컨트롤이 없다.
- 화면 페이지: 프레임 제한, 수직 동기화.
- 그래픽 페이지: 전체 품질, 그림자, 텍스처, 효과, 표시 거리, 안티앨리어싱, 식생. 전체 품질 프리셋은 세부 값을 바꾸며, 개별 수정 시 사용자 지정으로 전환한다.
- 설정 확장: SettingsPages에 ScrollBox + VerticalBox 페이지를 추가하고, 왼쪽 분류 버튼과 C++의 SelectCategory에 페이지 인덱스를 연결한다. 행은 SizeBox로 높이를 정하고 VerticalBox에 추가한다. 돌아가기/적용은 페이지 바깥에 고정한다.
- 설정 창 정렬: 전체 화면 앵커의 ResponsiveScale 아래 ScreenSize(1280×820)를 수평/수직 중앙에 정렬한다. 뷰포트에 맞춰 비율을 유지하며 크기가 바뀐다.
- 확인창 배치: WBP_OptionsConfirm. 통합 메뉴와 전용 화면은 동시에 표시하지 않는다.
- 상단 닉네임: PlayerNameText가 현재 세션 닉네임을 표시한다.
- 슬라이드: EntranceDuration, EntranceOffset; 호버: HoverLift, ResponseSpeed.

## 제작 도구

Client/Scripts/Unreal/create_options_menu.py는 편집기에서 처음 에셋을 만드는 도구다.
기존 에셋이 있으면 덮어쓰지 않고 중단한다. 이후 수정은 UMG 디자이너에서 한다.
render_options_icons.py는 동봉한 Font Awesome 글꼴로 PNG 아이콘 원본을 만든다(Pillow 필요).
게임 실행에는 제작 스크립트, Python, MCP 플러그인이 필요하지 않다.

style_project_ui.py와 style_options_journey.py는 전체 스타일 변경 취소에 따라 실행을 중지했다.

아이콘 원본 및 OFL 라이선스: Client/SourceArt/UI/Options.
일러스트 생성 기록: 같은 폴더의 ART_SOURCE.md.

로컬 테스트 전용 변경은 기존 작업 트리에 보존한다. 이 문서는 해당 변경의 커밋을 허용하지 않는다.
