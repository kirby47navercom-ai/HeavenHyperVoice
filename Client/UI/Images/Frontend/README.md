# 프론트엔드 UI 리소스

타이틀 → 서버 접속 → 로그인·회원가입 화면용 리소스다. 배치는 WBP 에서 직접 한다.
기준 해상도는 1920×1080 이고, 아래 좌표와 크기는 모두 그 기준이다.
글꼴 크기는 시안의 픽셀 값이다. UMG 글꼴 Size 는 포인트라 **0.75 배**로 넣는다 (예: 28px → 21).

| 무엇 | 어디 |
|---|---|
| 원본 PNG | `Client/UI/Images/Frontend/` |
| PNG 생성 스크립트 (TitleLogo, Background 제외) | `Client/Scripts/Art/Frontend/build_frontend_ui_textures.py` |
| 로고 조각 자르기 스크립트 | `Client/Scripts/Art/Frontend/crop_logo_layers.py` |
| 언리얼 임포트 스크립트 | `Client/Scripts/Unreal/import_frontend_ui_textures.py`, `import_frontend_fonts.py` |
| 텍스처 에셋 | `/Game/Frontend/UI/Textures/T_Frontend_<이름>` |

색이나 모양을 바꾸려면 생성 스크립트의 상수를 고치고 두 스크립트를 차례로 다시 돌린다.
`TitleLogo.png` 는 `output/imagegen/heaven-hypervoice-title-v4.png`, `Background_SpearPillar.png` 는
`output/imagegen/sky-background/spear-pillar-clouds.png` 를 복사한 것이다.

## 색

| 이름 | 값 | 쓰는 곳 |
|---|---|---|
| 로고 외곽 파랑 | `#0B2A86` | 패널 테두리, 제목·본문 글자, 선택된 탭 |
| 패널 흰색 | `#F5F9FF` | 패널 바탕 |
| 입력칸 | `#E4EEF9` | 입력칸 바탕 |
| 신호 청록 | `#2BA8E8` | 포커스, 마이크, 키 표시, 파형 |
| 로고 노랑 | `#FDD400` | 주 버튼, 탭 눈금, 패드 A |
| 별 금색 | `#E6B64A` | 패널 머리줄 별 (장식만) |
| 라벨 | `#1B2B5E` | 입력칸 라벨, 뒤로 링크 |
| 안내 글자 | `#45557F` | 설명 문구, 안내 상태 |
| 머리줄 글자 | `#DCE8F6` | 패널 머리줄 |
| 서버 링크 | `#8ED6FB` | 로그인 머리줄 서버 주소 |
| 탭 hover | `#DCE8F6` | 선택 안 된 탭에 마우스 올림 |

상태 색: 안내 `#45557F` · 연결 중 `#1A86C9` · 오류 `#C2372E` · 성공 `#1C7F4E`

## 글꼴

| 역할 | 글꼴 | 굵기 |
|---|---|---|
| 제목, 탭, 버튼 글자 | Do Hyeon | Regular |
| 라벨, 안내, 상태 문구 | IBM Plex Sans KR | Regular, Medium, SemiBold, Bold |
| 서버 주소, 아이디, 키 표시 | IBM Plex Mono | Regular, Medium, SemiBold |

셋 다 OFL 이다. 원본 TTF 와 `OFL.txt` 는 `Client/UI/Fonts/<묶음>/` 에 있다 (Google Fonts 저장소에서 받음).

UMG 글꼴 칸에는 아래 Font 에셋을 고르고, Typeface 에서 굵기를 고른다.

| Font 에셋 | Typeface |
|---|---|
| `/Game/UI/Fonts/DoHyeon/F_DoHyeon` | Regular |
| `/Game/UI/Fonts/IBMPlexSansKR/F_IBMPlexSansKR` | Regular, Medium, SemiBold, Bold |
| `/Game/UI/Fonts/IBMPlexMono/F_IBMPlexMono` | Regular, Medium, SemiBold |

