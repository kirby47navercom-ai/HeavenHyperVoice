#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Commandlets/Commandlet.h"
#include "CoreCollisionExport.generated.h"

UCLASS()
class HEAVENHYPERVOICEEDITOR_API UHHVCoreCollisionExportLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

  public:
	/** Save the current map's ServerGround/ServerWall snapshot to both deployment directories. */

	UFUNCTION(BlueprintCallable, Category = "HHV|Map Export")
	static bool ExportCurrentMapCollision(bool bSaveMap = true);
};

UCLASS()
class HEAVENHYPERVOICEEDITOR_API UHHVCoreCollisionExportCommandlet : public UCommandlet
{
	GENERATED_BODY()

  public:
	UHHVCoreCollisionExportCommandlet();
	virtual int32 Main(const FString &Params) override;
};
