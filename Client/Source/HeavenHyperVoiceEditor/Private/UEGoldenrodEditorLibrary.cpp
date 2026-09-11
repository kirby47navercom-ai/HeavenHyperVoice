#include "UEGoldenrodEditorLibrary.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

int32 UUEGoldenrodEditorLibrary::RemoveSeaSurface(UStaticMesh* Mesh, float SeaHeight, bool bApply)
{
	if (!Mesh || Mesh->GetNumSourceModels() != 1 || !Mesh->GetMeshDescription(0)) return -1;
	FMeshDescription* Description = Mesh->GetMeshDescription(0);
	FStaticMeshAttributes Attributes(*Description);
	const auto Positions = Attributes.GetVertexPositions();
	const auto Slots = Attributes.GetPolygonGroupMaterialSlotNames();
	TArray<FTriangleID> SeaTriangles;
	int32 WaterTriangles = 0;
	for (const FTriangleID Triangle : Description->Triangles().GetElementIDs())
	{
		if (!Slots[Description->GetTrianglePolygonGroup(Triangle)].ToString().Contains(TEXT("Water"))) continue;
		++WaterTriangles;
		bool bAtSeaHeight = true;
		for (const FVertexID Vertex : Description->GetTriangleVertices(Triangle))
		{
			bAtSeaHeight &= FMath::IsNearlyEqual(Positions[Vertex].Z, SeaHeight, .5f);
		}
		if (bAtSeaHeight) SeaTriangles.Add(Triangle);
	}
	UE_LOG(LogTemp, Display, TEXT("%s: Water triangles=%d, sea-height triangles=%d"), *Mesh->GetName(), WaterTriangles, SeaTriangles.Num());
	if (SeaTriangles.IsEmpty() || !bApply) return SeaTriangles.Num();
	Mesh->Modify();
	Description->DeleteTriangles(SeaTriangles);
	FElementIDRemappings Remappings;
	Description->Compact(Remappings);
	Mesh->CommitMeshDescription(0);
	Mesh->Build(false);
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();
	return SeaTriangles.Num();
}
