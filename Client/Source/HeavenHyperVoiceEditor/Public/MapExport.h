#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MapExport.generated.h"

UCLASS()
class HEAVENHYPERVOICEEDITOR_API UHHVMapExportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UHHVMapExportCommandlet();

	virtual int32 Main(const FString& Params) override;
};
