#include "UEEnvironmentScene.h"
#include "NiagaraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Engine/World.h"
void AUEEnvironmentScene::UpdateDust() {
    // 카메라 근처에만 먼지를 표현한다. 입자 하나하나를 서버가 전송하지 않는다.
    if(auto* PC=GetWorld()->GetFirstPlayerController()) {
        FVector Position; FRotator Rotation; PC->GetPlayerViewPoint(Position,Rotation);
        Dust->SetWorldLocation(Position);
        Dust->SetWorldRotation(FRotator(0,State.WindDirectionDegrees,0));
    }
    if(Parameters) {
        auto* MPC=GetWorld()->GetParameterCollectionInstance(Parameters);
        MPC->SetScalarParameterValue(TEXT("Sandstorm"),State.Environment.SandstormIntensity);
        MPC->SetScalarParameterValue(TEXT("Season"),State.Environment.YearFraction);
        MPC->SetScalarParameterValue(TEXT("DayFraction"),State.Environment.DayFraction);
    }
    if(DustSystem && State.Environment.SandstormIntensity>.001) { if(!Dust->IsActive()) Dust->Activate(); }
    else if(Dust->IsActive()) Dust->Deactivate();
}
