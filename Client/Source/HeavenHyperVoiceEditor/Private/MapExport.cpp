#include "MapExport.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "StaticMeshResources.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
#include "FileHelpers.h"
#endif

namespace
{
	constexpr TCHAR DefaultMapPackageName[] = TEXT("/Game/Level/PlayerTestLevel");
	constexpr TCHAR DefaultGroundMarker[] = TEXT("ServerGround");
	constexpr TCHAR DefaultWallMarker[] = TEXT("ServerWall");
	constexpr float DefaultWorldOriginOffset = 25600.0f;
	constexpr float DefaultCellSize = 50.0f;
	constexpr float DefaultCellHeight = 5.0f;
	constexpr float DefaultAgentRadius = 34.0f;
	constexpr float DefaultAgentHalfHeight = 88.0f;
	constexpr float DefaultAgentMaxStepHeight = 45.0f;
	constexpr float DefaultAgentMaxSlopeAngle = 44.0f;

	enum class EMapArea : uint8
	{
		Ground,
		Wall
	};

	struct FNavTriangle
	{
		FVector A = FVector::ZeroVector;
		FVector B = FVector::ZeroVector;
		FVector C = FVector::ZeroVector;
		EMapArea Area = EMapArea::Ground;
	};

	struct FMapData
	{
		FString Source;
		FBox Bounds = FBox(ForceInit);
		TArray<FNavTriangle> Triangles;
		int32 GroundTriangles = 0;
		int32 WallTriangles = 0;
		int32 UnsupportedComponents = 0;
	};

	struct FExportOptions
	{
		FString MapPackageName = DefaultMapPackageName;
		FString GroundMarker = DefaultGroundMarker;
		FString WallMarker = DefaultWallMarker;
		FString OutputPath;
		float CellSize = DefaultCellSize;
		float CellHeight = DefaultCellHeight;
		float AgentRadius = DefaultAgentRadius;
		float AgentHalfHeight = DefaultAgentHalfHeight;
		float AgentMaxStepHeight = DefaultAgentMaxStepHeight;
		float AgentMaxSlopeAngle = DefaultAgentMaxSlopeAngle;
		float WorldOriginOffset = DefaultWorldOriginOffset;
	};

	bool SameName(const FString& A, const FString& B)
	{
		return A.Equals(B, ESearchCase::IgnoreCase);
	}

	bool ContainsMarker(const FString& Text, const FString& Marker)
	{
		return !Text.IsEmpty() && Text.Contains(Marker, ESearchCase::IgnoreCase);
	}

	bool HasTag(const TArray<FName>& Tags, const FString& Marker)
	{
		for (const FName& Tag : Tags)
		{
			if (SameName(Tag.ToString(), Marker))
			{
				return true;
			}
		}
		return false;
	}

	bool MatchesMarker(const UPrimitiveComponent* Component, const FString& Marker)
	{
		if (!IsValid(Component))
		{
			return false;
		}

		if (SameName(Component->GetCollisionProfileName().ToString(), Marker))
		{
			return true;
		}

		if (HasTag(Component->ComponentTags, Marker) || ContainsMarker(Component->GetName(), Marker))
		{
			return true;
		}

		if (const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
		{
			if (const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh())
			{
				if (ContainsMarker(StaticMesh->GetName(), Marker))
				{
					return true;
				}
			}
		}

		const AActor* Owner = Component->GetOwner();
		if (Owner == nullptr)
		{
			return false;
		}

		if (HasTag(Owner->Tags, Marker) || ContainsMarker(Owner->GetName(), Marker))
		{
			return true;
		}

#if WITH_EDITOR
		return ContainsMarker(Owner->GetActorLabel(), Marker);
#else
		return false;
#endif
	}

	FString NormalizeMapPackageName(const FString& Raw)
	{
		if (Raw.IsEmpty())
		{
			return FString();
		}

		FString LongPackageName;
		if (Raw.EndsWith(TEXT(".umap")) &&
			FPackageName::TryConvertFilenameToLongPackageName(Raw, LongPackageName))
		{
			return LongPackageName;
		}

		return Raw.StartsWith(TEXT("/")) ? Raw : FString::Printf(TEXT("/Game/%s"), *Raw);
	}

	FString MakeDefaultOutputPath(const FString& MapPackageName)
	{
		FString MapName = FPackageName::GetShortName(MapPackageName);
		if (MapName.IsEmpty())
		{
			MapName = TEXT("ServerMap");
		}

		FString ServerMapsDir = FPaths::Combine(
			FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()),
			TEXT(".."),
			TEXT("Server"),
			TEXT("maps"));
		FPaths::CollapseRelativeDirectories(ServerMapsDir);
		return FPaths::Combine(ServerMapsDir, MapName + TEXT(".hhvmap"));
	}

