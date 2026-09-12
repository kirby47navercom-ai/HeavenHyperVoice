#include "UECoreCollisionSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Debug/DebugDrawComponent.h"
#include "Engine/Brush.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "LandscapeComponent.h"
#include "LandscapeDataAccess.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeProxy.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "StaticMeshResources.h"
#include <sstream>

namespace
{
bool HasSharedCollisionPreset(const UPrimitiveComponent* Component)
{
	FName Profile = Component->GetCollisionProfileName();
	bool bQueryEnabled = Component->IsQueryCollisionEnabled();
	// ULandscapeComponent is render-only (NoCollision). The instance settings belong to
	// ALandscapeProxy; collision geometry comes from its heightfield component.
	if (const auto* RenderLandscape = Cast<ULandscapeComponent>(Component))
	{
		const auto* Landscape = Cast<ALandscapeProxy>(Component->GetOwner());

		if (!Landscape)
			return false;
		Profile = Landscape->BodyInstance.GetCollisionProfileName();
		bQueryEnabled = CollisionEnabledHasQuery(Landscape->BodyInstance.GetCollisionEnabled());
		const auto* Heightfield = RenderLandscape->GetCollisionComponent();
		// Missing geometry should reach the exporter and produce an error, not vanish silently.
		if (Heightfield && (!Heightfield->IsRegistered() || !Heightfield->IsQueryCollisionEnabled()))
			return false;
	}

	return bQueryEnabled && (Profile == FName(TEXT("ServerGround")) || Profile == FName(TEXT("ServerWall")));
}

void AddTriangle(std::vector<hhv::movement::Triangle>& Data, const FVector& A, const FVector& B,
                 const FVector& C)
{
	if (FVector::CrossProduct(B - A, C - A).SizeSquared() < 1.e-8)
		return;
	auto V = [](const FVector& P)
	{
		return hhv::movement::Vec3{float(P.X), float(P.Y), float(P.Z)};
	};
	Data.push_back({V(A), V(B), V(C)});
}

void AddBoxGeometry(const UBoxComponent* Box, std::vector<hhv::movement::Triangle>& Data)
{
	const FVector Extent = Box->GetUnscaledBoxExtent();
	const FTransform& Transform = Box->GetComponentTransform();
	const FVector Corners[] = {
	    FVector(-Extent.X, -Extent.Y, -Extent.Z), FVector(Extent.X, -Extent.Y, -Extent.Z),
	    FVector(Extent.X, Extent.Y, -Extent.Z),   FVector(-Extent.X, Extent.Y, -Extent.Z),
	    FVector(-Extent.X, -Extent.Y, Extent.Z),  FVector(Extent.X, -Extent.Y, Extent.Z),
	    FVector(Extent.X, Extent.Y, Extent.Z),    FVector(-Extent.X, Extent.Y, Extent.Z)};

	FVector V[8];

	for (int32 Index = 0; Index < 8; ++Index)
	{
		V[Index] = Transform.TransformPosition(Corners[Index]);
	}

	const int32 Faces[][3] = {{0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7}, {0, 1, 5}, {0, 5, 4},
	                          {1, 2, 6}, {1, 6, 5}, {2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7}};

	for (const int32* Face : Faces)
	{
		AddTriangle(Data, V[Face[0]], V[Face[1]], V[Face[2]]);
	}
}

bool AddStaticMeshGeometry(const UStaticMeshComponent* Component, const FTransform& Transform,
                           std::vector<hhv::movement::Triangle>& Data)
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

	// Consistent winding for the portable, two-sided triangle snapshot.
	const bool bMirrored = Transform.GetDeterminant() < 0.0f;

	const FIndexArrayView Indices = LOD.IndexBuffer.GetArrayView();
	const int32 Before = static_cast<int32>(Data.size());

	for (int32 Index = 0; Index + 2 < Indices.Num(); Index += 3)
	{
		const uint32 I0 = Indices[Index];
		const uint32 I1 = Indices[Index + 1];
		const uint32 I2 = Indices[Index + 2];

		if (I0 >= PositionBuffer.GetNumVertices() || I1 >= PositionBuffer.GetNumVertices() ||
		    I2 >= PositionBuffer.GetNumVertices())
		{
			continue;
		}

		const FVector A = Transform.TransformPosition(FVector(PositionBuffer.VertexPosition(I0)));
		const FVector B = Transform.TransformPosition(FVector(PositionBuffer.VertexPosition(I1)));
		const FVector C = Transform.TransformPosition(FVector(PositionBuffer.VertexPosition(I2)));

		if (bMirrored)
		{
			AddTriangle(Data, A, B, C);
		}
		else
		{
			AddTriangle(Data, A, C, B);
		}
	}

