#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TriangleWorld.h"
#include "UECoreCollisionSubsystem.generated.h"

/** One immutable portable collision snapshot shared by every mover in this world. */
UCLASS()
class HEAVENHYPERVOICE_API UUECoreCollisionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	bool EnsureReady(const FString& File = FString());

	UFUNCTION(BlueprintPure, Category = "Shared Movement|Collision")
	FString GetDefaultCollisionFile() const;
	FString GetCollisionRelativePath() const;

	UFUNCTION(BlueprintCallable, Category = "Shared Movement|Collision")
	bool RebuildFromScene();

	UFUNCTION(BlueprintCallable, Category = "Shared Movement|Collision")
	bool LoadCollisionFile(const FString& File);

	UFUNCTION(BlueprintCallable, Category = "Shared Movement|Collision")
	bool SaveCollisionFile(const FString& File) const;

	const hhv::movement::TriangleWorld* GetCollision() const
	{
		return bReady ? &Collision : nullptr;
	}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shared Movement|Collision")
	FString LoadedFile;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shared Movement|Collision")
	int32 TriangleCount = 0;

private:
	hhv::movement::TriangleWorld Collision;
	bool bReady = false;
	bool bAttempted = false;
};