IBM Plex Mono 에는 한글이 없다. 서버 주소·아이디처럼 영문과 숫자만 들어가는 칸에만 쓴다.
글꼴을 다시 임포트할 때는 에디터 안에서 `import_frontend_fonts.py` 를 실행한다 (커맨드렛에서는 멈춘다).

## 텍스처

9-slice 는 Brush 의 **Draw As = Box**, **Margin** 은 텍스처 크기 대비 비율이다.
흰색 리소스는 Brush **Tint** 로 색을 입힌다.

| 텍스처 | 크기 | Draw As | Margin (L, T, R, B) | 용도 |
|---|---|---|---|---|
| Panel | 160×160 | Box | 0.25, 0.25, 0.25, 0.25 | 단말기 패널 |
| PanelShadow | 256×256 | Box | 0.35, 0.35, 0.35, 0.35 | 패널 그림자. 패널보다 사방 40px 크게, 아래로 28px 내림 |
| PanelHeader | 128×58 | Box | 0.29, 0, 0, 0 | 패널 머리줄. 높이 58 고정, 패널 테두리 5px 안쪽에 붙임 |
| Input_Normal / Hovered / Focused / Error | 96×96 | Box | 0.21 전부 | 입력칸 |
| ButtonPrimary_Normal / Hovered / Pressed / Disabled | 128×96 | Box | 0.19, 0.27, 0.19, 0.27 | 주 버튼 |
| PromptBar | 96×96 | Box | 0.23 전부 | 타이틀 시작 안내 바 |
| TabGroup_Frame | 64×64 | Box | 0.1 전부 | 로그인·회원가입 탭 묶음 테두리(Tint `#0B2A86`), 사각 포커스 링(Tint `#2BA8E8`) |
| FocusRing | 128×96 | Box | 0.24, 0.3, 0.24, 0.3 | 주 버튼 포커스 링. 흰색, Tint `#2BA8E8` |
| Chip_Frame | 48×48 | Box | 0.1 전부 | 키 표시(Tint `#2BA8E8`), 빠른 입력 칩(Tint `#0B2A86`) |
| TabTick | 48×10 | Image | — | 선택된 탭 왼쪽 아래 노란 눈금 |
| Icon_Star | 64×64 | Image | — | 머리줄 별. 20×20, Tint `#E6B64A` |
| Icon_Mic | 128×128 | Image | — | 시작 안내 마이크. 50×50, Tint `#2BA8E8` |
| Icon_StatusInfo / Error / Check | 64×64 | Image | — | 상태 아이콘. 30×30, Tint 는 상태 색 |
| Sparkle | 128×128 | Image | — | 로고 반짝이. Tint `#FFFBE6` |
| Streak | 512×24 | Image | — | 바람 줄기. 폭 300~380, 높이 24, 각도 −5° |
| Background_SpearPillar | 1672×941 | Image | — | 배경 그림. 화면 전체로 채움. 아래 카메라 값 참고 |
| TitleLogo | 2172×724 | Image | — | 타이틀 로고 (등장이 끝난 뒤 보이는 원본) |
| TitleLogo_Flash | 2236×788 | Image | — | 등장 끝의 번쩍임. 로고 모양 흰 실루엣 + 글로우. 로고보다 사방 32px 큼 |

버튼은 Button 의 Style 에서 Normal / Hovered / Pressed / Disabled 에 각각 넣는다.
입력칸은 EditableTextBox Style 의 Background Image Normal / Hovered / Focused 에 넣고,
오류는 아래 `OnStatusChanged` 에서 Normal 브러시를 Input_Error 로 바꿨다가 되돌린다.

## C++ 에서 바뀐 것

- `WBP_Login`, `WBP_ServerAddress` 에 **OnStatusChanged(Kind)** 이벤트가 생겼다. 상태 문구가 바뀔 때마다 온다.
  Kind 는 `Info` / `Pending` / `Error` / `Success`. 상태 아이콘·색·파형 표시는 여기서 고른다.
- `WBP_Login` 에 선택 바인딩 **ServerAddressText** (TextBlock) 가 생겼다. 이 이름으로 두면 지금 접속할 서버 주소가 들어간다.
  로그인 패널 머리줄의 `BackButton` 안에 두면 “서버 변경” 링크가 된다.
