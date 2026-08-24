// Fill out your copyright notice in the Description page of Project Settings.

#include "UEServerMapFile.h"

#include "UEServerHeightMapBuilder.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
#include "FileHelpers.h"
#endif

namespace
{
	constexpr TCHAR DefaultGroundProfileName[] = TEXT("ServerGround");
	constexpr TCHAR DefaultWallProfileName[] = TEXT("ServerWall");
	constexpr float DefaultWorldBoundsPadding = 1000.0f;

	bool IsValidBounds(const FBox& Bounds)
	{
		return
			Bounds.IsValid &&
			Bounds.Min.X <= Bounds.Max.X &&
			Bounds.Min.Y <= Bounds.Max.Y &&
			Bounds.Min.Z <= Bounds.Max.Z;
	}

	FString LayerToLinePrefix(EUEServerMapCollisionLayer Layer)
	{
		return Layer == EUEServerMapCollisionLayer::Ground ? TEXT("ground_obb") : TEXT("wall_obb");
	}

	bool MatchesProfile(const FName& ProfileName, const FString& ExpectedProfileName)
	{
		return ProfileName.ToString().Equals(ExpectedProfileName, ESearchCase::IgnoreCase);
	}

	FString MakeDefaultOutputPath(const FString& MapPackageName)
	{
		FString MapName = FPackageName::GetShortName(MapPackageName);
		if (MapName.IsEmpty())
		{
			MapName = TEXT("ServerMap");
		}
		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("ServerMaps"),
			MapName + TEXT(".hhvservermap")
		);
	}

	UWorld* FindWorldInLoadedPackage(UPackage* Package)
	{
		if (!Package)
		{
			return nullptr;
		}

		if (UWorld* World = UWorld::FindWorldInPackage(Package))
		{
			return World;
		}

		for (TObjectIterator<UWorld> It; It; ++It)
		{
			UWorld* World = *It;
			if (World && World->GetOutermost() == Package)
			{
				return World;
			}
		}

		return nullptr;
	}

	void AddBoundsLine(FString& FileContents, const FBox& Bounds)
	{
		FileContents += FString::Printf(
			TEXT("bounds %.3f %.3f %.3f %.3f %.3f %.3f\n"),
			Bounds.Min.X,
			Bounds.Min.Y,
			Bounds.Min.Z,
			Bounds.Max.X,
			Bounds.Max.Y,
			Bounds.Max.Z
		);
	}

	void AddObbLine(FString& FileContents, const FUEServerMapObb& Obb)
	{
		FileContents += FString::Printf(
			TEXT("%s %s %.3f %.3f %.3f %.3f %.3f %.3f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f\n"),
			*LayerToLinePrefix(Obb.Layer),
			*Obb.ProfileName,
			Obb.Center.X,
			Obb.Center.Y,
			Obb.Center.Z,
			Obb.HalfExtent.X,
			Obb.HalfExtent.Y,
			Obb.HalfExtent.Z,
			Obb.AxisX.X,
			Obb.AxisX.Y,
			Obb.AxisX.Z,
			Obb.AxisY.X,
			Obb.AxisY.Y,
			Obb.AxisY.Z,
			Obb.AxisZ.X,
			Obb.AxisZ.Y,
			Obb.AxisZ.Z
		);
	}

	void AddHeightMapLines(FString& FileContents, const FUEServerHeightMapData& HeightMap)
	{
		FileContents += FString::Printf(
			TEXT("heightmap %.3f %.3f %.3f %d %d\n"),
			HeightMap.Origin.X,
			HeightMap.Origin.Y,
			HeightMap.CellSize,
			HeightMap.Width,
			HeightMap.Height
		);

		for (const FUEServerHeightCell& Cell : HeightMap.Cells)
		{
			FileContents += FString::Printf(
				TEXT("height_cell %d %d %.3f %.6f %.6f %.6f\n"),
				Cell.CellX,
				Cell.CellY,
				Cell.Height,
				Cell.Normal.X,
				Cell.Normal.Y,
				Cell.Normal.Z
			);
		}
	}

	bool ReadDouble(const TArray<FString>& Tokens, int32 Index, double& OutValue)
	{
		if (!Tokens.IsValidIndex(Index))
		{
			return false;
		}

		OutValue = FCString::Atod(*Tokens[Index]);
		return true;
	}

	bool ReadFloatVector(const TArray<FString>& Tokens, int32 StartIndex, FVector& OutVector)
	{
		double X = 0.0;
		double Y = 0.0;
		double Z = 0.0;
		if (!ReadDouble(Tokens, StartIndex, X) || !ReadDouble(Tokens, StartIndex + 1, Y) || !ReadDouble(Tokens, StartIndex + 2, Z))
		{
			return false;
		}

		OutVector = FVector(X, Y, Z);
		return true;
	}

	bool ParseBoundsLine(const TArray<FString>& Tokens, FBox& OutBounds)
	{
		FVector Min;
		FVector Max;
		if (Tokens.Num() < 7 || !ReadFloatVector(Tokens, 1, Min) || !ReadFloatVector(Tokens, 4, Max))
		{
			return false;
		}

		OutBounds = FBox(Min, Max);
		return true;
	}

	bool ParseHeightMapLine(const TArray<FString>& Tokens, FUEServerHeightMapData& OutHeightMap)
	{
		double OriginX = 0.0;
		double OriginY = 0.0;
		double CellSize = 0.0;
		if (
			Tokens.Num() < 6 ||
			!ReadDouble(Tokens, 1, OriginX) ||
			!ReadDouble(Tokens, 2, OriginY) ||
			!ReadDouble(Tokens, 3, CellSize)
		)
		{
			return false;
		}

		OutHeightMap.Origin = FVector2D(OriginX, OriginY);
		OutHeightMap.CellSize = CellSize;
		OutHeightMap.Width = FCString::Atoi(*Tokens[4]);
		OutHeightMap.Height = FCString::Atoi(*Tokens[5]);
		return true;
	}

	bool ParseHeightCellLine(const TArray<FString>& Tokens, FUEServerHeightCell& OutCell)
	{
		double Height = 0.0;
		FVector Normal;
		if (
			Tokens.Num() < 7 ||
			!ReadDouble(Tokens, 3, Height) ||
			!ReadFloatVector(Tokens, 4, Normal)
		)
		{
			return false;
		}

		OutCell.CellX = FCString::Atoi(*Tokens[1]);
		OutCell.CellY = FCString::Atoi(*Tokens[2]);
		OutCell.Height = Height;
		OutCell.Normal = Normal.GetSafeNormal();
		return true;
	}

	bool ParseObbLine(const TArray<FString>& Tokens, EUEServerMapCollisionLayer Layer, FUEServerMapObb& OutObb)
	{
		FVector Center;
		FVector HalfExtent;
		FVector AxisX;
		FVector AxisY;
		FVector AxisZ;
		if (
			Tokens.Num() < 17 ||
			!ReadFloatVector(Tokens, 2, Center) ||
			!ReadFloatVector(Tokens, 5, HalfExtent) ||
			!ReadFloatVector(Tokens, 8, AxisX) ||
			!ReadFloatVector(Tokens, 11, AxisY) ||
			!ReadFloatVector(Tokens, 14, AxisZ)
		)
		{
			return false;
		}

		OutObb.Layer = Layer;
		OutObb.ProfileName = Tokens[1];
		OutObb.Center = Center;
		OutObb.HalfExtent = HalfExtent;
		OutObb.AxisX = AxisX.GetSafeNormal();
		OutObb.AxisY = AxisY.GetSafeNormal();
		OutObb.AxisZ = AxisZ.GetSafeNormal();
		OutObb.Bounds = FUEServerMapFile::CalculateObbBounds(OutObb);
		return true;
	}
}

