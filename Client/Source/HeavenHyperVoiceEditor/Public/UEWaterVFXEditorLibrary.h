#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UEWaterVFXEditorLibrary.generated.h"
class UNiagaraSystem;
class UNiagaraEmitter;

/** Editor-only access for authoring lightweight Niagara layers from Python. */
UCLASS()
class HEAVENHYPERVOICEEDITOR_API UUEWaterVFXEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** 에디터 제작 전용: 경로별 수명/속도를 Niagara User 파라미터에 연결한다. */
	UFUNCTION(BlueprintCallable, Category="VFX|Editor")
	static bool BindWeatherParticle(UNiagaraSystem* System, UObject* Initialize, UObject* Velocity);
	UFUNCTION(BlueprintCallable, Category="VFX|Editor")
	static bool AddStandardVFXLayer(UNiagaraSystem* System, UNiagaraEmitter* Emitter);
	UFUNCTION(BlueprintCallable, Category="VFX|Editor")
	static UObject* WaterLayer(UNiagaraSystem* System, FName Name, bool bDuplicate);
	UFUNCTION(BlueprintCallable, Category="VFX|Editor")
	static bool SetWaterProperty(UObject* Object, FName Property, const FString& Value);
	UFUNCTION(BlueprintCallable, Category="VFX|Editor")
	static bool FinishWaterSystem(UNiagaraSystem* System);
};
