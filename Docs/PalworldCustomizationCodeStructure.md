# Palworld 커스터마이징 코드 구조 정리

이 문서는 현재 `HeavenHyperVoice` 프로젝트에 들어간 Palworld 캐릭터 커스터마이징 흐름을, 코드와 에셋 기준으로 다시 읽기 쉽게 정리한 것이다.  
노션 업로드가 인증 문제로 막혀서, 같은 내용을 repo 안의 Markdown 문서로 남긴다.

## 한 줄 요약

현재 구조는 "커마 레벨에서 프리뷰 액터와 UMG 위젯으로 외형을 고르고, Start 버튼을 누르면 `GameInstance`에 외형 값을 임시 저장한 뒤 `PlayerTestLevel`로 이동해서 실제 `AUEPlayerCharacter`에 같은 외형을 다시 적용하는 방식"이다.

큰 흐름은 아래와 같다.

```text
L_PalworldCustomization
  -> BP_PalworldCustomizationPlayerController
  -> WBP_PalworldCustomization
  -> BP_PalworldCustomizationPreview
  -> DA_PalworldCustomizationCatalog
  -> UUEGameInstance::SetPendingPalworldAppearance()
  -> OpenLevel(/Game/Level/PlayerTestLevel, game=/Script/HeavenHyperVoice.UEGameModeBase)
  -> AUEPlayerCharacter::BeginPlay()
  -> AUEPlayerCharacter::ApplyPendingPalworldAppearance()
  -> AUEPlayerCharacter::ApplyPalworldAppearance()
```

## 관련 폴더

### 코드

- `Client/Source/HeavenHyperVoice/CharacterCustomization/Palworld/Data`
  - 커마 옵션, 선택 상태, 색상, 스케일 값을 담는 데이터 타입.
- `Client/Source/HeavenHyperVoice/CharacterCustomization/Palworld/Preview`
  - 커마 화면 중앙의 프리뷰 캐릭터를 조립하고 갱신하는 액터.
- `Client/Source/HeavenHyperVoice/CharacterCustomization/Palworld/UI`
  - 커마 UI 위젯과 버튼, 팔레트, 스케일 슬라이더, Start 버튼 처리.
- `Client/Source/HeavenHyperVoice/CharacterCustomization/Palworld/Framework`
  - 커마 레벨 전용 PlayerController.
- `Client/Source/HeavenHyperVoice/Character`
  - 실제 플레이어 캐릭터에 Palworld 커마 결과를 적용하는 코드.
- `Client/Source/HeavenHyperVoice/System`
  - 레벨 전환 중 커마 결과를 들고 가는 `GameInstance`.
- `Client/Source/HeavenHyperVoice/Data`
  - 플레이어 애니메이션용 DataAsset 타입.
- `Client/Source/HeavenHyperVoice/Animation`
  - AnimBP가 읽을 이동 상태 브리지.

### 에셋

- `Client/Content/CharacterCustomization/Palworld/Maps/L_PalworldCustomization.umap`
  - 커마 화면 레벨.
- `Client/Content/CharacterCustomization/Palworld/Blueprints/WBP_PalworldCustomization.uasset`
  - 커마 UI 블루프린트 위젯.
- `Client/Content/CharacterCustomization/Palworld/Blueprints/BP_PalworldCustomizationPreview.uasset`
  - 프리뷰 액터 블루프린트.
- `Client/Content/CharacterCustomization/Palworld/Blueprints/BP_PalworldCustomizationPlayerController.uasset`
  - 커마 화면용 PlayerController 블루프린트.
- `Client/Content/CharacterCustomization/Palworld/Blueprints/BP_PalworldCustomizationGameMode.uasset`
  - 커마 레벨에서 쓰는 GameMode 블루프린트 에셋.
- `Client/Content/CharacterCustomization/Palworld/Data/DA_PalworldCustomizationCatalog.uasset`
  - 몸, 얼굴, 머리, 눈, 의상 옵션과 팔레트를 담는 핵심 DataAsset.
- `Client/Content/CharacterCustomization/Palworld/Materials/M_PalworldCharacterMaster.uasset`
  - Palworld 추출 머티리얼용 마스터 머티리얼.
- `Client/Content/CharacterCustomization/Palworld/Generated/MorphSafeMaterials`
  - Morph Target 적용 시 기본 회색 머티리얼로 깨지는 문제를 줄이기 위해 만든 머티리얼 인스턴스들.
- `Client/Content/CharacterCustomization/Palworld/Generated/EyeComposite`
  - 눈 흰자, 홍채, 하이라이트를 합성해둔 눈 텍스처들.
- `Client/Content/Data/Animation/DA_PlayerAnimation.uasset`
  - 플레이어 애니메이션 시퀀스/몽타주 참조용 DataAsset.

## 핵심 데이터 모델

파일: `Client/Source/HeavenHyperVoice/CharacterCustomization/Palworld/Data/UEPalworldCustomizationTypes.h`

### `EUEPalworldGender`

```cpp
enum class EUEPalworldGender : uint8
{
	TypeA,
	TypeB
};
```

Palworld의 체형 타입이다. 현재 UI에서는 Type 1, Type 2로 보여준다.  
코드 기준으로 `TypeA`는 여성형 쪽, `TypeB`는 남성형 쪽 메쉬를 고르는 데 쓰인다.

### `EUEPalworldCustomizationCategory`

```cpp
enum class EUEPalworldCustomizationCategory : uint8
{
	Body,
	Head,
	Hair,
	Eyes,
	BodyEquipment
};
```

현재 지원하는 커마 카테고리다.