FString FUEServerMapFile::NormalizeMapPackageName(const FString& RawMapPackageName)
{
	if (RawMapPackageName.IsEmpty())
	{
		return FString();
	}

	FString LongPackageName;
	if (
		RawMapPackageName.EndsWith(TEXT(".umap")) &&
		FPackageName::TryConvertFilenameToLongPackageName(RawMapPackageName, LongPackageName)
	)
	{
		return LongPackageName;
	}

	if (RawMapPackageName.StartsWith(TEXT("/")))
	{
		return RawMapPackageName;
	}

	return FString::Printf(TEXT("/Game/%s"), *RawMapPackageName);
}

FString FUEServerMapFile::ResolveOutputPath(const FString& MapPackageName, const FString& OutputPath)
{
	return OutputPath.IsEmpty() ? MakeDefaultOutputPath(MapPackageName) : OutputPath;
}

bool FUEServerMapFile::ExportMapPackageToFile(const FUEServerMapExportOptions& Options, FString& OutErrorMessage)
{
	FUEServerMapExportOptions NormalizedOptions = Options;
	NormalizedOptions.MapPackageName = NormalizeMapPackageName(Options.MapPackageName);
	if (NormalizedOptions.MapPackageName.IsEmpty())
	{
		OutErrorMessage = TEXT("Server map export failed: a map package must be provided.");
		return false;
	}
	NormalizedOptions.OutputPath = ResolveOutputPath(NormalizedOptions.MapPackageName, Options.OutputPath);

	UWorld* World = LoadWorld(NormalizedOptions.MapPackageName, OutErrorMessage);
	if (!World)
	{
		return false;
	}

	return ExportWorldToFile(World, NormalizedOptions, OutErrorMessage);
}

