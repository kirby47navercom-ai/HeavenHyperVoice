#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UEWaterVFXEditorLibrary.generated.h"
class UNiagaraSystem;

/** Editor-only access for authoring lightweight Niagara layers from Python. */
UCLASS()
class HEAVENHYPERVOICEEDITOR_API UUEWaterVFXEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="VFX|Editor")
	static UObject* WaterLayer(UNiagaraSystem* System, FName Name, bool bDuplicate);
	UFUNCTION(BlueprintCallable, Category="VFX|Editor")
	static bool SetWaterProperty(UObject* Object, FName Property, const FString& Value);
	UFUNCTION(BlueprintCallable, Category="VFX|Editor")
	static bool FinishWaterSystem(UNiagaraSystem* System);
};