- `Body`: 체형 타입.
- `Head`: 얼굴/머리 베이스.
- `Hair`: 머리카락.
- `Eyes`: 눈 타입.
- `BodyEquipment`: 의상.

현재 코드에는 별도 `HeadEquipment`, `Accessory`, `Voice` 카테고리는 없다. 예전에 문제를 만들던 모자/안경/마스크류는 현재 지원 범위에서 빼는 방향으로 처리되어 있다.

### `EUEPalworldColorChannel`

```cpp
enum class EUEPalworldColorChannel : uint8
{
	Skin,
	Hair,
	Eye
};
```

현재 직접 색을 바꾸는 채널이다.

- 피부색
- 머리색
- 눈색

의상 색은 현재 RGB로 직접 덮는 구조가 아니라, 원본 Palworld 머티리얼과 텍스처를 유지하는 방향이다.

### `EUEPalworldScaleChannel`

```cpp
enum class EUEPalworldScaleChannel : uint8
{
	TorsoSize,
	ArmSize,
	LegSize
};
```

Palworld식 체형 조절을 위해 쓰는 채널이다.

- 몸통 크기
- 팔 크기
- 다리 크기

이 값은 액터 전체 스케일이나 머리만 따로 줄이는 값이 아니다.  
현재 구현은 `BS_Torso_min/max`, `BS_Arm_min/max`, `BS_Leg_min/max` morph target에 `-1.0 ~ 1.0` 값을 넣는 방식이다.

### `FUEPalworldCustomizationOption`

하나의 선택지를 나타낸다.

주요 필드:

- `Id`: 내부 식별자.
- `DisplayName`: UI 표시명.
- `Category`: 어떤 카테고리의 옵션인지.
- `FemaleMesh`: TypeA일 때 쓸 SkeletalMesh.
- `MaleMesh`: TypeB일 때 쓸 SkeletalMesh.
- `Icon`: UI 타일 썸네일.
- `Material`: 눈 타입처럼 별도 머티리얼을 직접 참조해야 하는 경우 사용.

중요 함수:

```cpp
USkeletalMesh* FUEPalworldCustomizationOption::LoadMesh(EUEPalworldGender Gender) const;
```

이 함수는 성별에 맞는 메쉬를 고른 뒤, 의상 메쉬라면 `/Palworld/Assets/` 경로의 단일 슬롯 메쉬 대신 `/Palworld/AssetsFBX/` 쪽 재추출 메쉬를 우선 사용하려고 시도한다.  
이유는 일부 `/Assets` 의상 메쉬가 머티리얼 슬롯이 1개라서 원본 옷 텍스처를 제대로 복원하기 어렵기 때문이다.

### `FUEPalworldAppearance`

현재 선택된 커마 상태 전체를 담는다.

주요 필드:

- `Gender`
- `BodyIndex`
- `HeadIndex`
- `HairIndex`
- `EyeIndex`
- `BodyEquipmentIndex`
- `SkinColor`
- `HairColor`
- `EyeColor`
- `ArmVolume`
- `TorsoVolume`
- `LegVolume`

Start 버튼을 누르면 이 구조체가 `UUEGameInstance`에 저장되고, 다음 레벨의 플레이어가 이 값을 읽어서 외형을 다시 조립한다.

### `UUEPalworldCustomizationCatalog`

커마 옵션 목록을 담는 DataAsset 클래스다.

담는 배열:

- `BodyOptions`
- `HeadOptions`
- `HairOptions`
- `EyeOptions`
- `BodyEquipmentOptions`
- `SkinColors`
- `HairColors`
- `EyeColors`

주요 함수:

- `GetOptions(Category)`
- `GetOptionCount(Category)`
- `GetOption(Category, Index)`

실제 에셋은 `Client/Content/CharacterCustomization/Palworld/Data/DA_PalworldCustomizationCatalog.uasset`이다.

## 커마 레벨 PlayerController

파일: `Client/Source/HeavenHyperVoice/CharacterCustomization/Palworld/Framework/UEPalworldCustomizationPlayerController.*`

클래스: `AUEPalworldCustomizationPlayerController`

역할:

- 커마 레벨에 들어오면 기본 Pawn을 숨긴다.
- 레벨에 배치된 `AUEPalworldCustomizationPreviewActor`를 찾는다.
- 카메라 ViewTarget을 프리뷰 액터로 바꾼다.
- `WBP_PalworldCustomization` 위젯을 생성해 Viewport에 올린다.
- 마우스 커서를 켜고 Game+UI 입력 모드로 전환한다.
- 마우스 휠과 드래그 입력을 프리뷰 회전/줌/이동에 연결한다.

중요 흐름:

```cpp
BeginPlay()
  -> 기본 Pawn 숨김
  -> PreviewActor 검색
  -> SetViewTarget(PreviewActor)
  -> CreateWidget<UUEPalworldCustomizationWidget>()
  -> Widget->SetPreviewActor(PreviewActor)
  -> Widget->SetCatalog(PreviewActor->GetCatalog())
  -> AddToViewport()
```

현재 `CustomizationWidgetClass`는 먼저 `/Game/CharacterCustomization/Palworld/Blueprints/WBP_PalworldCustomization`를 찾는다.  
찾으면 블루프린트 위젯을 사용하고, 못 찾으면 C++ `UUEPalworldCustomizationWidget` 클래스로 떨어진다.

주의할 점:

- 커마 UI의 시각적 배치는 블루프린트 `WBP_PalworldCustomization` 안의 Designer Canvas가 기준이다.
- C++는 그 안의 이름 있는 위젯을 찾아 데이터와 버튼 목록을 채운다.
- 그래서 완전히 C++만으로 UI를 그리는 구조는 아니지만, 일부 타일/팔레트/슬라이더는 런타임에 C++가 생성한다.