- **DuplicateCheckButton 바인딩을 뺐다.** 버튼은 WBP 에 남아 있지만 아무 동작도 안 하니 지운다.
  대신 회원가입 화면에서 아이디를 칠 때마다 형식 안내가 상태 문구로 나온다 (글자 문제는 Error, 길이는 Info, 통과는 Success).
- `DuplicateCheckRequiredStatusText` 기본값 칸이 없어졌다.
- `WBP_Login` 에 선택 바인딩 **LoginTabButton** (Button) 과 **OnScreenModeChanged(Mode)** 이벤트가 생겼다.
  LoginTabButton 을 두면 로그인·회원가입 탭이 항상 둘 다 보이고, BackButton 은 모드와 상관없이 서버 화면으로 간다.
  두지 않으면 예전처럼 회원가입 중엔 RegisterTabButton 이 숨고 BackButton 이 로그인으로 돌아간다.
  선택된 탭 모양은 OnScreenModeChanged 에서 고른다.
- `SubtitleBlock` 이 선택 바인딩이 됐다. 탭 배치에서는 지워도 된다.
- `SetStatusMessage` 에 Kind 인자가 붙었다 (기본값 Info). 기존 블루프린트 호출은 그대로 된다.

## 배치 기준

### 배경과 카메라

배경 그림 한 장을 확대·이동해서 타이틀 장면과 전체 장면을 만든다.
값은 `output/imagegen/sky-background/camera-framing.json` 에서 계산했다.

| 장면 | 보이는 영역 (그림 기준 비율) | 배경 Image Render Transform (Pivot 0.5, 0.5) | TitleLogo |
|---|---|---|---|
| 타이틀 | x 1.5%, y 18%, 가로·세로 55% (구름만) | Scale 1.8182, Translation (733, 88) | (210, 140) 1500×500 |
| 서버·로그인·회원가입 | 전체 (오른쪽에 창기둥) | Scale 1, Translation (0, 0) | Render Transform Pivot (0, 0), Scale 0.4267, Translation (1025, −70) → 창기둥 위 하늘에 640 폭 |

배경 Image 는 화면 전체(1920×1080)를 채우게 둔다. Translation 은 그 크기 기준이다.

**해상도 주의:** 그림이 1672×941 이라 타이틀 장면은 약 920px 폭을 1920 으로 늘려 보여 준다 (2.1배, 흐림).
전체 장면은 1.15배라 괜찮다. 타이틀까지 선명하려면 가로 3500px 이상 원본이 필요하다.

### 타이틀 → 서버 줌아웃

| 무엇 | 값 |
|---|---|
| 배경 | 타이틀 Transform → Scale 1 / Translation 0, 1.6초, ease in-out |
| 로고 | 같은 1.6초 동안 타이틀 위치 → 창기둥 위 (위 표) |
| 반짝이·바람 줄기·시작 안내 | 누르는 순간 0.15초 페이드아웃 |
| 서버 패널 | 0.95초 뒤 시작, 왼쪽 60px 에서 제자리로 0.5초, 불투명도 0 → 1 |
| 되돌아갈 때 (서버 → 타이틀) | 같은 애니메이션을 거꾸로 |

이 줌은 아래 “C++ 연결: 등장 건너뛰기와 줌아웃” 의 `ZoomOut` 애니메이션으로 만든다.

### 타이틀

- 반짝이: 큰 것 (469, 507) 64×64, 작은 것 (824, 477) 34×34. 로고 안 금색 별 자리다.
- 시작 안내: PromptBar, 가로 중앙, y 872, 높이 86, 안쪽 여백 좌 22 · 상하 18 · 우 34, 요소 간격 20.
  - Icon_Mic 50 → Chip_Frame 안에 “Enter” (IBM Plex Mono SemiBold 20, 흰색) → “또는” → 노란 원 38 안에 “A” (Do Hyeon 26) → “눌러 시작” (IBM Plex Sans KR Medium 27, `#F5F9FF`) → 파형 막대 5개 (5×30, 간격 5, `#2BA8E8`)
