# Palworld Character Customization

이 폴더는 커스터마이징 전용 레벨에서 쓰는 팔월드식 캐릭터 생성 코드야.

## 런타임 구조

- `Data/UEPalworldCustomizationTypes.*`
  블루프린트에서 읽고 바꿀 수 있는 데이터 모델이야. 몸 타입, 얼굴, 머리카락, 눈, 의상, 색상, 스케일 값을 담는다.

- `Preview/UEPalworldCustomizationPreviewActor.*`
  화면 중앙의 캐릭터 프리뷰를 담당해. 팔월드 추출 테이블 순서대로 메쉬를 골라 끼우고, 베이스 바디를 애니메이션 기준으로 삼아서 의상, 얼굴, 머리카락이 같이 움직이게 한다.

- `UI/UEPalworldCustomizationWidget.*`
  `WBP_PalworldCustomization`이 감싸는 UI 베이스 클래스야. 화면 배치는 블루프린트 안의 Canvas가 맡고, C++는 카탈로그 버튼, 팔레트, 체형 슬라이더 같은 반복 데이터만 채운다.

- `Framework/UEPalworldCustomizationPlayerController.*`
  레벨에서 프리뷰 액터를 찾고, 카탈로그와 위젯을 연결한다.

## 원본 데이터

카탈로그는 아래 파일에서 만든다.

- `Saved/Codex/PalworldImport/palworld_customization_manifest.json`
- `Saved/Codex/PalworldImport/palworld_unreal_import_manifest.json`
- `Saved/Codex/PalworldImport/palworld_fbx_asset_map.json`

옵션 목록은 파일 이름을 임의로 훑어서 만들지 않는다. 반드시 팔월드 캐릭터 생성 테이블의 순서와 ID를 기준으로 한다.

## 중요한 규칙

- 기본값은 원본 머티리얼과 원본 텍스처를 쓴다. 피부, 머리, 눈 색은 Palworld식 팔레트 버튼으로만 덮어쓴다.
- 피부색은 얼굴과 몸이 갈라져 보이지 않도록 같은 기본 팔월드 톤으로 시작한다.
- 기본 착장은 원본 머티리얼 그대로 시작한다. 저장된 커마가 없어도 다른 레벨에서 의상 없는 상태로 시작하지 않는다.
- 커스터마이징 화면에서는 얼굴을 덮는 장비 섹션을 숨긴다. 모자, 안경, 마스크처럼 위치가 안 맞거나 얼굴을 가리는 부착물은 현재 범위에서 제외한다.
- 체형은 Palworld 원본 모프 타깃(`BS_Torso_*`, `BS_Arm_*`, `BS_Leg_*`)으로 조절한다. 얼굴/머리만 따로 스케일하지 않는다.
- 선택 버튼은 텍스처 조각만 보여주지 말고, 가능하면 모델 썸네일을 보여줘야 한다.

## 검증 스크립트

검증 스크립트는 `Saved/Codex/PalworldImport/Scripts` 아래에 있고, 결과는 `Saved/Codex/PalworldImport/Reports`에 쓴다.

- `validate_palworld_catalog_asset.py`
- `validate_palworld_runtime_preview.py`
- `validate_palworld_fbx_catalog_sections.py`
- `generate_palworld_model_thumbnails.py`
- `import_palworld_model_thumbnails_to_catalog.py`