## 커마 UI 위젯

파일: `Client/Source/HeavenHyperVoice/CharacterCustomization/Palworld/UI/UEPalworldCustomizationWidget.*`

클래스:

- `UUEPalworldCustomizationWidget`
- `UUEPalworldOptionButton`
- `UUEPalworldColorButton`
- `UUEPalworldScaleSlider`

### `UUEPalworldOptionButton`

옵션 타일과 카테고리 버튼에 쓰는 버튼 클래스다.

역할:

- 어떤 카테고리와 인덱스를 선택할지 저장한다.
- 클릭되면 소유 위젯의 `SelectOption` 또는 `OpenCategory`로 전달한다.

### `UUEPalworldColorButton`

팔레트 색상 버튼이다.

역할:

- 어떤 색상 채널인지 저장한다.
- 클릭되면 `SelectColor(Channel, Color)`로 전달한다.

### `UUEPalworldScaleSlider`

체형 조절 슬라이더다.

역할:

- 몸통, 팔, 다리 중 어떤 스케일 채널인지 저장한다.
- 값이 바뀌면 `SetScaleFromSlider`로 전달한다.

### `UUEPalworldCustomizationWidget`

실제 커마 UI 본체다.

주요 필드:

- `Catalog`: `DA_PalworldCustomizationCatalog` 참조.
- `PreviewActor`: 화면 중앙 프리뷰 액터 참조.
- `CurrentCategory`: 현재 열려 있는 카테고리.
- `CachedAppearance`: 프리뷰가 없을 때 임시로 들고 있는 커마 상태.
- `StartLevelName`: Start 버튼으로 이동할 레벨. 기본값은 `PlayerTestLevel`.

주요 함수:

- `SetPreviewActor`
  - 프리뷰 액터를 연결한다.
  - 프리뷰 액터에서 Catalog도 가져온다.
- `SetCatalog`
  - 커마 DataAsset을 위젯에 연결한다.
- `SelectOption`
  - 선택한 옵션을 프리뷰 액터에 전달한다.
  - UI를 다시 동기화한다.
- `SelectGender`
  - TypeA/TypeB를 프리뷰에 전달한다.
- `SelectColor`
  - 피부, 머리, 눈 색상을 프리뷰에 전달한다.
- `SetScaleValue`
  - 슬라이더 값을 프리뷰에 전달한다.
- `OpenCategory`
  - 왼쪽 카테고리 변경.
  - 카테고리 버튼, 옵션 타일, 파라미터 패널을 다시 만든다.
- `StartWithCurrentAppearance`
  - 프리뷰의 현재 외형을 `GameInstance`에 저장한다.
  - `/Game/Level/PlayerTestLevel`로 이동한다.
  - 이동 옵션에 `game=/Script/HeavenHyperVoice.UEGameModeBase`를 붙인다.

### Start 버튼 흐름

```cpp
UUEPalworldCustomizationWidget::StartWithCurrentAppearance()
  -> RefreshFromPreview()
  -> UUEGameInstance::SetPendingPalworldAppearance(CachedAppearance)
  -> UGameplayStatics::OpenLevel("/Game/Level/PlayerTestLevel", "game=/Script/HeavenHyperVoice.UEGameModeBase")
```

이 구조 때문에 커마 결과는 다른 레벨로 넘어갈 때 직접 actor를 옮기는 게 아니라, `GameInstance`에 값만 저장하고 다음 레벨에서 다시 적용한다.

### 옵션 표시 규칙

현재 UI는 Catalog에 있는 모든 Raw 옵션을 그대로 보여주지 않는다.

- Body는 실제 커마에서 쓰는 TypeA/TypeB 2개만 보인다.
- BodyEquipment는 1번부터 14번까지만 보이게 제한되어 있다.
- 0번 의상은 추출 기본값/베이스로 취급되어 일반 선택지에서 제외된다.

관련 함수:

- `GetVisibleOptionCount`
- `GetActualOptionIndex`

### 팔레트 색상

현재 숫자 RGB 입력이 아니라 팔레트/색상 매트릭스 방식이다.

색상 우선순위:

1. Catalog에 들어 있는 `SkinColors`, `HairColors`, `EyeColors`
2. 코드에서 추가로 생성하는 시각 팔레트 색상
3. 팔레트가 비었을 때의 fallback 색상

의상 카테고리에서는 색상 RGB를 직접 덮지 않고, 원본 Palworld 머티리얼을 쓰는 안내만 표시한다.

### 프리뷰 마우스 조작

위젯 쪽에서도 프리뷰 영역에서 마우스를 처리한다.

- 왼쪽 드래그: 캐릭터 회전.
- 오른쪽 또는 가운데 드래그: 카메라/프리뷰 이동.
- 휠: 확대/축소.

관련 함수:

- `NativeOnMouseButtonDown`
- `NativeOnMouseButtonUp`
- `NativeOnMouseMove`
- `NativeOnMouseWheel`
- `IsPointerOverPreviewArea`

## 프리뷰 액터

파일: `Client/Source/HeavenHyperVoice/CharacterCustomization/Palworld/Preview/UEPalworldCustomizationPreviewActor.*`

클래스: `AUEPalworldCustomizationPreviewActor`

커마 화면 중앙의 캐릭터를 조립하는 핵심 액터다.

주요 컴포넌트:

- `CharacterRoot`
- `BaseBodyMesh`
- `BodyEquipmentMesh`
- `HeadMesh`
- `HairMesh`
- `PreviewCamera`
- 조명 컴포넌트들