	FVector ToServerPosition(const FVector& UnrealPosition, float WorldOriginOffset)
	{
		return FVector(
			UnrealPosition.X + WorldOriginOffset,
			UnrealPosition.Y + WorldOriginOffset,
			UnrealPosition.Z);
	}

	bool IsDegenerateTriangle(const FVector& A, const FVector& B, const FVector& C)
	{
		return FVector::CrossProduct(B - A, C - A).SizeSquared() < 1.0;
	}

	void AddTriangle(FMapData& Data, EMapArea Area, const FVector& A, const FVector& B,
	                 const FVector& C)
	{
		if (IsDegenerateTriangle(A, B, C))
		{
			return;
		}

		Data.Triangles.Add(FNavTriangle{A, B, C, Area});
		Data.Bounds += A;
		Data.Bounds += B;
		Data.Bounds += C;
		if (Area == EMapArea::Ground)
		{
			++Data.GroundTriangles;
		}
		else
		{
			++Data.WallTriangles;
		}
	}

	void AddBoxGeometry(const UBoxComponent* Box, EMapArea Area, float WorldOriginOffset,
	                    FMapData& Data)
	{
		const FVector Extent = Box->GetUnscaledBoxExtent();
		const FTransform& Transform = Box->GetComponentTransform();
		const FVector Corners[] =
		{
			FVector(-Extent.X, -Extent.Y, -Extent.Z),
			FVector( Extent.X, -Extent.Y, -Extent.Z),
			FVector( Extent.X,  Extent.Y, -Extent.Z),
			FVector(-Extent.X,  Extent.Y, -Extent.Z),
			FVector(-Extent.X, -Extent.Y,  Extent.Z),
			FVector( Extent.X, -Extent.Y,  Extent.Z),
			FVector( Extent.X,  Extent.Y,  Extent.Z),
			FVector(-Extent.X,  Extent.Y,  Extent.Z)
		};

		FVector V[8];
		for (int32 Index = 0; Index < 8; ++Index)
		{
			V[Index] = ToServerPosition(Transform.TransformPosition(Corners[Index]), WorldOriginOffset);
		}

		const int32 Faces[][3] =
		{
			{0, 2, 1}, {0, 3, 2},
			{4, 5, 6}, {4, 6, 7},
			{0, 1, 5}, {0, 5, 4},
			{1, 2, 6}, {1, 6, 5},
			{2, 3, 7}, {2, 7, 6},
			{3, 0, 4}, {3, 4, 7}
		};

		for (const int32* Face : Faces)
		{
			AddTriangle(Data, Area, V[Face[0]], V[Face[1]], V[Face[2]]);
		}
	}

	bool AddStaticMeshGeometry(const UStaticMeshComponent* Component, EMapArea Area,
	                           float WorldOriginOffset, FMapData& Data)
	{
		const UStaticMesh* StaticMesh = Component->GetStaticMesh();
		if (StaticMesh == nullptr || StaticMesh->GetRenderData() == nullptr ||
			StaticMesh->GetRenderData()->LODResources.IsEmpty())
		{
			return false;
		}

		const FStaticMeshLODResources& LOD = StaticMesh->GetRenderData()->LODResources[0];
		const FPositionVertexBuffer& PositionBuffer = LOD.VertexBuffers.PositionVertexBuffer;
		if (PositionBuffer.GetNumVertices() == 0 || LOD.IndexBuffer.GetNumIndices() < 3)
		{
			return false;
		}

		const FTransform& Transform = Component->GetComponentTransform();
		const FIndexArrayView Indices = LOD.IndexBuffer.GetArrayView();
		const int32 Before = Data.Triangles.Num();
		for (int32 Index = 0; Index + 2 < Indices.Num(); Index += 3)
		{
			const uint32 I0 = Indices[Index];
			const uint32 I1 = Indices[Index + 1];
			const uint32 I2 = Indices[Index + 2];
			if (I0 >= PositionBuffer.GetNumVertices() ||
				I1 >= PositionBuffer.GetNumVertices() ||
				I2 >= PositionBuffer.GetNumVertices())
			{
				continue;
			}

			const FVector A = ToServerPosition(
				Transform.TransformPosition(FVector(PositionBuffer.VertexPosition(I0))),
				WorldOriginOffset);
			const FVector B = ToServerPosition(
				Transform.TransformPosition(FVector(PositionBuffer.VertexPosition(I1))),
				WorldOriginOffset);
			const FVector C = ToServerPosition(
				Transform.TransformPosition(FVector(PositionBuffer.VertexPosition(I2))),
				WorldOriginOffset);
			AddTriangle(Data, Area, A, B, C);
		}

		return Data.Triangles.Num() > Before;
	}

