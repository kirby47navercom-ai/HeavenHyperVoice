// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "../Data/UEPrimaryDataAsset.h"
#include "UEAssetManager.generated.h"


DECLARE_DELEGATE_TwoParams(FAsyncLoadCompletedDelegate, const FName&/*AssetName or Label*/, UObject*/*LoadedAsset*/);

/**
 * 
 */
UCLASS()
class HEAVENHYPERVOICE_API UUEAssetManager : public UAssetManager
{
	GENERATED_BODY()
	
public:
	UUEAssetManager();

	static UUEAssetManager& Get();

public:
	static void Initialize();

	template<typename AssetType>
	static AssetType* GetAssetByName(const FName& AssetName);

	static void LoadSyncByPath(const FSoftObjectPath& AssetPath);
	static void LoadSyncByName(const FName& AssetName);
	static void LoadSyncByLabel(const FName& Label);

	static void LoadAsyncByPath(const FSoftObjectPath& AssetPath, FAsyncLoadCompletedDelegate CompletedDelegate = FAsyncLoadCompletedDelegate());
	static void LoadAsyncByName(const FName& AssetName, FAsyncLoadCompletedDelegate CompletedDelegate = FAsyncLoadCompletedDelegate());

	static void ReleaseByPath(const FSoftObjectPath& AssetPath);
	static void ReleaseByName(const FName& AssetName);
	static void ReleaseByLabel(const FName& Label);
	static void ReleaseAll();

private:
	void LoadPreloadAssets();
	void AddLoadedAsset(const FName& AssetName, const UObject* Asset);

private:
	UPROPERTY()
	TObjectPtr<UUEPrimaryDataAsset> LoadedAssetData;

	UPROPERTY()
	TMap<FName, TObjectPtr<const UObject>> NameToLoadedAsset;

	//FCriticalSection LoadedAssetsCritical;
};

template<typename AssetType>
AssetType* UUEAssetManager::GetAssetByName(const FName& AssetName)
{
	UUEPrimaryDataAsset* AssetData = Get().LoadedAssetData;
	check(AssetData);

	AssetType* LoadedAsset = nullptr;
	const FSoftObjectPath& AssetPath = AssetData->GetAssetPathByName(AssetName);
	if (AssetPath.IsValid())
	{
		LoadedAsset = Cast<AssetType>(AssetPath.ResolveObject());
		if (LoadedAsset == nullptr)
		{
			LoadedAsset = Cast<AssetType>(AssetPath.TryLoad());
		}
	}
	return LoadedAsset;
}