### 생성자

역할:

- SkeletalMeshComponent들을 만든다.
- 충돌을 끈다.
- 데칼 수신을 끈다.
- 커마 프리뷰에서는 선택할 때마다 캐릭터가 위아래로 덜컹거리지 않도록 SingleNode 애니메이션 모드와 pause 상태를 기본으로 둔다.
- Catalog 기본값을 로드한다.

### `ApplyAppearance`

외부에서 커마 상태 전체를 넣을 때 사용한다.

```cpp
ApplyAppearance(NewAppearance)
  -> Appearance = NewAppearance
  -> NormalizeLegacyDefaultColors()
  -> RefreshMeshes()
```

### `SelectOption`

카테고리와 인덱스를 받아서 해당 옵션만 바꾼다.

Body를 선택하면 `TypeA`, `TypeB`에 따라 `Gender`도 같이 바뀐다.

### `SetColor`

피부, 머리, 눈 색상만 변경한다.  
의상은 여기서 색상을 덮지 않는다.

### `SetScaleValue`

UI 슬라이더의 `0~100` 값을 `-1.0~1.0` morph 값으로 바꿔 저장한다.

```text
0   -> -1.0
50  ->  0.0
100 -> +1.0
```

### `RefreshMeshes`

프리뷰 조립의 중심 함수다.

하는 일:

1. Catalog가 없으면 종료.
2. 기본 색상을 보정.
3. 선택 인덱스를 유효 범위로 clamp.
4. Body 0번은 실제 선택용이 아니면 TypeA/TypeB 기본값으로 보정.
5. 의상은 1~14 범위로 제한.
6. Body, BodyEquipment, Head, Hair, Eyes 옵션을 가져온다.
7. BaseBody와 선택 의상이 같은 메쉬인지 확인한다.
8. 별도 의상 메쉬가 필요하면 `BodyEquipmentMesh`에 넣고, 아니면 숨긴다.
9. Body, Outfit, Head, Hair 메쉬를 각각 지정한다.
10. 각 컴포넌트의 머티리얼 override를 초기화한다.
11. 각 메쉬 폴더의 원본/로컬 머티리얼을 다시 찾는다.
12. 얼굴을 가리는 마스크/안경/모자류 섹션을 숨긴다.
13. 별도 의상을 입으면 기본 Body의 옷 섹션을 숨긴다.
14. 프리뷰 파츠 애니메이션을 정지 상태로 둔다.
15. 같은 Skeleton이면 `LeaderPose`로 묶는다.
16. 피부/머리 색을 적용한다.
17. 눈 머티리얼을 홍채 슬롯에만 적용한다.
18. morph target 스케일을 적용한다.
19. 카메라 프레이밍을 갱신한다.
20. 지원하지 않는 액세서리 컴포넌트를 제거/숨김 처리한다.

### `RefreshFollowerPose`

Body를 리더로 두고, 의상/머리/얼굴 파츠가 같은 Skeleton이면 `SetLeaderPoseComponent(BaseBodyMesh)`로 묶는다.

중요한 이유:

- 같은 Skeleton인 파츠는 바디 애니메이션을 같이 따라간다.
- 다른 Skeleton인 파츠는 강제로 묶지 않는다.
- 강제로 묶으면 머리카락 같은 파츠가 어깨망이나 엉뚱한 본으로 따라가서 깨질 수 있다.

### `ApplyMaterialColors`

현재 선택한 피부색과 머리색을 특정 슬롯에만 적용한다.

- 피부색: Head, BaseBody, BodyEquipment의 Body/Skin 슬롯.
- 머리색: HairMesh의 Hair 슬롯.

눈, 흰자, 눈썹, 속눈썹, 입, 코, 수염 등 얼굴 디테일 슬롯은 여기서 색 덮기를 피한다.

### `ApplyMeshLocalMaterials`

메쉬에 붙어 있는 기본 머티리얼이 잘못 들어온 경우, 같은 메쉬 폴더 주변의 원본 머티리얼을 찾아 다시 꽂는다.

특히 의상은 다음 문제를 줄이기 위해 이 과정을 거친다.

- `/Assets` 쪽 메쉬가 머티리얼 슬롯 1개로 들어와서 옷 색/무늬가 빠짐.
- 성별이 다른 머티리얼이 잘못 연결됨.
- 원본 Palworld 의상 텍스처가 아니라 흰색/회색 기본 머티리얼처럼 보임.

### `ApplyEyeMaterial`

눈 옵션 머티리얼을 Head 전체에 덮지 않고, 눈 홍채 슬롯으로 판단되는 슬롯에만 적용한다.

눈색은 가능하면 `Generated/EyeComposite`의 합성 텍스처를 사용한다.  
합성 텍스처가 없으면 머티리얼 파라미터 색상으로 fallback한다.

### `IsEyeIrisMaterialSlot`

눈 머티리얼을 넣어도 되는 슬롯인지 판단한다.

제외하는 것:

- Head/Skin 계열
- Brow
- Beard
- Mouth/Lip/Nose
- Lash/Eyelid
- Teeth/Tongue
- Sclera/White/Highlight

허용하는 것:

- `MI_Player_Eye`
- `player_eye`
- `_eye`
- `iris`
- `pupil`

이 함수가 중요한 이유는 눈 머티리얼이 얼굴 전체에 들어가서 얼굴이 검거나 이상한 색으로 덮이는 문제를 막기 위해서다.

### `HideFaceCoverSections`

마스크, 얼굴 덮개, 머리 장비, 모자, 안경처럼 얼굴을 가리거나 위치가 안 맞는 섹션을 숨긴다.

