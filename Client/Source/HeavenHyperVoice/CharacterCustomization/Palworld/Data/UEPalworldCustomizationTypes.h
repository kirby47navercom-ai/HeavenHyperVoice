#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UEPalworldCustomizationTypes.generated.h"

class USkeletalMesh;
class UMaterialInterface;
class UTexture2D;

UENUM(BlueprintType)
enum class EUEPalworldGender : uint8
{
	TypeA,
	TypeB
};

UENUM(BlueprintType)
enum class EUEPalworldCustomizationCategory : uint8
{
	Body,
	Head,
	Hair,
	Eyes,
	BodyEquipment,
	HeadEquipment
};

UENUM(BlueprintType)
enum class EUEPalworldColorChannel : uint8
{
	Skin,
	Hair,
	Eye,
	BodyEquipment,
	HeadEquipment
};

UENUM(BlueprintType)
enum class EUEPalworldScaleChannel : uint8
{
	Height,
	HeadSize,
	BodyWidth
};

USTRUCT(BlueprintType)
struct HEAVENHYPERVOICE_API FUEPalworldCustomizationOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palworld")
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palworld")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palworld")
	EUEPalworldCustomizationCategory Category = EUEPalworldCustomizationCategory::Body;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palworld")
	TObjectPtr<USkeletalMesh> FemaleMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palworld")
	TObjectPtr<USkeletalMesh> MaleMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palworld")
	TObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palworld")
	TObjectPtr<UMaterialInterface> Material = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palworld|Attachment")
	bool bIsHairAttachAccessory = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palworld|Attachment")
	FName FemaleAttachSocket;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palworld|Attachment")
	FName MaleAttachSocket;

	USkeletalMesh* LoadMesh(EUEPalworldGender Gender) const;
	FName GetAttachSocket(EUEPalworldGender Gender) const;
};

USTRUCT(BlueprintType)
struct HEAVENHYPERVOICE_API FUEPalworldAppearance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palworld")
	EUEPalworldGender Gender = EUEPalworldGender::TypeA;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palworld")
	int32 BodyIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palworld")
	int32 HeadIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palworld")
	int32 HairIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palworld")
	int32 EyeIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palworld")
	int32 BodyEquipmentIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palworld")
	int32 HeadEquipmentIndex = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palworld|Color")
	FLinearColor SkinColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palworld|Color")
	FLinearColor HairColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palworld|Color")
	FLinearColor EyeColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palworld|Color")
	FLinearColor BodyEquipmentColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palworld|Color")
	FLinearColor HeadEquipmentColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palworld|Shape", meta = (ClampMin = "0.75", ClampMax = "1.25"))
	float HeightScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palworld|Shape", meta = (ClampMin = "0.75", ClampMax = "1.25"))
	float HeadScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palworld|Shape", meta = (ClampMin = "0.75", ClampMax = "1.25"))
	float BodyWidthScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palworld|Shape", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float ArmVolume = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palworld|Shape", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float TorsoVolume = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palworld|Shape", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float LegVolume = 0.0f;
};

UCLASS(BlueprintType)
class HEAVENHYPERVOICE_API UUEPalworldCustomizationCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palworld|Options")
	TArray<FUEPalworldCustomizationOption> BodyOptions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palworld|Options")
	TArray<FUEPalworldCustomizationOption> HeadOptions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palworld|Options")
	TArray<FUEPalworldCustomizationOption> HairOptions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palworld|Options")
	TArray<FUEPalworldCustomizationOption> EyeOptions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palworld|Options")
	TArray<FUEPalworldCustomizationOption> BodyEquipmentOptions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palworld|Options")
	TArray<FUEPalworldCustomizationOption> HeadEquipmentOptions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palworld|Colors")
	TArray<FLinearColor> SkinColors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palworld|Colors")
	TArray<FLinearColor> HairColors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palworld|Colors")
	TArray<FLinearColor> EyeColors;

	UFUNCTION(BlueprintPure, Category = "Palworld")
	const TArray<FUEPalworldCustomizationOption>& GetOptions(EUEPalworldCustomizationCategory Category) const;

	UFUNCTION(BlueprintPure, Category = "Palworld")
	int32 GetOptionCount(EUEPalworldCustomizationCategory Category) const;

	UFUNCTION(BlueprintPure, Category = "Palworld")
	const FUEPalworldCustomizationOption& GetOption(EUEPalworldCustomizationCategory Category, int32 Index) const;
};
