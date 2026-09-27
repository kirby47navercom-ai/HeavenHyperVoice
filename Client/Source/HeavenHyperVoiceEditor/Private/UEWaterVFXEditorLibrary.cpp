#include "UEWaterVFXEditorLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitterBase.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "UObject/UnrealType.h"
#include "Stateless/NiagaraStatelessDistribution.h"

bool UUEWaterVFXEditorLibrary::BindWeatherParticle(UNiagaraSystem* System, UObject* Initialize, UObject* Velocity)
{
    if (!System || !Initialize || !Velocity) return false;
    auto* LifeProperty = FindFProperty<FStructProperty>(Initialize->GetClass(),TEXT("LifetimeDistribution"));
    auto* VelocityProperty = FindFProperty<FStructProperty>(Velocity->GetClass(),TEXT("LinearVelocityDistribution"));
    if (!LifeProperty || !VelocityProperty ||
        LifeProperty->Struct != FNiagaraDistributionRangeFloat::StaticStruct() ||
        VelocityProperty->Struct != FNiagaraDistributionRangeVector3::StaticStruct()) return false;
    FNiagaraVariable Life(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.FallLifetime"));
    FNiagaraVariable Speed(FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.FallVelocity"));
    System->GetExposedParameters().AddParameter(Life);
    System->GetExposedParameters().AddParameter(Speed);
    System->GetExposedParameters().SetParameterValue(1.f,Life);
    System->GetExposedParameters().SetParameterValue(FVector3f(0,0,-600),Speed);
    auto* L = LifeProperty->ContainerPtrToValuePtr<FNiagaraDistributionRangeFloat>(Initialize);
    auto* V = VelocityProperty->ContainerPtrToValuePtr<FNiagaraDistributionRangeVector3>(Velocity);
    L->Mode = ENiagaraDistributionMode::Binding;
    L->ParameterBinding = Life;
    V->Mode = ENiagaraDistributionMode::Binding;
    V->ParameterBinding = Speed;
    return true;
}

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

bool UUEWaterVFXEditorLibrary::AddStandardVFXLayer(UNiagaraSystem* System, UNiagaraEmitter* Emitter)
{
	if (!System || !Emitter) return false;
	return FNiagaraEditorUtilities::AddEmitterToSystem(*System, *Emitter, Emitter->GetExposedVersion().VersionGuid).IsValid();
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
