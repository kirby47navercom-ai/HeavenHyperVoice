# Palworld Character Customization

이 폴더는 커스터마이징 전용 레벨에서 쓰는 팔월드식 캐릭터 생성 코드야.

## 런타임 구조

- `Data/UEPalworldCustomizationTypes.*`
  블루프린트에서 읽고 바꿀 수 있는 데이터 모델이야. 몸 타입, 얼굴, 머리카락, 눈, 의상, 색상, 스케일 값을 담는다.

- `Preview/UEPalworldCustomizationPreviewActor.*`
  화면 중앙의 캐릭터 프리뷰를 담당해. 팔월드 추출 테이블 순서대로 메쉬를 골라 끼우고, 의상 메쉬를 애니메이션 기준으로 삼아서 얼굴과 머리카락이 같이 움직이게 한다.

- `UI/UEPalworldCustomizationWidget.*`
  `WBP_PalworldCustomization`이 감싸는 UI 베이스 클래스야. 블루프린트 안에 Canvas 루트가 있으면 그 Canvas를 쓰고, 없으면 런타임에서 같은 구조를 만든다.

- `Framework/UEPalworldCustomizationPlayerController.*`
  레벨에서 프리뷰 액터를 찾고, 카탈로그와 위젯을 연결한다.

## 원본 데이터

카탈로그는 아래 파일에서 만든다.

- `Saved/Codex/PalworldImport/palworld_customization_manifest.json`
- `Saved/Codex/PalworldImport/palworld_unreal_import_manifest.json`
- `Saved/Codex/PalworldImport/palworld_fbx_asset_map.json`

옵션 목록은 파일 이름을 임의로 훑어서 만들지 않는다. 반드시 팔월드 캐릭터 생성 테이블의 순서와 ID를 기준으로 한다.

## 중요한 규칙

- 기본값은 원본 머티리얼과 원본 텍스처를 쓴다. RGB 0-255 값은 사용자가 팔레트로 덮어쓸 때만 적용한다.
- 피부색은 얼굴과 몸이 갈라져 보이지 않도록 같은 기본 팔월드 톤으로 시작한다.
- 현재 추출본에는 별도 알몸 베이스 바디가 없다. 그래서 의상/바디 장비 메쉬가 애니메이션 기준이 된다.
- 커스터마이징 화면에서는 머리 장비를 숨긴다. 모자, 귀, 안경, 마스크가 얼굴을 가리거나 위치가 틀어지는 문제를 피하기 위해서다.
- 키와 몸 폭은 공용 루트에 적용해서 몸, 얼굴, 머리카락, 의상이 같이 움직이게 한다.
- 선택 버튼은 텍스처 조각만 보여주지 말고, 가능하면 모델 썸네일을 보여줘야 한다.

## 검증 스크립트

검증 스크립트는 `Saved/Codex/PalworldImport/Scripts` 아래에 있고, 결과는 `Saved/Codex/PalworldImport/Reports`에 쓴다.

- `validate_palworld_catalog_asset.py`
- `validate_palworld_runtime_preview.py`
- `validate_palworld_fbx_catalog_sections.py`
- `generate_palworld_model_thumbnails.py`
- `import_palworld_model_thumbnails_to_catalog.py`