현재 범위에서는 Palworld 장신구/머리 장비를 커마 선택지로 지원하지 않기 때문에, 이전 BP나 추출 파츠가 남아 있으면 화면에 나오지 않도록 막는 용도다.

### `HideBaseBodyOutfitSections`

별도 의상을 입을 때 기본 Body 안의 OldCloth/Outfit 섹션이 겹쳐 보이지 않도록 숨긴다.  
다만 Body/Skin 섹션은 남겨야 손, 목, 피부가 보인다.

### `ApplyScale`

프리뷰의 체형 스케일 적용 함수다.

하는 일:

- `CharacterRoot`와 각 파츠의 Scale/Location을 기본값으로 되돌린다.
- 머리나 얼굴만 따로 줄이지 않는다.
- BaseBody와 BodyEquipment에 아래 morph target만 적용한다.

```text
BS_Torso_min / BS_Torso_max
BS_Arm_min   / BS_Arm_max
BS_Leg_min   / BS_Leg_max
```

즉, 현재 방식은 Palworld 원본의 체형 조절용 morph target에 의존한다.

### QA 스크린샷 함수

프리뷰 액터에는 커맨드라인 기반 QA 캡처 함수도 들어 있다.

- `ApplyQACommandLineAppearance`
- `CaptureQAWidgetScreenshot`
- `CaptureQAScreenshot`
- `PrepareQAHeadScreenshot`
- `CaptureQAHeadScreenshot`
- `ExitAfterQAScreenshot`

용도는 자동 실행에서 특정 조합을 적용하고 스크린샷을 찍어 검증하는 것이다.

## 실제 플레이어 캐릭터 적용

파일: `Client/Source/HeavenHyperVoice/Character/UEPlayerCharacter.*`

클래스: `AUEPlayerCharacter`

기존 역할:

- 이동 입력 처리.
- 카메라/스프링암.
- 서버 이동 보정.
- 포켓몬 동료 스폰/디스폰 이벤트.

추가된 Palworld 커마 역할:

- 실제 플레이어 메쉬에 커마 결과를 적용한다.
- 프리뷰 액터와 거의 같은 조립 규칙을 게임 레벨에서도 반복한다.

### 추가된 커마 컴포넌트

생성자에서 아래 컴포넌트를 만든다.

- `PalworldBodyEquipmentMesh`
- `PalworldHeadMesh`
- `PalworldHairMesh`

모두 기본 `GetMesh()`에 붙고, 충돌은 꺼져 있다.

### 로드하는 에셋

생성자에서 기본 로드한다.

- `DA_PalworldCustomizationCatalog`
  - `/Game/CharacterCustomization/Palworld/Data/DA_PalworldCustomizationCatalog`
- `DA_PlayerAnimation`
  - `/Game/Data/Animation/DA_PlayerAnimation`

### `BeginPlay`

```cpp
BeginPlay()
  -> PlayerCharacterInit()
  -> ApplyPendingPalworldAppearance()
```

플레이어가 생성되면 커마 레벨에서 저장해 둔 외형이 있는지 확인하고 적용한다.  
없으면 기본 Palworld 외형을 적용한다.

### `ApplyPendingPalworldAppearance`

`UUEGameInstance`에서 `PendingPalworldAppearance`를 읽는다.

- 값이 있으면 그 외형을 적용한다.
- 값이 없으면 `FUEPalworldAppearance()` 기본값을 적용한다.

이 때문에 커마 없이 일반 레벨로 들어가도 기본 의상/기본 머리/기본 눈이 세팅되어야 한다.

### `ApplyPalworldAppearance`

실제 게임 캐릭터 조립의 핵심 함수다.

하는 일:

1. Catalog가 없으면 로드한다.
2. Body, Head, Hair, Eye, Outfit 인덱스를 유효 범위로 보정한다.
3. Body 0번은 실제 선택용이 아니면 TypeA/TypeB 기본값으로 보정한다.
4. Outfit은 1~14 범위로 제한한다.
5. Body, Outfit, Head, Hair, Eyes 옵션을 가져온다.
6. `GetMesh()`에 Body 메쉬를 넣는다.
7. 별도 Outfit 메쉬가 필요하면 `PalworldBodyEquipmentMesh`에 넣는다.
8. Head, Hair 메쉬를 각각 별도 컴포넌트에 넣는다.
9. 모든 파츠의 머티리얼 override를 초기화한다.
10. 메쉬 폴더의 원본/로컬 머티리얼을 재적용한다.
11. 마스크/얼굴덮개/머리장비 섹션을 숨긴다.
12. 별도 의상을 입으면 기본 바디의 의상 섹션을 숨긴다.
13. 같은 Skeleton인 파츠를 `GetMesh()`의 `LeaderPose`로 묶는다.
14. 파츠 RelativeTransform을 Identity로 맞춘다.
15. 피부색, 머리색을 필요한 슬롯에만 적용한다.
16. 눈 머티리얼을 눈 홍채 슬롯에만 적용한다.
17. 체형 morph target을 적용한다.
18. 지원하지 않는 액세서리 컴포넌트를 제거/숨긴다.

### `ResetPalworldMaterials`

컴포넌트의 머티리얼 override를 전부 제거해서, SkeletalMesh 에셋이 가진 기본 머티리얼 슬롯으로 되돌린다.

### `ApplyPalworldMeshLocalMaterials`

프리뷰 액터의 `ApplyMeshLocalMaterials`와 같은 목적이다.  
실제 게임 플레이어에서도 원본 Palworld 머티리얼을 다시 찾아 꽂는다.

특히 의상에서 중요하다.