- `ContinueButton` 을 이 바 전체에 씌우면 클릭으로도 넘어간다.

### 패널 (서버 · 로그인 · 회원가입)

- PanelShadow → Panel. 패널 위치 (96, 150), 폭 650. 높이는 내용에 맞춘다.
- 머리줄: PanelHeader 높이 58, 좌 여백 46 · 우 28.
  - 왼쪽: 제목 (IBM Plex Mono Medium 17, `#DCE8F6`, 자간 넓게) + Icon_Star
  - 오른쪽: 서버 화면은 “1 / 2”, 로그인·회원가입은 BackButton 안에 초록 점 (10×10, `#4FD08F`) + ServerAddressText + “ · 변경” (IBM Plex Mono Medium 17, `#8ED6FB`)
- 본문 여백: 위 38 · 좌우 50 · 아래 46.

| 요소 | 크기·글꼴 | 간격 |
|---|---|---|
| 탭 묶음 (로그인·회원가입) | 높이 68 (테두리 포함), 두 칸 반반. Do Hyeon 30 | 아래 32 |
| 선택된 탭 | 바탕 `#0B2A86`, 글자 `#F5F9FF`, TabTick 을 왼쪽 26 · 아래 11 | |
| 제목 (서버 화면) | Do Hyeon 52 | 아래 10 |
| 설명 | IBM Plex Sans KR 21, `#45557F` | 아래 30 |
| 라벨 | IBM Plex Sans KR SemiBold 19, `#1B2B5E` | 아래 9 |
| 입력칸 | 높이 72, 안쪽 좌우 24, IBM Plex Mono Medium 28 | 아래 24 |
| 빠른 입력 칩 (서버 화면) | Chip_Frame, IBM Plex Mono Medium 18, 안쪽 5 / 14 | |
| 상태 줄 | 아이콘 30 + 간격 14 + IBM Plex Sans KR 20, 최소 높이 36 | 아래 26 |
| 주 버튼 | 높이 84 (테두리 포함), Do Hyeon 36, 글자 `#0B2A86`, 뒤에 “▶” | |
| 뒤로 링크 (서버 화면) | “← 타이틀로”, IBM Plex Sans KR SemiBold 21 | 주 버튼과 한 줄, 양 끝 정렬 |

- 서버 화면 제목 “어느 서버에 접속할까요?”, 칩 “로컬 127.0.0.1”, 버튼 “접속”.
- 회원가입 버튼 글자 “계정 만들기”. 회원가입으로 바꿔도 패널 위치는 그대로 두고 아래로만 늘어난다.
- 탭은 `LoginTabButton` 과 `RegisterTabButton` 두 개, 머리줄 서버 링크는 `BackButton` 이다.

## 상태

| 요소 | 기본 | 마우스 올림 | 누름 | 포커스 | 잠김 · 오류 |
|---|---|---|---|---|---|
| 주 버튼 | Normal | Hovered, 3px 위로 | Pressed, 2px 아래 | 청록 외곽선 | 연결 중 Disabled |
| 입력칸 | Input_Normal | Input_Hovered | — | Input_Focused | Input_Error |
| 탭 | 투명 | `#DCE8F6` 채움 | — | 청록 외곽선 | 선택: 파랑 채움 + TabTick |
| 링크 | 글자만 | 밑줄 2px | — | 청록 외곽선 | — |

OnStatusChanged 에서 고를 것:

| Kind | 아이콘 | 색 | 그 밖에 |
|---|---|---|---|
| Info | Icon_StatusInfo | `#45557F` | |
| Pending | 아이콘 자리에 파형 막대 5개 | `#1A86C9` | 주 버튼은 C++ 이 잠근다 |
| Error | Icon_StatusError | `#C2372E` | 해당 입력칸 Input_Error |
| Success | Icon_StatusCheck | `#1C7F4E` | |

## 애니메이션

