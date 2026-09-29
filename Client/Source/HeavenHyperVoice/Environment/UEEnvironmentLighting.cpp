#include "UEEnvironmentScene.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"

#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

void AUEEnvironmentScene::UpdateLighting() {
    const auto& E=State.Environment;
    if(LegacySkyDome) LegacySkyDome->SetActorHiddenInGame(true);
    // 태양이 수평선 아래로 내려가면 황혼을 거쳐 어두워진다. 시각에서 낮/밤을 즉시 토글하지 않는다.
    const float Day=FMath::SmoothStep(-6.f,12.f,static_cast<float>(E.SunElevationDegrees));
    const float Noon=FMath::SmoothStep(0.f,30.f,static_cast<float>(E.SunElevationDegrees));
    if(Sun) {
        Sun->SetActorRotation(FRotator(-E.SunElevationDegrees,E.SunAzimuthDegrees,0));
        Sun->GetLightComponent()->SetIntensity(DayLux*Day*FMath::Lerp(1.f,.45f,State.CloudAmount));
        Sun->GetLightComponent()->SetLightColor(FMath::Lerp(DawnColor,DayColor,Noon));
    }
    if(Moon) {
        Moon->SetActorRotation(FRotator(E.SunElevationDegrees,E.SunAzimuthDegrees+180,0));
        Moon->GetLightComponent()->SetIntensity(NightLux*(1-Day));
        Moon->GetLightComponent()->SetLightColor(NightColor);
    }
    if(Sky) Sky->GetLightComponent()->SetIntensity(SkyIntensity*FMath::Lerp(.06f,1.f,Day));
    if(Fog) {
        const float DustAmount=E.SandstormIntensity;
        Fog->GetComponent()->SetFogDensity(FMath::Lerp(ClearFog,DenseFog,FMath::Max(State.FogDensity,DustAmount)));
        Fog->GetComponent()->SetFogInscatteringColor(FMath::Lerp(FMath::Lerp(NightColor,FogColor,Day),DustColor,DustAmount));
    }
    if(CloudMID) {
        Clouds->FindComponentByClass<UVolumetricCloudComponent>()->SetMaterial(CloudMID);
        CloudMID->SetScalarParameterValue(CloudDensityParameter,State.CloudAmount*CloudDensityScale);
    }
}