	return static_cast<int32>(Data.size()) > Before;
}

bool AddLandscapeGeometry(ULandscapeComponent* Component, std::vector<hhv::movement::Triangle>& Data)
{
#if WITH_EDITOR
	if (Component == nullptr)
	{
		return false;
	}

	ULandscapeHeightfieldCollisionComponent* Collision = Component->GetCollisionComponent();

	if (!Collision || Collision->CollisionSizeQuads <= 0)
	{
		return false;
	}

	// Bake the simple landscape heightfield, including holes and its mip resolution.
	const bool bSimple = Collision->SimpleCollisionSizeQuads > 0;
	const int32 Quads = bSimple ? Collision->SimpleCollisionSizeQuads : Collision->CollisionSizeQuads;
	const int32 VertexCount = Quads + 1;
	const int32 SampleOffset = bSimple ? FMath::Square(Collision->CollisionSizeQuads + 1) : 0;

	if (Collision->CollisionHeightData.GetElementCount() < SampleOffset + VertexCount * VertexCount)
	{
		return false;
	}
	const uint16* Heights = static_cast<const uint16*>(Collision->CollisionHeightData.LockReadOnly());

	if (!Heights)
	{
		Collision->CollisionHeightData.Unlock();
		return false;
	}
	const uint8* Layers =
	    Collision->DominantLayerData.GetElementCount() >= SampleOffset + VertexCount * VertexCount
	        ? static_cast<const uint8*>(Collision->DominantLayerData.LockReadOnly())
	        : nullptr;
	const int32 VisibilityLayer =
	    Collision->ComponentLayerInfos.IndexOfByKey(ALandscapeProxy::VisibilityLayer);
	const float GridScale = Collision->CollisionScale * Collision->CollisionSizeQuads / Quads;
	const FTransform Transform = Collision->GetComponentTransform();

	const int32 Before = static_cast<int32>(Data.size());
	TArray<FVector> Vertices;
	Vertices.SetNum(VertexCount * VertexCount);

	for (int32 Y = 0; Y < VertexCount; ++Y)
	{
		for (int32 X = 0; X < VertexCount; ++X)
		{
			Vertices[Y * VertexCount + X] = Transform.TransformPosition(
			    FVector(X * GridScale, Y * GridScale,
			            LandscapeDataAccess::GetLocalHeight(Heights[SampleOffset + Y * VertexCount + X])));
		}
	}

	for (int32 Y = 0; Y + 1 < VertexCount; ++Y)
	{
		for (int32 X = 0; X + 1 < VertexCount; ++X)
		{
			if (Layers && VisibilityLayer != INDEX_NONE &&
			    Layers[SampleOffset + Y * VertexCount + X] == VisibilityLayer)
			{
				continue;
			}
			const FVector& A = Vertices[Y * VertexCount + X];
			const FVector& B = Vertices[Y * VertexCount + X + 1];
			const FVector& C = Vertices[(Y + 1) * VertexCount + X];
			const FVector& D = Vertices[(Y + 1) * VertexCount + X + 1];
			// Chaos::FHeightField uses the A-D diagonal.
			if (Transform.GetDeterminant() >= 0.0)
			{
				AddTriangle(Data, A, B, D);
				AddTriangle(Data, A, D, C);
			}
			else
			{
				AddTriangle(Data, A, D, B);
				AddTriangle(Data, A, C, D);
			}
		}
	}

	if (Layers)
	{
		Collision->DominantLayerData.Unlock();
	}
	Collision->CollisionHeightData.Unlock();
	return static_cast<int32>(Data.size()) > Before;
#else
	return false;
#endif
}

} // namespace

FString UUECoreCollisionSubsystem::GetCollisionRelativePath() const
{
	const FString Package = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());
	// Keep the package directories so maps with the same short name never collide.
	return Package.StartsWith(TEXT("/Game/")) ? Package.Mid(6) + TEXT(".hhvcollision") : FString();
}

FString UUECoreCollisionSubsystem::GetDefaultCollisionFile() const
{
	const FString Relative = GetCollisionRelativePath();
	return Relative.IsEmpty() ? FString()
	                          : FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() /
	                                                              TEXT("MovementCollision") / Relative);
}

bool UUECoreCollisionSubsystem::EnsureReady(const FString& File)
{
	if (bReady)
		return File.IsEmpty() || FPaths::ConvertRelativePathToFull(File) == LoadedFile;

	if (bAttempted)
		return false;
	bAttempted = true;
	// Play uses the exported artifact, including in PIE. Never rebuild a different world implicitly.
	return LoadCollisionFile(File.IsEmpty() ? GetDefaultCollisionFile() : File);
}