	bool AddComponentGeometry(UPrimitiveComponent* Component, EMapArea Area,
	                          const FExportOptions& Options, FMapData& Data)
	{
		if (const UBoxComponent* Box = Cast<UBoxComponent>(Component))
		{
			AddBoxGeometry(Box, Area, Options.WorldOriginOffset, Data);
			return true;
		}

		if (const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
		{
			return AddStaticMeshGeometry(StaticMeshComponent, Area, Options.WorldOriginOffset, Data);
		}

		return false;
	}

	const TCHAR* AreaName(EMapArea Area)
	{
		return Area == EMapArea::Ground ? TEXT("ground") : TEXT("wall");
	}

	void AppendTriangleLine(FString& Text, const FNavTriangle& Triangle)
	{
		Text += FString::Printf(
			TEXT("triangle %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %s\n"),
			Triangle.A.X,
			Triangle.A.Y,
			Triangle.A.Z,
			Triangle.B.X,
			Triangle.B.Y,
			Triangle.B.Z,
			Triangle.C.X,
			Triangle.C.Y,
			Triangle.C.Z,
			AreaName(Triangle.Area));
	}

	bool SaveMapFile(const FString& Path, const FMapData& Data, const FExportOptions& Options)
	{
		FString Text;
		Text.Reserve(1024 + Data.Triangles.Num() * 128);
		Text += TEXT("# HeavenHyperVoice server nav geometry v1\n");
		Text += TEXT("# Units are Unreal uu. X/Y are shifted into server coordinates by WorldOriginOffset.\n");
		Text += TEXT("# Recast uses: cellSize cellHeight agentRadius agentHalfHeight maxStepHeight maxSlopeAngle.\n");
		Text += FString::Printf(TEXT("source %s\n"), *Data.Source);
		Text += FString::Printf(
			TEXT("settings %.3f %.3f %.3f %.3f %.3f %.3f\n"),
			Options.CellSize,
			Options.CellHeight,
			Options.AgentRadius,
			Options.AgentHalfHeight,
			Options.AgentMaxStepHeight,
			Options.AgentMaxSlopeAngle);

		const FBox Bounds = Data.Bounds.IsValid ? Data.Bounds : FBox(FVector::ZeroVector, FVector::ZeroVector);
		Text += FString::Printf(
			TEXT("bounds %.3f %.3f %.3f %.3f %.3f %.3f\n"),
			Bounds.Min.X,
			Bounds.Min.Y,
			Bounds.Min.Z,
			Bounds.Max.X,
			Bounds.Max.Y,
			Bounds.Max.Z);
		for (const FNavTriangle& Triangle : Data.Triangles)
		{
			AppendTriangleLine(Text, Triangle);
		}

		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		return FFileHelper::SaveStringToFile(Text, *Path);
	}

	bool LoadWorld(const FString& MapPackageName, UWorld*& OutWorld)
	{
		OutWorld = nullptr;

#if WITH_EDITOR
		FString MapFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(
			    MapPackageName,
			    MapFilename,
			    FPackageName::GetMapPackageExtension()))
		{
			if (UWorld* EditorWorld = UEditorLoadingAndSavingUtils::LoadMap(MapFilename))
			{
				OutWorld = EditorWorld;
				return true;
			}
		}
#endif

		UPackage* Package = LoadPackage(nullptr, *MapPackageName, LOAD_None);
		if (Package == nullptr)
		{
			return false;
		}

		Package->FullyLoad();
		OutWorld = UWorld::FindWorldInPackage(Package);
		if (OutWorld != nullptr)
		{
			return true;
		}

		for (TObjectIterator<UWorld> It; It; ++It)
		{
			if (It->GetPackage() == Package)
			{
				OutWorld = *It;
				return true;
			}
		}
		return false;
	}

	bool BuildMapData(UWorld* World, const FExportOptions& Options, FMapData& OutData)
	{
		if (World == nullptr)
		{
			return false;
		}

		OutData = {};
		OutData.Source = Options.MapPackageName;

		for (ULevel* Level : World->GetLevels())
		{
			if (Level == nullptr)
			{
				continue;
			}

			for (AActor* Actor : Level->Actors)
			{
				if (!IsValid(Actor))
				{
					continue;
				}

				TArray<UPrimitiveComponent*> Components;
				Actor->GetComponents<UPrimitiveComponent>(Components);
				for (UPrimitiveComponent* Component : Components)
				{
					if (!IsValid(Component))
					{
						continue;
					}

					const bool bGround = MatchesMarker(Component, Options.GroundMarker);
					const bool bWall = MatchesMarker(Component, Options.WallMarker);
					if (!bGround && !bWall)
					{
						continue;
					}
					if (bGround && bWall)
					{
						UE_LOG(LogTemp, Warning, TEXT("HHVMapExport: %s matches both markers; skipped"),
						       *Component->GetPathName());
						continue;
					}

					const EMapArea Area = bGround ? EMapArea::Ground : EMapArea::Wall;
					if (!AddComponentGeometry(Component, Area, Options, OutData))
					{
						++OutData.UnsupportedComponents;
						UE_LOG(LogTemp, Warning, TEXT("HHVMapExport: unsupported nav geometry component %s"),
						       *Component->GetPathName());
					}
				}
			}
		}

		return true;
	}