bool FUEServerMapFile::ExportWorldToFile(UWorld* World, const FUEServerMapExportOptions& Options, FString& OutErrorMessage)
{
	FUEServerMapData MapData;
	if (!BuildFromWorld(World, Options, MapData, OutErrorMessage))
	{
		return false;
	}

	return SaveToFile(ResolveOutputPath(Options.MapPackageName, Options.OutputPath), MapData, OutErrorMessage);
}

bool FUEServerMapFile::BuildFromWorld(UWorld* World, const FUEServerMapExportOptions& Options, FUEServerMapData& OutMapData, FString& OutErrorMessage)
{
	if (!World)
	{
		OutErrorMessage = TEXT("Server map export failed: world is null.");
		return false;
	}

	OutMapData = FUEServerMapData{};
	OutMapData.SourceMapPackageName = NormalizeMapPackageName(Options.MapPackageName);
	if (OutMapData.SourceMapPackageName.IsEmpty())
	{
		OutMapData.SourceMapPackageName = World->GetOutermost()->GetName();
	}

	for (ULevel* Level : World->GetLevels())
	{
		if (!Level)
		{
			continue;
		}

		for (AActor* Actor : Level->Actors)
		{
			if (!IsValid(Actor))
			{
				continue;
			}

			TArray<UPrimitiveComponent*> PrimitiveComponents;
			Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
			for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
			{
				if (!IsValid(PrimitiveComponent))
				{
					continue;
				}

				const FName ProfileName = PrimitiveComponent->GetCollisionProfileName();
				const bool bIsGround = MatchesProfile(ProfileName, Options.GroundProfileName);
				const bool bIsWall = MatchesProfile(ProfileName, Options.WallProfileName);
				if (!bIsGround && !bIsWall)
				{
					continue;
				}

				FUEServerMapObb Obb;
				const EUEServerMapCollisionLayer Layer = bIsGround ? EUEServerMapCollisionLayer::Ground : EUEServerMapCollisionLayer::Wall;
				if (!TryBuildObb(PrimitiveComponent, Layer, Obb))
				{
					continue;
				}

				if (bIsGround)
				{
					OutMapData.GroundObbs.Add(Obb);
				}
				else
				{
					OutMapData.WallObbs.Add(Obb);
				}

				IncludeBounds(OutMapData.WorldBounds, Obb.Bounds);
			}
		}
	}

	if (!IsValidBounds(OutMapData.WorldBounds))
	{
		OutMapData.WorldBounds = FBox(
			FVector(-5000.0, -5000.0, -1000.0),
			FVector(5000.0, 5000.0, 3000.0)
		);
	}
	else
	{
		OutMapData.WorldBounds = OutMapData.WorldBounds.ExpandBy(DefaultWorldBoundsPadding);
	}

	FUEServerHeightMapBuilder::BuildFromGroundObbs(
		OutMapData.GroundObbs,
		Options.HeightCellSize,
		OutMapData.HeightMap
	);

	if (OutMapData.GroundObbs.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("ServerMapExport: no components use collision profile %s"), *Options.GroundProfileName);
	}
	if (OutMapData.WallObbs.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("ServerMapExport: no components use collision profile %s"), *Options.WallProfileName);
	}

	return true;
}