bool UUECoreCollisionSubsystem::LoadCollisionFile(const FString& File)
{
	FString Text;

	if (File.IsEmpty() || !FFileHelper::LoadFileToString(Text, *File))
	{
		UE_LOG(
		    LogTemp, Error,
		    TEXT(
		        "Core collision file missing: %s. In the editor use Tools > Save Shared Movement Collision, then "
		        "restart play."),
		    *File);
		return false;
	}
	std::istringstream Stream(TCHAR_TO_UTF8(*Text));

	if (!Collision.load(Stream))
	{
		UE_LOG(LogTemp, Error, TEXT("Core collision file is invalid: %s"), *File);
		return false;
	}
	bReady = bAttempted = true;
	LoadedFile = FPaths::ConvertRelativePathToFull(File);
	TriangleCount = static_cast<int32>(Collision.size());
	return true;
}

bool UUECoreCollisionSubsystem::SaveCollisionFile(const FString& File) const
{
	if (!bReady)
		return false;
	std::ostringstream Stream;

	if (!Collision.save(Stream))
		return false;
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
	return FFileHelper::SaveStringToFile(UTF8_TO_TCHAR(Stream.str().c_str()), *File,
	                                     FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool UUECoreCollisionSubsystem::RebuildFromScene()
{
#if WITH_EDITOR
	// Scene extraction happens once, before simulation. No Chaos queries are used by movers.
	TArray<UPrimitiveComponent*> Components;

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->IsA<APawn>() || *It == GetWorld()->GetDefaultBrush() || !It->GetActorEnableCollision())
			continue;
		TArray<UPrimitiveComponent*> ActorComponents;
		It->GetComponents(ActorComponents);

		for (auto* C : ActorComponents)
		{
			// PIE spawns debug renderers with primitive collision flags, but they contain no terrain.
			// Exclude their base class without silently skipping unsupported physical obstacles.
			if (C->IsA<UDebugDrawComponent>() || !C->IsRegistered() || !HasSharedCollisionPreset(C) ||
			    C->IsA<ULandscapeHeightfieldCollisionComponent>())
				continue;
			Components.Add(C);
		}
	}
	Components.Sort(
	    [](const UPrimitiveComponent& A, const UPrimitiveComponent& B)
	    {
		    return A.GetPathName() < B.GetPathName();
	    });
	std::vector<hhv::movement::Triangle> Geometry;
	bool bSupported = true;

	for (auto* C : Components)
	{
		bool bAdded = false;

		if (auto* Box = Cast<UBoxComponent>(C))
		{
			AddBoxGeometry(Box, Geometry);
			bAdded = true;
		}
		else if (auto* Landscape = Cast<ULandscapeComponent>(C))
			bAdded = AddLandscapeGeometry(Landscape, Geometry);
		else if (auto* Mesh = Cast<UStaticMeshComponent>(C))
		{
			if (auto* Instances = Cast<UInstancedStaticMeshComponent>(Mesh))
			{
				bAdded = true;

				for (int32 I = 0; I < Instances->GetInstanceCount(); ++I)
				{
					FTransform Transform;

					if (!Instances->GetInstanceTransform(I, Transform, true) ||
					    !AddStaticMeshGeometry(Mesh, Transform, Geometry))
						bAdded = false;
				}
			}
			else
				bAdded = AddStaticMeshGeometry(Mesh, Mesh->GetComponentTransform(), Geometry);
		}

		if (!bAdded)
		{
			UE_LOG(
			    LogTemp, Error,
			    TEXT(
			        "Core collision bake: unsupported blocking component %s (%s). Use a box or static mesh proxy."),
			    *C->GetPathName(), *C->GetClass()->GetName());
			bSupported = false;
		}
	}

	if (!bSupported)
		return false;

	if (Geometry.empty())
	{
		UE_LOG(
		    LogTemp, Error,
		    TEXT(
		        "No ServerGround or ServerWall collision geometry in this map. Assign Collision Presets before "
		        "exporting."));
		return false;
	}
	hhv::movement::TriangleWorld Candidate;
	Candidate.build(std::move(Geometry));

	if (!Candidate.size())
		return false;
	Collision = std::move(Candidate);
	bReady = bAttempted = true;
	TriangleCount = static_cast<int32>(Collision.size());
	LoadedFile.Empty();
	return true;
#else
	return false;
#endif
}
