#include "UEWaterVFXEditorLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitterBase.h"
#include "UObject/UnrealType.h"

UObject* UUEWaterVFXEditorLibrary::WaterLayer(UNiagaraSystem* System, FName Name, bool bDuplicate)
{
	if (!System || System->GetEmitterHandles().IsEmpty()) return nullptr;
	for (auto& Handle : System->GetEmitterHandles())
		if (Handle.GetName() == Name) return Handle.GetEmitterBase();
	if (bDuplicate)
	{
		const FNiagaraEmitterHandle Source = System->GetEmitterHandles()[0];
		return System->DuplicateEmitterHandle(Source, Name).GetEmitterBase();
	}
	auto& Handle = System->GetEmitterHandles()[0];
	Handle.SetName(Name, *System);
	return Handle.GetEmitterBase();
}

bool UUEWaterVFXEditorLibrary::SetWaterProperty(UObject* Object, FName Name, const FString& Value)
{
	FProperty* Property = Object ? FindFProperty<FProperty>(Object->GetClass(), Name) : nullptr;
	if (!Property) return false;
	Object->Modify();
	if (!Property->ImportText_Direct(*Value, Property->ContainerPtrToValuePtr<void>(Object), Object, PPF_None)) return false;
	FPropertyChangedEvent Event(Property);
	Object->PostEditChangeProperty(Event);
	return true;
}

bool UUEWaterVFXEditorLibrary::FinishWaterSystem(UNiagaraSystem* System)
{
	if (!System) return false;
	for (auto& Handle : System->GetEmitterHandles())
		if (auto* Emitter = Handle.GetEmitterBase()) Emitter->PostEditChange();
	System->PostEditChange();
	System->RequestCompile(false);
	System->WaitForCompilationComplete(true, false);
	System->MarkPackageDirty();
	return System->IsReadyToRun();
}