bool FUEServerMapFile::SaveToFile(const FString& FilePath, const FUEServerMapData& MapData, FString& OutErrorMessage)
{
	FString FileContents;
	FileContents += TEXT("# HeavenHyperVoice server map v1\n");
	FileContents += TEXT("# format: source mapPackageName\n");
	FileContents += TEXT("# format: bounds minX minY minZ maxX maxY maxZ\n");
	FileContents += TEXT("# format: ground_obb profile centerX centerY centerZ halfX halfY halfZ axisXX axisXY axisXZ axisYX axisYY axisYZ axisZX axisZY axisZZ\n");
	FileContents += TEXT("# format: wall_obb profile centerX centerY centerZ halfX halfY halfZ axisXX axisXY axisXZ axisYX axisYY axisYZ axisZX axisZY axisZZ\n");
	FileContents += TEXT("# format: heightmap originX originY cellSize width height\n");
	FileContents += TEXT("# format: height_cell cellX cellY height normalX normalY normalZ\n");
	FileContents += FString::Printf(TEXT("source %s\n"), *MapData.SourceMapPackageName);
	AddBoundsLine(FileContents, MapData.WorldBounds);

	for (const FUEServerMapObb& Obb : MapData.GroundObbs)
	{
		AddObbLine(FileContents, Obb);
	}
	for (const FUEServerMapObb& Obb : MapData.WallObbs)
	{
		AddObbLine(FileContents, Obb);
	}

	AddHeightMapLines(FileContents, MapData.HeightMap);

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);
	if (!FFileHelper::SaveStringToFile(FileContents, *FilePath))
	{
		OutErrorMessage = FString::Printf(TEXT("Server map export failed: could not write %s"), *FilePath);
		return false;
	}

	return true;
}

