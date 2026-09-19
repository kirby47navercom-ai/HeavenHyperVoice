#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "../Pokemon/UEPokemonSpeciesData.h"
#include "UEProjectAssets.generated.h"

class AGameModeBase;
class AUEGachaMachine;
class AUEPokemonCharacter;
class UUEGachaPool;
class UUEGachaStudioWidget;
class UUEFieldPartyWidget;
class UUEPokemonSpeciesCatalog;
class UUserWidget;
class UWorld;

USTRUCT(BlueprintType)
struct FUECharacterMaterialBinding
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
	TSoftObjectPtr<USkeletalMesh> Mesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials", meta = (ClampMin = "0"))
	int32 MaterialSlot = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> Material;
};

USTRUCT(BlueprintType)
struct FUEMorphMaterialBinding
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> Source;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> Replacement;
};

USTRUCT(BlueprintType)
struct FUELevelCollisionAsset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	TSoftObjectPtr<UWorld> Level;

	/** Content/MovementCollision 기준 상대 파일명. 서버에도 같은 파일을 배포한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	FString CollisionFile;
};

/** 공통 에셋 연결표. 폴더나 파일 이름을 바꾸면 에디터에서 참조를 갱신한다. */
UCLASS(BlueprintType)
class HEAVENHYPERVOICE_API UUEProjectAssets : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Levels")
	TSoftObjectPtr<UWorld> FrontendLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Levels")
	TSoftObjectPtr<UWorld> FieldLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Levels")
	TSoftObjectPtr<UWorld> InstanceLevel;

	/** 비어 있으면 필드 맵의 World Settings를 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Levels")
	TSoftClassPtr<AGameModeBase> FieldGameModeClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon")
	TSoftObjectPtr<UUEPokemonSpeciesCatalog> SpeciesCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon")
	TSoftClassPtr<AUEPokemonCharacter> PokemonClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSoftClassPtr<UUEFieldPartyWidget> PartyWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSoftClassPtr<UUserWidget> LoadingScreenWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TMap<EUEPokemonType, TSoftObjectPtr<UTexture2D>> TypeIcons;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gacha")
	TSoftClassPtr<UUEGachaStudioWidget> GachaWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gacha")
	TSoftClassPtr<AUEGachaMachine> GachaMachineClass;

	/** 각 표의 DisplayOrder가 서버 뽑기 타입과 일치해야 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gacha")
	TArray<TSoftObjectPtr<UUEGachaPool>> GachaPools;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gacha")
	TSoftObjectPtr<UWorld> GachaStudioLevel;

	/** 맵 폴더를 바꿔도 서버와 공유하는 충돌 파일은 그대로 지정할 수 있다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (TitleProperty = "Level"))
	TArray<FUELevelCollisionAsset> LevelCollisions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character", meta = (TitleProperty = "Mesh"))
	TArray<FUECharacterMaterialBinding> CharacterMaterials;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character", meta = (TitleProperty = "Source"))
	TArray<FUEMorphMaterialBinding> MorphMaterials;

	UMaterialInterface* FindCharacterMaterial(const USkeletalMesh* Mesh, int32 Slot) const;
	UMaterialInterface* FindMorphMaterial(const UMaterialInterface* Source) const;
};

/** 프로젝트 설정에서 공통 데이터 에셋 하나만 지정한다. */
UCLASS(Config = Game, DefaultConfig, BlueprintType, meta = (DisplayName = "공통 에셋 연결"))
class HEAVENHYPERVOICE_API UUEProjectAssetSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Assets")
	TSoftObjectPtr<UUEProjectAssets> ProjectAssets;

	UFUNCTION(BlueprintPure, Category = "HHV|Assets")
	static UUEProjectAssets* GetProjectAssets();

private:
	UPROPERTY(Transient)
	TObjectPtr<UUEProjectAssets> LoadedAssets;
};