- 단일 슬롯으로 잘못 추출된 의상은 원본 `MI_*_M01`을 찾는다.
- `/AssetsFBX` 의상은 같은 폴더 안의 `MI___M##` 또는 원본 머티리얼 이름을 우선 찾는다.
- 머리카락은 잘못된 보정 로직이 머리 색/모양을 망가뜨릴 수 있어서 원본 머티리얼을 그대로 두는 예외가 있다.

### `ApplyPalworldEyeMaterial`

실제 플레이어 HeadMesh의 눈 홍채 슬롯에만 눈 머티리얼/합성 텍스처를 적용한다.

프리뷰와 마찬가지로 `Generated/EyeComposite`의 텍스처를 먼저 찾는다.

### `ApplyPalworldColorToSlots`

피부/머리 색을 특정 슬롯에만 적용한다.

이 함수도 눈, 흰자, 눈썹, 속눈썹, 입, 코, 수염 같은 얼굴 디테일은 제외한다.

### `ApplyPalworldScale`

실제 플레이어에서도 actor scale이 아니라 morph target을 적용한다.

적용 대상:

- `GetMesh()`
- `PalworldBodyEquipmentMesh`

적용하지 않는 대상:

- HeadMesh
- HairMesh

이유는 Palworld 체형 조절은 몸통/팔/다리 체형 morph 쪽이고, 머리와 얼굴을 별도로 줄이면 목/얼굴/머리 연결이 깨지기 쉽기 때문이다.

## 레벨 이동과 커마 상태 전달

파일: `Client/Source/HeavenHyperVoice/System/UEGameInstance.*`

클래스: `UUEGameInstance`

추가된 필드:

- `PendingPalworldAppearance`
- `bHasPendingPalworldAppearance`

추가된 함수:

- `SetPendingPalworldAppearance`
- `GetPendingPalworldAppearance`
- `ClearPendingPalworldAppearance`

역할:

- 커마 레벨에서 Start를 누른 순간의 `FUEPalworldAppearance`를 저장한다.
- 다음 레벨에서 플레이어가 생성될 때 그 값을 읽는다.
- 이 값은 `Transient`라서 영구 저장이 아니라 레벨 이동용 임시 상태다.

현재 구조상 "커마하고 플레이했을 때 같은 외형으로 넘어가기"는 이 GameInstance 경유로 처리된다.

## 플레이어 애니메이션 DataAsset

파일:

- `Client/Source/HeavenHyperVoice/Data/UEPlayerAnimationDataAsset.h`
- `Client/Source/HeavenHyperVoice/Data/UEPlayerAnimationDataAsset.cpp`

클래스: `UUEPlayerAnimationDataAsset`

실제 에셋:

- `Client/Content/Data/Animation/DA_PlayerAnimation.uasset`

### 직접 슬롯

- `IdleSequence`
- `WalkSequence`
- `RunSequence`
- `JumpSequence`
- `FallSequence`
- `RollMontage`
- `AttackMontage`
- `HitMontage`
- `DeathMontage`

### 태그 확장 슬롯

- `MontageEntries`
  - `FUEPlayerMontageEntry`
  - GameplayTag와 `UAnimMontage`를 매칭한다.
- `SequenceEntries`
  - `FUEPlayerSequenceEntry`
  - GameplayTag와 `UAnimSequence`를 매칭한다.

### 조회 함수

- `FindMontageByTag`
  - `State.Character.Roll` 태그는 `RollMontage`를 바로 반환한다.
  - 그 외는 `MontageEntries` 배열에서 찾는다.
- `FindSequenceByTag`
  - Idle, Walk, Run, Jump, Fall은 직접 슬롯을 먼저 확인한다.
  - 그 외는 `SequenceEntries` 배열에서 찾는다.

현재 이 DataAsset은 "플레이어 몽타주/시퀀스를 코드와 블루프린트가 같은 방식으로 찾기 위한 목록"이다.  
실제 재생은 AnimBP나 캐릭터 액션 코드에서 이 DataAsset을 읽어 호출해야 한다.

## AnimInstance 브리지

파일:

- `Client/Source/HeavenHyperVoice/Animation/UEAnimInstance.h`
- `Client/Source/HeavenHyperVoice/Animation/UEAnimInstance.cpp`

클래스: `UUEAnimInstance`

역할:

- AnimBP에서 필요한 이동 상태를 직접 계산해 노출한다.

노출하는 값:

- `OwnerCharacter`
- `GroundSpeed`
- `DirectionAngle`
- `MovementInput`
- `bIsMoving`
- `bIsRunning`
- `bIsRolling`
- `bIsFalling`

`NativeUpdateAnimation`에서 플레이어 캐릭터의 Velocity, MovementInput, CharacterMovement 상태를 읽고 값을 갱신한다.

## 포켓몬 동료 관련 코드와 커마의 관계

파일: `Client/Source/HeavenHyperVoice/Character/UEPlayerCharacter.*`

`AUEPlayerCharacter`에는 기존 포켓몬 동료 기능이 남아 있다.

관련 필드/함수:

- `PokemonCompanionClass`
- `SpawnedPokemon`
- `PendingDespawnPokemon`
- `PokemonLifecycleBrain`
- `TrySpawnPokemonCompanion`
- `RequestDespawnPokemonCompanion`
- `FinishPokemonDespawn`
- `BP_OnPokemonSpawnRequested`
- `BP_OnPokemonSpawned`
- `BP_OnPokemonDespawnRequested`
- `BP_OnPokemonDespawned`