bool FUEServerMapFile::LoadFromFile(const FString& FilePath, FUEServerMapData& OutMapData, FString& OutErrorMessage)
{
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *FilePath))
	{
		OutErrorMessage = FString::Printf(TEXT("Server map load failed: could not read %s"), *FilePath);
		return false;
	}

	OutMapData = FUEServerMapData{};

	for (const FString& RawLine : Lines)
	{
		FString Line = RawLine.TrimStartAndEnd();
		if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
		{
			continue;
		}

		TArray<FString> Tokens;
		Line.ParseIntoArrayWS(Tokens);
		if (Tokens.IsEmpty())
		{
			continue;
		}

		const FString& Type = Tokens[0];
		if (Type == TEXT("source") && Tokens.Num() >= 2)
		{
			OutMapData.SourceMapPackageName = Tokens[1];
		}
		else if (Type == TEXT("bounds"))
		{
			if (!ParseBoundsLine(Tokens, OutMapData.WorldBounds))
			{
				OutErrorMessage = FString::Printf(TEXT("Server map load failed: invalid bounds line in %s"), *FilePath);
				return false;
			}
		}
		else if (Type == TEXT("heightmap"))
		{
			if (!ParseHeightMapLine(Tokens, OutMapData.HeightMap))
			{
				OutErrorMessage = FString::Printf(TEXT("Server map load failed: invalid heightmap line in %s"), *FilePath);
				return false;
			}
		}
		else if (Type == TEXT("height_cell"))
		{
			FUEServerHeightCell Cell;
			if (!ParseHeightCellLine(Tokens, Cell))
			{
				OutErrorMessage = FString::Printf(TEXT("Server map load failed: invalid height_cell line in %s"), *FilePath);
				return false;
			}

			OutMapData.HeightMap.Cells.Add(Cell);
		}
		else if (Type == TEXT("ground_obb") || Type == TEXT("wall_obb"))
		{
			const EUEServerMapCollisionLayer Layer = Type == TEXT("ground_obb")
				? EUEServerMapCollisionLayer::Ground
				: EUEServerMapCollisionLayer::Wall;

			FUEServerMapObb Obb;
			if (!ParseObbLine(Tokens, Layer, Obb))
			{
				OutErrorMessage = FString::Printf(TEXT("Server map load failed: invalid OBB line in %s"), *FilePath);
				return false;
			}

			if (Layer == EUEServerMapCollisionLayer::Ground)
			{
				OutMapData.GroundObbs.Add(Obb);
			}
			else
			{
				OutMapData.WallObbs.Add(Obb);
			}
		}
	}

	return true;
}

bool FUEServerMapFile::TryBuildObb(UPrimitiveComponent* PrimitiveComponent, EUEServerMapCollisionLayer Layer, FUEServerMapObb& OutObb)
{
	if (!IsValid(PrimitiveComponent))
	{
		return false;
	}

	const FTransform ComponentTransform = PrimitiveComponent->GetComponentTransform();
	OutObb.Layer = Layer;
	OutObb.ProfileName = PrimitiveComponent->GetCollisionProfileName().ToString();
	OutObb.AxisX = ComponentTransform.GetUnitAxis(EAxis::X).GetSafeNormal();
	OutObb.AxisY = ComponentTransform.GetUnitAxis(EAxis::Y).GetSafeNormal();
	OutObb.AxisZ = ComponentTransform.GetUnitAxis(EAxis::Z).GetSafeNormal();

	if (const UBoxComponent* BoxComponent = Cast<UBoxComponent>(PrimitiveComponent))
	{
		OutObb.Center = BoxComponent->GetComponentLocation();
		OutObb.HalfExtent = BoxComponent->GetScaledBoxExtent();
		OutObb.Bounds = CalculateObbBounds(OutObb);
		return true;
	}

	if (const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(PrimitiveComponent))
	{
		if (const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh())
		{
			const FBox LocalBox = StaticMesh->GetBoundingBox();
			const FVector AbsScale = ComponentTransform.GetScale3D().GetAbs();
			OutObb.Center = ComponentTransform.TransformPosition(LocalBox.GetCenter());
			OutObb.HalfExtent = LocalBox.GetExtent() * AbsScale;
			OutObb.Bounds = CalculateObbBounds(OutObb);
			return true;
		}
	}

	const FBox WorldBox = PrimitiveComponent->CalcBounds(ComponentTransform).GetBox();
	if (!WorldBox.IsValid)
	{
		return false;
	}

	OutObb.Center = WorldBox.GetCenter();
	OutObb.HalfExtent = WorldBox.GetExtent();
	OutObb.AxisX = FVector::ForwardVector;
	OutObb.AxisY = FVector::RightVector;
	OutObb.AxisZ = FVector::UpVector;
	OutObb.Bounds = CalculateObbBounds(OutObb);
	return true;
}

