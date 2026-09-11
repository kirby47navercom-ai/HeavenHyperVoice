#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PokemonAnimationEditorLibrary.generated.h"

class UBlendSpace;

/** Python authoring support for the editor-only BlendSpace resampling API. */
UCLASS()
class HEAVENHYPERVOICEEDITOR_API UPokemonAnimationEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category="Pokemon|Editor")
	static int32 GetBlendSpaceSegmentCount(UBlendSpace* BlendSpace);
	UFUNCTION(BlueprintPure, Category="Pokemon|Editor")
	static bool NeedsBlendSpaceRebuild(UBlendSpace* BlendSpace);
	UFUNCTION(BlueprintCallable, Category="Pokemon|Editor")
	static bool RebuildBlendSpace(UBlendSpace* BlendSpace);
};
