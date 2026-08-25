#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UEHHVCustomizationTypes.generated.h"

class USkeletalMesh;
class UMaterialInterface;
class UTexture2D;

UENUM(BlueprintType)
enum class EUEHHVGender : uint8
{
	TypeA,
	TypeB
};

UENUM(BlueprintType)
enum class EUEHHVCustomizationCategory : uint8
{
	Body,
	Head,
	Hair,
	Eyes,
	BodyEquipment
};

UENUM(BlueprintType)
enum class EUEHHVColorChannel : uint8
{
	Skin,
	Hair,
	Eye
};

UENUM(BlueprintType)
enum class EUEHHVScaleChannel : uint8
{
	TorsoSize,
	ArmSize,
	LegSize
};

USTRUCT(BlueprintType)
struct HEAVENHYPERVOICE_API FUEHHVCustomizationOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization")
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization")
	EUEHHVCustomizationCategory Category = EUEHHVCustomizationCategory::Body;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization")
	TObjectPtr<USkeletalMesh> FemaleMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization")
	TObjectPtr<USkeletalMesh> MaleMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization")
	TObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization")
	TObjectPtr<UMaterialInterface> Material = nullptr;

	USkeletalMesh* LoadMesh(EUEHHVGender Gender) const;
};

USTRUCT(BlueprintType)
struct HEAVENHYPERVOICE_API FUEHHVAppearance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization")
	EUEHHVGender Gender = EUEHHVGender::TypeA;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization")
	int32 BodyIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization")
	int32 HeadIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization")
	int32 HairIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization")
	int32 EyeIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization")
	int32 BodyEquipmentIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Color")
	FLinearColor SkinColor = FLinearColor(1.0f, 0.712f, 0.6458f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Color")
	FLinearColor HairColor = FLinearColor(0.1719f, 0.1111f, 0.0850f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Color")
	FLinearColor EyeColor = FLinearColor(0.070638f, 0.484375f, 0.243701f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Shape", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float ArmVolume = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Shape", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float TorsoVolume = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Customization|Shape", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float LegVolume = 0.0f;
};

UCLASS(BlueprintType)
class HEAVENHYPERVOICE_API UUEHHVCustomizationCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization|Options")
	TArray<FUEHHVCustomizationOption> BodyOptions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization|Options")
	TArray<FUEHHVCustomizationOption> HeadOptions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization|Options")
	TArray<FUEHHVCustomizationOption> HairOptions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization|Options")
	TArray<FUEHHVCustomizationOption> EyeOptions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization|Options")
	TArray<FUEHHVCustomizationOption> BodyEquipmentOptions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization|Colors")
	TArray<FLinearColor> SkinColors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization|Colors")
	TArray<FLinearColor> HairColors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization|Colors")
	TArray<FLinearColor> EyeColors;

	UFUNCTION(BlueprintPure, Category = "Customization")
	const TArray<FUEHHVCustomizationOption>& GetOptions(EUEHHVCustomizationCategory Category) const;

	UFUNCTION(BlueprintPure, Category = "Customization")
	int32 GetOptionCount(EUEHHVCustomizationCategory Category) const;

	UFUNCTION(BlueprintPure, Category = "Customization")
	const FUEHHVCustomizationOption& GetOption(EUEHHVCustomizationCategory Category, int32 Index) const;
};
