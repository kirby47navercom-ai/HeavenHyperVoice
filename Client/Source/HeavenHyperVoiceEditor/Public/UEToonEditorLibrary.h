#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UEToonEditorLibrary.generated.h"

class UMaterial;
class UToonProfile;
class UMaterialInstanceConstant;

/** Editor-only authoring: preserve existing material inputs and append an editable Toon BSDF. */
UCLASS()
class HEAVENHYPERVOICEEDITOR_API UUEToonEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Toon|Editor")
	static bool ConvertLegacyMaterial(UMaterial* Material, UToonProfile* Profile);
	UFUNCTION(BlueprintCallable, Category="Toon|Editor")
	static void SetInstanceProfile(UMaterialInstanceConstant* Instance, UToonProfile* Profile);
	UFUNCTION(BlueprintPure, Category="Toon|Editor")
	static UToonProfile* GetInstanceProfile(const UMaterialInstanceConstant* Instance);
};