	void ParseOptions(const FString& Params, FExportOptions& Options)
	{
		FString MapOverride;
		if (FParse::Value(*Params, TEXT("Map="), MapOverride))
		{
			Options.MapPackageName = NormalizeMapPackageName(MapOverride);
		}
		else
		{
			Options.MapPackageName = NormalizeMapPackageName(Options.MapPackageName);
		}

		FParse::Value(*Params, TEXT("Ground="), Options.GroundMarker);
		FParse::Value(*Params, TEXT("Wall="), Options.WallMarker);
		FParse::Value(*Params, TEXT("Out="), Options.OutputPath);
		FParse::Value(*Params, TEXT("CellSize="), Options.CellSize);
		FParse::Value(*Params, TEXT("CellHeight="), Options.CellHeight);
		FParse::Value(*Params, TEXT("AgentRadius="), Options.AgentRadius);
		FParse::Value(*Params, TEXT("AgentHalfHeight="), Options.AgentHalfHeight);
		FParse::Value(*Params, TEXT("AgentMaxStep="), Options.AgentMaxStepHeight);
		FParse::Value(*Params, TEXT("AgentMaxSlope="), Options.AgentMaxSlopeAngle);

		float ConfigOriginOffset = Options.WorldOriginOffset;
		if (GConfig != nullptr &&
			GConfig->GetFloat(
				TEXT("/Script/HeavenHyperVoice.UEFieldServerBridgeComponent"),
				TEXT("WorldOriginOffset"),
				ConfigOriginOffset,
				GGameIni))
		{
			Options.WorldOriginOffset = ConfigOriginOffset;
		}
		FParse::Value(*Params, TEXT("OriginOffset="), Options.WorldOriginOffset);

		if (Options.OutputPath.IsEmpty())
		{
			Options.OutputPath = MakeDefaultOutputPath(Options.MapPackageName);
		}
	}
}

UHHVMapExportCommandlet::UHHVMapExportCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UHHVMapExportCommandlet::Main(const FString& Params)
{
	FExportOptions Options;
	ParseOptions(Params, Options);

	if (Options.MapPackageName.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("HHVMapExport: missing map package"));
		return 1;
	}

	UWorld* World = nullptr;
	if (!LoadWorld(Options.MapPackageName, World))
	{
		UE_LOG(LogTemp, Error, TEXT("HHVMapExport: cannot load %s"), *Options.MapPackageName);
		return 1;
	}

	FMapData Data;
	if (!BuildMapData(World, Options, Data))
	{
		UE_LOG(LogTemp, Error, TEXT("HHVMapExport: cannot collect nav geometry from %s"),
		       *Options.MapPackageName);
		return 1;
	}

	if (Data.GroundTriangles == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("HHVMapExport: no geometry marked as %s"),
		       *Options.GroundMarker);
	}
	if (Data.WallTriangles == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("HHVMapExport: no geometry marked as %s"),
		       *Options.WallMarker);
	}
	if (Data.Triangles.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("HHVMapExport: no nav geometry was exported"));
		return 1;
	}

	if (!SaveMapFile(Options.OutputPath, Data, Options))
	{
		UE_LOG(LogTemp, Error, TEXT("HHVMapExport: cannot write %s"), *Options.OutputPath);
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("HHVMapExport: wrote %s"), *Options.OutputPath);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("HHVMapExport: source=%s groundTriangles=%d wallTriangles=%d totalTriangles=%d unsupported=%d cell=%.1f/%.1f radius=%.1f halfHeight=%.1f maxStep=%.1f maxSlope=%.1f originOffset=%.1f"),
		*Options.MapPackageName,
		Data.GroundTriangles,
		Data.WallTriangles,
		Data.Triangles.Num(),
		Data.UnsupportedComponents,
		Options.CellSize,
		Options.CellHeight,
		Options.AgentRadius,
		Options.AgentHalfHeight,
		Options.AgentMaxStepHeight,
		Options.AgentMaxSlopeAngle,
		Options.WorldOriginOffset);
	return 0;
}