| 대상 | 움직임 | 주기 |
|---|---|---|
| TitleLogo | 위로 8px 떴다 내려옴, ease-in-out (줌 이동과 따로, 안쪽 한 겹에) | 7초 반복 |
| 배경 | 크기 1 ↔ 1.035 천천히 (줌 Transform 과 따로, 안쪽 한 겹에) | 26초 왕복 |
| Sparkle | 크기 0.25 → 1 (45° 회전, 불투명) → 0.5 → 0.25 (투명) | 3.2초, 두 개 1.6초 엇갈림 |
| Streak 왼쪽 2개 | (−20, 560) 폭 380 · (40, 604) 폭 300. 왼쪽 220px 에서 제자리로 오며 나타났다 사라짐 | 5초, 2.4초 엇갈림 |
| Streak 오른쪽 2개 | (1640, 380) 폭 320 · (1600, 566) 폭 360. 제자리에서 오른쪽 220px 로 | 5초, 1.2초·3.6초 엇갈림 |
| 파형 막대 | 세로 크기 0.3 ↔ 1 | 1.1초, 막대마다 0.2초 엇갈림 |
| 패널 등장 | 왼쪽 40px 에서 제자리로, 불투명도 0 → 1 | 0.18초 한 번 |

시안에 있던 “로고 위를 빛이 훑고 지나가는” 효과는 머티리얼이 필요해서 리소스에 넣지 않았다.

## 로고 조각과 등장 애니메이션

`TitleLogo` 한 장 대신 조각 12장을 겹쳐 쓰면 조각마다 따로 움직일 수 있다.
원본은 `output/imagegen/heaven-hypervoice-layers-refined` 이고, 빈 여백을 잘라 `Logo_*.png` 로 넣었다.
잘라낸 조각을 아래 위치에 겹치면 원본 합성본(`recombined.png`)과 픽셀 차이 1 이내로 같다.

### 배치

로고 묶음은 Canvas Panel 하나(타이틀에서 1500×500, 위치 (210, 140))로 만들고, 그 안에 아래 순서대로 Image 를 쌓는다 (위가 아래 레이어).
위치·크기는 그 1500×500 캔버스 기준이다. 각 Image 의 Render Transform Pivot 은 (0.5, 0.5).

| 순서 | 텍스처 | 위치 (x, y) | 크기 |
|---|---|---|---|
| 1 | Logo_Wave (음파) | 0, 147.8 | 1500 × 351.5 |
| 2 | Logo_Glyph1_Po (포) | 103.6, 29.0 | 346.7 × 303.2 |
| 3 | Logo_Glyph2_Ket (켓) | 384.7, 22.1 | 249.3 × 288.0 |
| 4 | Logo_Glyph3_Mon (몬) | 602.9, 24.2 | 268.0 × 273.5 |
| 5 | Logo_Glyph4_Seu (스) | 832.2, 38.0 | 258.3 × 268.0 |
| 6 | Logo_Glyph5_Teo (터) | 1037.3, 63.5 | 344.6 × 271.4 |
| 7 | Logo_Subtitle (천계의 하이퍼보이스) | 279.7, 304.6 | 1149.2 × 176.8 |
| 8 | Logo_FeatherLeft | 65.6, 178.2 | 102.2 × 136.0 |
| 9 | Logo_FeatherRight | 1307.3, 268.6 | 118.1 × 131.2 |
| 10 | Logo_Shout (울려라!) | 29.0, 272.8 | 344.6 × 191.3 |
| 11 | Logo_StarLeft | 247.2, 359.8 | 85.6 × 96.0 |
| 12 | Logo_StarRight | 593.9, 298.3 | 85.6 × 102.2 |

원본 2172×724 캔버스 좌표가 필요하면 위 값을 0.6906 으로 나눈다 (`crop_logo_layers.py` 가 출력한다).

### 등장 (WBP_Title 의 `LogoIntro` 애니메이션, 총 2.8초)

조각이 포켓몬스터 → 울려라! → 천계의 하이퍼보이스 순서로 들어오고, 번쩍하는 순간 조각 묶음을 원본 로고 한 장(`TitleLogo`)으로 바꾼다.
조각 합성본은 원본과 픽셀 단위로 같지 않아서, 멈춰 있는 로고는 원본을 쓴다.