이번 Palworld 커마 코드는 이 포켓몬 동료 로직을 직접 지우거나 대체하지 않는다.  
다만 `AUEPlayerCharacter` 생성자와 `BeginPlay`에 커마 적용 코드가 추가되어, 같은 클래스 안에 포켓몬 동료 로직과 Palworld 외형 적용 로직이 함께 들어 있다.

따라서 동료가 안 따라다니는 문제는 이 문서 기준으로는 별도 추적 대상이다.  
커마 코드는 플레이어 외형 메쉬를 교체하지만, 동료 FSM 자체를 끄는 코드는 아니다.

## 머티리얼 적용 규칙

현재 방향은 "의상은 원본 Palworld 머티리얼을 유지하고, 피부/머리/눈만 커마 색상으로 바꾼다"이다.

### 원본 머티리얼 복구

프리뷰와 실제 플레이어 양쪽에 같은 계열 함수가 있다.

- Preview:
  - `ResetComponentMaterials`
  - `ApplyMeshLocalMaterials`
- Player:
  - `ResetPalworldMaterials`
  - `ApplyPalworldMeshLocalMaterials`

원칙:

1. 먼저 Component의 material override를 제거한다.
2. SkeletalMesh가 가진 기본 머티리얼 슬롯을 다시 사용한다.
3. 잘못 연결된 경우, 메쉬 폴더나 원본 Outfit 폴더에서 같은 이름의 머티리얼을 다시 찾는다.
4. `/AssetsFBX` 의상은 같은 폴더 안에 들어온 재추출 머티리얼을 우선 사용한다.
5. Hair는 원본 머티리얼 유지가 우선이다.

### 피부/머리 색상

피부와 머리색은 `TintColor` 파라미터를 가진 Dynamic Material Instance로 적용한다.

단, 얼굴 디테일 슬롯은 제외한다.

제외 키워드:

- eye
- iris
- pupil
- sclera
- white
- highlight
- brow
- lash
- lid
- mouth
- nose
- lip
- teeth
- tongue
- line
- beard
- mustache
- moustache

### 눈 머티리얼

눈은 반드시 홍채/눈동자 슬롯으로 판단되는 곳에만 들어가야 한다.

흰자와 하이라이트를 살리기 위해 `Generated/EyeComposite` 텍스처를 우선 사용한다.  
예시 경로:

```text
/Game/CharacterCustomization/Palworld/Generated/EyeComposite/Eye001/T_Player_Eye001_Composite_C00
```

합성 텍스처가 없으면 색상 파라미터만 넣는다.

## 체형 스케일 규칙

현재 구현은 Palworld식 morph target 기반이다.

UI 슬라이더 값:

```text
0~100
```

내부 저장값:

```text
-1.0~1.0
```

적용 morph target:

```text
BS_Torso_min / BS_Torso_max
BS_Arm_min   / BS_Arm_max
BS_Leg_min   / BS_Leg_max
```

적용 대상:

- BaseBody
- BodyEquipment

적용하지 않는 대상:

- Head
- Hair

이 방식의 의도는 얼굴만 줄어들거나 머리카락만 따로 떠 보이는 문제를 피하는 것이다.  
단, 의상 메쉬에 같은 morph target이 없으면 바디와 의상이 완전히 같은 방식으로 변형되지 않을 수 있다.

## 의상 표시 규칙

현재 의상은 1번부터 14번까지만 UI에 보여준다.

관련 상수:

```cpp
constexpr int32 PalworldMaxVisibleOutfits = 14;
constexpr int32 PalworldFirstVisibleOutfitIndex = 1;
constexpr int32 PreviewMaxVisibleOutfits = 14;
constexpr int32 PreviewFirstVisibleOutfitIndex = 1;
```

이 제한은 아래 양쪽에 들어 있다.

- 프리뷰 액터
- 실제 플레이어 적용 코드

0번 의상은 일반 선택지에서 제외된다.

## 현재 구조의 한계와 주의점

### UI는 블프와 C++가 섞여 있다

`WBP_PalworldCustomization` 블루프린트가 Designer Canvas와 기본 배치를 가진다.  
C++는 그 안의 이름 있는 위젯을 찾아 옵션 타일, 색상 팔레트, 스케일 슬라이더를 채운다.

그래서 "완전히 코드 UI"도 아니고, "완전히 블루프린트 UI"도 아니다.  
현재 실제 구조는 "블프 캔버스 + C++ 데이터 바인딩/동적 버튼 생성"이다.

### C++ 주석 일부가 깨져 보인다

현재 소스 일부 한글 주석은 인코딩이 깨져 보인다.  
이 문서는 그 깨진 주석 대신 사람이 읽을 수 있는 구조 설명을 남기기 위한 보조 문서다.

### 지원하지 않는 장신구는 숨긴다

모자, 안경, 마스크, HeadEquipment, Accessory는 현재 선택 가능한 정식 커마 카테고리가 아니다.  
위치가 맞지 않는 장신구가 남아 있으면 `HideUnsupportedAttachmentComponents` 계열 함수가 숨긴다.

### Skeleton이 다르면 같이 애니메이션되지 않는다

의상/머리/얼굴 파츠가 Body와 같은 Skeleton이면 `LeaderPose`로 같이 움직인다.  
Skeleton이 다르면 강제로 묶지 않는다.  
이 경우 애니메이션을 완전히 맞추려면 같은 Skeleton으로 재임포트하거나 리타겟/스킨 작업이 필요하다.

### 커마 저장은 영구 저장이 아니다

현재 `GameInstance`에 넣는 `PendingPalworldAppearance`는 레벨 이동용 임시 값이다.  
게임을 껐다 켜도 유지하려면 SaveGame이나 서버 저장 구조로 별도 확장이 필요하다.

### UE 버전

