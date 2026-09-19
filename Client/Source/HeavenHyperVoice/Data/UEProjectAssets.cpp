#include "UEProjectAssets.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"

UUEProjectAssets* UUEProjectAssetSettings::GetProjectAssets()
{
	if (IsEngineExitRequested() || IsGarbageCollecting()) return nullptr;
	UUEProjectAssetSettings* Settings = GetMutableDefault<UUEProjectAssetSettings>();
	// 설정 변경도 반영하고, 로드한 연결표는 GC 중에도 유지한다.
	Settings->LoadedAssets = Settings->ProjectAssets.LoadSynchronous();
	return Settings->LoadedAssets;
}

UMaterialInterface* UUEProjectAssets::FindCharacterMaterial(const USkeletalMesh* Mesh, int32 Slot) const
{
	if (!Mesh) return nullptr;
	for (const FUECharacterMaterialBinding& Entry : CharacterMaterials)
	{
		if (Entry.MaterialSlot == Slot && Entry.Mesh.ToSoftObjectPath() == FSoftObjectPath(Mesh))
			return Entry.Material.LoadSynchronous();
	}
	return nullptr;
}

UMaterialInterface* UUEProjectAssets::FindMorphMaterial(const UMaterialInterface* Source) const
{
	if (!Source) return nullptr;
	for (const FUEMorphMaterialBinding& Entry : MorphMaterials)
	{
		if (Entry.Source.ToSoftObjectPath() == FSoftObjectPath(Source))
			return Entry.Replacement.LoadSynchronous();
	}
	return nullptr;
}