로고 묶음 Canvas 안에 조각 12장을 넣은 **Pieces** 묶음, 그 위에 **TitleLogo** (0, 0, 1500×500),
그 위에 **TitleLogo_Flash** (−22.1, −22.1, 1544.2×544.2) 를 둔다. 화면 전체를 덮는 흰 Image **FlashScreen** 을 로고보다 위 레이어에 하나 더 둔다.
기본(디자이너) 모습은 등장이 끝난 모습: Pieces 불투명도 0, TitleLogo 1, TitleLogo_Flash 0, FlashScreen 0.

시간은 애니메이션 시작 기준 초. “튕김”은 목표보다 살짝 지나쳤다 돌아오는 커브(ease out back).

| 시작 | 길이 | 대상 | 처음 → 끝 | 커브 |
|---|---|---|---|---|
| 0.00 | — | Pieces 불투명도 1, TitleLogo 불투명도 0 | 시작 키 | 상수 |
| 0.00 | 0.50 | Wave | Scale X 0.15 → 1, 불투명도 0 → 1 | ease out |
| 0.15 / 0.25 / 0.35 / 0.45 / 0.55 | 0.42 | 포 / 켓 / 몬 / 스 / 터 | Translation Y −140 → 0, Scale 1.5 → 1, 불투명도 0 → 1 | 튕김 |
| 0.95 | 0.40 | Shout (울려라!) | Scale 0 → 1, Angle −18 → 0, 불투명도 0 → 1 | 튕김 |
| 1.25 | 0.40 | Subtitle (천계의 하이퍼보이스) | Translation X 140 → 0, 불투명도 0 → 1 | ease out |
| 1.50 | 0.50 | FeatherLeft | Translation (−90, 50) → 0, Angle −35 → 0, 불투명도 0 → 1 | ease out |
| 1.50 | 0.50 | FeatherRight | Translation (90, 50) → 0, Angle 35 → 0, 불투명도 0 → 1 | ease out |
| 1.70 / 1.82 | 0.35 | StarLeft / StarRight | Scale 0 → 1, Angle −90 → 0, 불투명도 0 → 1 | 튕김 |
| 2.00 | 0.50 | TitleLogo_Flash, FlashScreen | 불투명도 0 → 1 (2.20) → 0 (2.50) | ease out |
| 2.20 | — | Pieces 불투명도 0, TitleLogo 불투명도 1 | 번쩍임이 가장 밝을 때 한 프레임에 바꿈 | 상수 |
| 2.50 | 0.30 | 시작 안내 (PromptBar 묶음) | Translation Y 16 → 0, 불투명도 0 → 1 | ease out |

FlashScreen 은 화면 전체 흰색, 최대 불투명도 0.6 정도면 눈이 아프지 않다. 가운데만 밝게 하려면 SunGlow 같은 방사형 텍스처를 흰색으로 쓴다.
바람 줄기(Streak)와 빛줄기는 등장이 끝난 뒤부터 보이게 한다 (LogoIntro 끝에 불투명도 키).
등장 중 건너뛰면 C++ 이 LogoIntro 끝 모습으로 맞추므로, 마지막 키가 반드시 “원본 로고만 보이는” 상태여야 한다.

## WBP 구성

WBP_Title(배경·로고·시작 안내와 `LogoIntro`·`ZoomOut`·`Idle`), WBP_ServerAddress·WBP_Login(단말기 패널과 `PanelIn`,
로그인은 `StatusWaveLoop` 도), WBP_LoadingScreen 은 이 문서 값대로 이미 구성돼 있다. 고칠 때는 UMG 디자이너에서 직접 고친다.