FBox FUEServerMapFile::CalculateObbBounds(const FUEServerMapObb& Obb)
{
	FBox Bounds(ForceInit);
	const double Signs[] = { -1.0, 1.0 };
	for (const double SignX : Signs)
	{
		for (const double SignY : Signs)
		{
			for (const double SignZ : Signs)
			{
				Bounds +=
					Obb.Center +
					Obb.AxisX * Obb.HalfExtent.X * SignX +
					Obb.AxisY * Obb.HalfExtent.Y * SignY +
					Obb.AxisZ * Obb.HalfExtent.Z * SignZ;
			}
		}
	}

	return Bounds;
}

void FUEServerMapFile::IncludeBounds(FBox& TargetBounds, const FBox& BoundsToInclude)
{
	if (!IsValidBounds(BoundsToInclude))
	{
		return;
	}

	if (!IsValidBounds(TargetBounds))
	{
		TargetBounds = BoundsToInclude;
		return;
	}

	TargetBounds += BoundsToInclude;
}

UWorld* FUEServerMapFile::LoadWorld(const FString& MapPackageName, FString& OutErrorMessage)
{
	const FString NormalizedMapPackageName = NormalizeMapPackageName(MapPackageName);
	UWorld* World = nullptr;

#if WITH_EDITOR
	const FString MapFilename = FPackageName::LongPackageNameToFilename(
		NormalizedMapPackageName,
		FPackageName::GetMapPackageExtension()
	);
	World = UEditorLoadingAndSavingUtils::LoadMap(MapFilename);
#endif

	if (!World)
	{
		UPackage* Package = LoadPackage(nullptr, *NormalizedMapPackageName, LOAD_None);
		if (!Package)
		{
			OutErrorMessage = FString::Printf(TEXT("Server map export failed: could not load map package %s"), *NormalizedMapPackageName);
			return nullptr;
		}

		Package->FullyLoad();
		World = FindWorldInLoadedPackage(Package);
	}

	if (!World)
	{
		OutErrorMessage = FString::Printf(TEXT("Server map export failed: could not find world in %s"), *NormalizedMapPackageName);
	}

	return World;
}

UUEServerMapExportCommandlet::UUEServerMapExportCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UUEServerMapExportCommandlet::Main(const FString& Params)
{
	FUEServerMapExportOptions Options;
	Options.GroundProfileName = DefaultGroundProfileName;
	Options.WallProfileName = DefaultWallProfileName;

	FParse::Value(*Params, TEXT("Map="), Options.MapPackageName);
	FParse::Value(*Params, TEXT("GroundProfile="), Options.GroundProfileName);
	FParse::Value(*Params, TEXT("WallProfile="), Options.WallProfileName);
	FParse::Value(*Params, TEXT("HeightCellSize="), Options.HeightCellSize);
	FParse::Value(*Params, TEXT("Out="), Options.OutputPath);

	Options.MapPackageName = FUEServerMapFile::NormalizeMapPackageName(Options.MapPackageName);
	Options.OutputPath = FUEServerMapFile::ResolveOutputPath(Options.MapPackageName, Options.OutputPath);
	Options.HeightCellSize = FMath::Max(Options.HeightCellSize, 1.0f);

	FString ErrorMessage;
	if (!FUEServerMapFile::ExportMapPackageToFile(Options, ErrorMessage))
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *ErrorMessage);
		return 1;
	}

	FUEServerMapData ExportedMapData;
	if (!FUEServerMapFile::LoadFromFile(Options.OutputPath, ExportedMapData, ErrorMessage))
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *ErrorMessage);
		return 1;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("ServerMapExport: wrote %s"),
		*Options.OutputPath
	);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("ServerMapExport: ground OBBs=%d wall OBBs=%d height cells=%d heightmap=%dx%d cell=%.3f"),
		ExportedMapData.GroundObbs.Num(),
		ExportedMapData.WallObbs.Num(),
		ExportedMapData.HeightMap.Cells.Num(),
		ExportedMapData.HeightMap.Width,
		ExportedMapData.HeightMap.Height,
		ExportedMapData.HeightMap.CellSize
	);

	return 0;
}
