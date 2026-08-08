// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "UEServerMapFile.generated.h"

class UPrimitiveComponent;
class UWorld;

enum class EUEServerMapCollisionLayer : uint8
{
	Ground,
	Wall
};

struct FUEServerMapObb
{
	EUEServerMapCollisionLayer Layer = EUEServerMapCollisionLayer::Ground;
	FString ProfileName;
	FVector Center = FVector::ZeroVector;
	FVector HalfExtent = FVector::ZeroVector;
	FVector AxisX = FVector::ForwardVector;
	FVector AxisY = FVector::RightVector;
	FVector AxisZ = FVector::UpVector;
	FBox Bounds = FBox(ForceInit);
};

struct FUEServerHeightCell
{
	int32 CellX = 0;
	int32 CellY = 0;
	double Height = 0.0;
	FVector Normal = FVector::UpVector;
};

struct FUEServerHeightMapData
{
	FVector2D Origin = FVector2D::ZeroVector;
	double CellSize = 50.0;
	int32 Width = 0;
	int32 Height = 0;
	TArray<FUEServerHeightCell> Cells;
};

struct FUEServerMapData
{
	FString SourceMapPackageName;
	FBox WorldBounds = FBox(ForceInit);
	TArray<FUEServerMapObb> GroundObbs;
	TArray<FUEServerMapObb> WallObbs;
	FUEServerHeightMapData HeightMap;
};

struct FUEServerMapExportOptions
{
	FString MapPackageName = TEXT("/Game/Level/PlayerTestLevel");
	FString GroundProfileName = TEXT("ServerGround");
	FString WallProfileName = TEXT("ServerWall");
	float HeightCellSize = 50.0f;
	FString OutputPath;
};

class HEAVENHYPERVOICE_API FUEServerMapFile
{
public:
	static FString NormalizeMapPackageName(const FString& RawMapPackageName);
	static FString ResolveOutputPath(const FString& MapPackageName, const FString& OutputPath);

	static bool ExportMapPackageToFile(const FUEServerMapExportOptions& Options, FString& OutErrorMessage);
	static bool ExportWorldToFile(UWorld* World, const FUEServerMapExportOptions& Options, FString& OutErrorMessage);
	static bool BuildFromWorld(UWorld* World, const FUEServerMapExportOptions& Options, FUEServerMapData& OutMapData, FString& OutErrorMessage);

	static bool SaveToFile(const FString& FilePath, const FUEServerMapData& MapData, FString& OutErrorMessage);
	static bool LoadFromFile(const FString& FilePath, FUEServerMapData& OutMapData, FString& OutErrorMessage);
	static FBox CalculateObbBounds(const FUEServerMapObb& Obb);

private:
	static bool TryBuildObb(UPrimitiveComponent* PrimitiveComponent, EUEServerMapCollisionLayer Layer, FUEServerMapObb& OutObb);
	static void IncludeBounds(FBox& TargetBounds, const FBox& BoundsToInclude);
	static UWorld* LoadWorld(const FString& MapPackageName, FString& OutErrorMessage);
};

UCLASS()
class HEAVENHYPERVOICE_API UUEServerMapExportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UUEServerMapExportCommandlet();

	virtual int32 Main(const FString& Params) override;
};