- C++ 에 바인딩된 위젯 이름(CharacterNameInput, IdInputBox, PrimaryActionButton…)과 애니메이션 이름은 바꾸지 않는다.
- 로딩 화면은 무비 플레이어가 로딩 스레드에서 그려 UMG 애니메이션이 돌지 않는다. 파형은 그릴 때 시간으로 움직이는 `UEVoiceWave` 위젯이다.
- 서버·로그인 WBP 의 클래스 기본값(Style 분류)에 들어 있는 값:
  - `StatusStyles`: 안내·연결 중·오류·성공별 문구 색과 아이콘. 연결 중은 아이콘 대신 파형(StatusWave).
  - `bUseInputErrorStyle` + `InputStyle` / `InputErrorStyle`: 오류 원인 입력칸만 빨간 테두리. 서버는 주소 칸, 로그인은 C++ 이 원인에 맞춰 고른다 (아이디 형식·중복 → 아이디, 비밀번호 불일치 → 확인 칸, 로그인 실패 → 비밀번호).
  - `bUseTabStyles` + `TabStyle` / `SelectedTabStyle` / 글자색: 선택된 탭은 남색 채움 + 흰 글자 + 노란 눈금.
- `Idle`·`StatusWaveLoop` 은 C++ 이 위젯이 뜰 때부터 계속 반복 재생하고, `PanelIn` 은 뜰 때 한 번 재생한다.
- 서버 화면의 “로컬 127.0.0.1” 칩은 `LocalAddressButton` 바인딩이다. 누르면 기본 주소가 채워진다.
- 키보드·패드 포커스 링: 버튼 옆에 이름이 `<버튼 이름>FocusRing` 인 Image 를 두면 C++(`FUEFrontendFocusRings`)이 그 버튼이 Tab·방향키·패드로 포커스를 받았을 때만 보여 준다. 마우스로 누른 경우엔 안 보인다. 주 버튼은 모서리 깎은 `FocusRing`, 링크·칩·탭은 `TabGroup_Frame` 을 청록으로 쓴다.
- 서버·로그인 화면일 때 뒤에 깔린 WBP_Title 은 포커스를 받지 않고 시작 버튼도 접힌다 (Tab 이동이 보이지 않는 시작 버튼으로 빠지던 문제).

## C++ 연결: 등장 건너뛰기와 줌아웃

WBP_Title 에 아래 **이름 그대로** 위젯 애니메이션을 만들면 C++ 이 찾아서 재생한다. 없으면 그냥 넘어간다.

| 애니메이션 | 담을 것 | 언제 재생되나 |
|---|---|---|
| `LogoIntro` | 위 등장 표 전체 | 게임을 켜고 타이틀이 처음 뜰 때 한 번 |
| `ZoomOut` | 0초 = 타이틀 장면, 1.6초 = 전체 장면. 배경 Transform, 로고 묶음 Transform, 시작 안내·바람 줄기 0.15초 페이드아웃 | 타이틀 → 서버 때 앞으로, 서버 → 타이틀 때 거꾸로 |

- WBP_Title 디자이너의 기본 모습은 **타이틀 장면**으로 둔다. ZoomOut 의 0초와 같아야 한다.
- 타이틀 위젯은 이제 서버·로그인 화면으로 넘어가도 지워지지 않고 그 뒤 배경으로 남는다. 배경·로고는 WBP_Title 에만 두고, WBP_ServerAddress·WBP_Login 은 배경 없이 패널만 둔다.
- 줌아웃이 시작되고 0.95초 뒤에 서버 패널이 붙는다 (`BP_FrontendPlayerController` 의 `MenuRevealDelay`). 패널 자체의 0.5초 슬라이드인은 서버 WBP 에서 Construct 때 재생한다.
- 로그인 ↔ 서버 전환은 이미 전체 장면이라 줌 없이 바로 바뀐다. 로비·캐릭터 이름 화면으로 가면 타이틀 위젯을 지운다.
- 등장 중 첫 Enter(또는 ContinueButton, 패드 A)는 등장을 끝 모습으로 건너뛰고, 다음 입력부터 서버 화면으로 넘어간다.
- 등장 도중 서버로 넘어가게 되면 등장을 먼저 끝 모습으로 맞춘 뒤 줌아웃한다.
- 전체 장면일 때 ContinueButton 은 클릭을 받지 않는다.
