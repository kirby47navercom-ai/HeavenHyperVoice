#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UEGoldenrodEditorLibrary.generated.h"

class UStaticMesh;

UCLASS()
class HEAVENHYPERVOICEEDITOR_API UUEGoldenrodEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Counts/removes only horizontal Water faces at sea height. Returns triangle count, or -1 on invalid input. */
	UFUNCTION(BlueprintCallable, Category="Goldenrod|Editor")
	static int32 RemoveSeaSurface(UStaticMesh* Mesh, float SeaHeight = -175.f, bool bApply = true);
};