현재 로컬 프로젝트는 UE 5.8 경로를 사용한다.

```text
C:\Program Files\Epic Games\UE_5.8
```

다만 에디터가 DLL을 잡고 있으면 C++ 빌드가 링크 단계에서 실패할 수 있다.  
그 경우 에디터를 닫고 다시 빌드해야 한다.

## 검증 관련 파일

작업 중 생성/사용된 검증 스크립트와 자료는 주로 아래에 있다.

```text
Client/Saved/Codex/PalworldImport
```

대표 파일:

- `palworld_customization_manifest.json`
- `palworld_unreal_import_manifest.json`
- `palworld_fbx_asset_map.json`
- `CUE4ParseDump/DataTables/DT_CharacterCreationMeshPresetTable_Body.json`
- `CUE4ParseDump/DataTables/DT_CharacterCreationMeshPresetTable_Head.json`
- `CUE4ParseDump/DataTables/DT_CharacterCreationMeshPresetTable_Hair.json`
- `CUE4ParseDump/DataTables/DT_CharacterCreationEyeMaterialPresetTable.json`
- `CUE4ParseDump/DataTables/DT_CharacterCreationColorPresetTable.json`

대표 스크립트:

- `Scripts/validate_palworld_catalog_asset.py`
- `Scripts/validate_palworld_runtime_preview.py`
- `Scripts/validate_palworld_play_start_transfer.py`
- `Scripts/validate_palworld_fbx_catalog_sections.py`
- `Scripts/validate_palworld_selection_flow.py`
- `Scripts/generate_palworld_model_thumbnails.py`
- `Scripts/generate_palworld_eye_composites.py`
- `Scripts/create_palworld_morphsafe_materials.py`
- `Scripts/import_palworld_fbx_catalog_assets.py`
- `Scripts/repair_palworld_catalog_links.py`
- `Scripts/repair_palworld_eye_materials_and_catalog.py`
- `Scripts/repair_palworld_materials.py`

대표 스크린샷:

- `Screenshots/Palworld_Play_Final_Customization.png`
- `Screenshots/Palworld_Play_Final_PlayerTestLevel.png`
- `Screenshots/Palworld_Play_After_Start_Final.png`
- `Screenshots/palworld_after_fix_default.png`
- `Screenshots/palworld_after_fix_default_2.png`

## 최근 커밋 흐름

최근 Palworld 커마 관련 커밋은 아래 순서로 쌓였다.

- `965d267 fix(customization): replace VRoid editor with Palworld flow`
  - VRoid 방식 대신 Palworld 커마 흐름으로 교체.
- `8f6011c feat(palworld): add playable customization flow`
  - 커마 선택 후 플레이 레벨로 이동하고 실제 플레이어에 적용하는 흐름 추가.
- `d18042f 커마 눈 색 적용과 플레이어 애니메이션 데이터 에셋 추가`
  - 눈 색상 적용과 플레이어 애니메이션 DataAsset 추가.
- `342b980 커마 프리뷰 조작과 원본 머티리얼 적용 수정`
  - 마우스 프리뷰 조작, 원본 머티리얼 적용 보강.
- `1a583c9 팔월드 커마 적용 검증 보강`
  - 커마 적용 검증 보강.
- `6701847 팔월드 의상 머티리얼 복구`
  - 의상 원본 머티리얼 복구 쪽 보강.

## 파일별 빠른 표

| 파일 | 역할 |
| --- | --- |
| `UEPalworldCustomizationTypes.h/cpp` | 커마 데이터 타입, Catalog DataAsset, 옵션 로드 규칙 |
| `UEPalworldCustomizationPlayerController.h/cpp` | 커마 레벨에서 프리뷰 액터와 UI 연결 |
| `UEPalworldCustomizationWidget.h/cpp` | UI 버튼, 카테고리, 팔레트, 스케일, Start 버튼 |
| `UEPalworldCustomizationPreviewActor.h/cpp` | 커마 화면 프리뷰 캐릭터 조립, 머티리얼/눈/스케일 적용 |
| `UEGameInstance.h/cpp` | 커마 결과를 다음 레벨로 넘기는 임시 저장소 |
| `UEPlayerCharacter.h/cpp` | 실제 플레이어 캐릭터에 커마 외형 적용 |
| `UEPlayerAnimationDataAsset.h/cpp` | 플레이어 시퀀스/몽타주를 DataAsset으로 관리 |
| `UEAnimInstance.h/cpp` | AnimBP가 읽을 이동 상태 계산 |

## 앞으로 손댈 때 우선순위

1. 얼굴 머티리얼이 또 깨지면 `IsEyeIrisMaterialSlot`과 `ApplyPalworldEyeMaterial`을 먼저 본다.
2. 의상 텍스처가 빠지면 `FUEPalworldCustomizationOption::LoadMesh`와 `ApplyMeshLocalMaterials`를 먼저 본다.
3. 의상이 바디와 안 맞으면 해당 의상 메쉬에 `BS_Torso_*`, `BS_Arm_*`, `BS_Leg_*` morph target이 있는지 확인한다.
4. 커마 결과가 다른 레벨로 안 넘어가면 `StartWithCurrentAppearance`, `UUEGameInstance`, `ApplyPendingPalworldAppearance` 순서로 본다.
5. 애니메이션이 안 따라오면 파츠 Skeleton이 Body와 같은지 보고, `SetLeaderPoseComponent`가 걸렸는지 확인한다.
6. UI가 안 보이거나 버튼이 안 먹으면 `WBP_PalworldCustomization` 안의 Designer 위젯 이름과 `BindDesignerInterface`가 찾는 이름이 같은지 본다.
