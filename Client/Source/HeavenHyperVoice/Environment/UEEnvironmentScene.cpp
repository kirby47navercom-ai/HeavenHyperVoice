#include "UEEnvironmentScene.h"
#include "UEEnvironmentProfile.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/VolumetricCloudComponent.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "WaterBodyOceanActor.h"
#include "WaterBodyOceanComponent.h"
#include "NiagaraComponent.h"

AUEEnvironmentScene::AUEEnvironmentScene() {
    PrimaryActorTick.bCanEverTick=true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Weather=CreateDefaultSubobject<UUEInstanceWeatherPresentationComponent>(TEXT("Weather"));
    Dust=CreateDefaultSubobject<UNiagaraComponent>(TEXT("Dust"));
    Dust->SetupAttachment(RootComponent); Dust->bAutoActivate=false;
    bReplicates=false; // 서버는 상태만 전송한다. 연출 액터는 클라이언트마다 하나씩 존재한다.
}
void AUEEnvironmentScene::BeginPlay() {
    Super::BeginPlay();
    if(GetNetMode()==NM_DedicatedServer) { SetActorTickEnabled(false); return; }
    // 레벨에서 지정한 상태를 저장한다. 데이터가 끊기거나 퇴장하면 원상 복구한다.
    if(Sun) { SunRotation=Sun->GetActorRotation(); SunIntensity=Sun->GetLightComponent()->Intensity; SunColor=Sun->GetLightComponent()->GetLightColor(); }
    if(Moon) { MoonRotation=Moon->GetActorRotation(); MoonIntensity=Moon->GetLightComponent()->Intensity; MoonColor=Moon->GetLightComponent()->GetLightColor(); }
    if(Sky) SkyIntensity=Sky->GetLightComponent()->Intensity;
    if(LegacySkyDome) bSkyDomeWasHidden=LegacySkyDome->IsHidden();
    if(Fog) { FogDensity=Fog->GetComponent()->FogDensity; FogColor=Fog->GetComponent()->FogInscatteringLuminance; }
    if(Clouds) {
        OriginalCloudMaterial=Clouds->FindComponentByClass<UVolumetricCloudComponent>()->GetMaterial();
        if(OriginalCloudMaterial) { CloudMID=UMaterialInstanceDynamic::Create(OriginalCloudMaterial,this); Clouds->FindComponentByClass<UVolumetricCloudComponent>()->SetMaterial(CloudMID); }
    }
    if(Ocean) {
        OriginalWaves=Ocean->GetWaterWaves(); OceanOffset=Ocean->GetWaterBodyComponent()->GetHeightOffset();
        OceanLocation=Ocean->GetActorLocation(); OceanMobility=Ocean->GetRootComponent()->Mobility;
        TideBiasCm=Profile ? FMath::Max(0.,Profile->TideAmplitudeM)*100 : 0;
    }
    Dust->SetAsset(DustSystem);
}
void AUEEnvironmentScene::Tick(float Dt) {
    Super::Tick(Dt); State=Weather->GetPresentationState();
    if(!State.Environment.Enabled) { if(bApplied) RestoreScene(); return; }
    bApplied=true; UpdateLighting(); UpdateOcean(); UpdateDust();
}
void AUEEnvironmentScene::RestoreScene() {
    if(Sun) { Sun->SetActorRotation(SunRotation); Sun->GetLightComponent()->SetIntensity(SunIntensity); Sun->GetLightComponent()->SetLightColor(SunColor); }
    if(Moon) { Moon->SetActorRotation(MoonRotation); Moon->GetLightComponent()->SetIntensity(MoonIntensity); Moon->GetLightComponent()->SetLightColor(MoonColor); }
    if(Sky) Sky->GetLightComponent()->SetIntensity(SkyIntensity);
    if(LegacySkyDome) LegacySkyDome->SetActorHiddenInGame(bSkyDomeWasHidden);
    if(Fog) { Fog->GetComponent()->SetFogDensity(FogDensity); Fog->GetComponent()->SetFogInscatteringColor(FogColor); }
    if(Clouds && OriginalCloudMaterial) Clouds->FindComponentByClass<UVolumetricCloudComponent>()->SetMaterial(OriginalCloudMaterial);
    if(Ocean) {
        Ocean->SetActorLocation(OceanLocation);
        Ocean->GetWaterBodyComponent()->SetHeightOffset(OceanOffset); Ocean->SetWaterWaves(OriginalWaves);
        Ocean->GetRootComponent()->SetMobility(OceanMobility);
    }
    Dust->Deactivate();
    if(Parameters) GetWorld()->GetParameterCollectionInstance(Parameters)->SetScalarParameterValue(TEXT("Sandstorm"),0);
    RuntimeWaves=nullptr; WaveGenerator=nullptr; bApplied=false;
}
void AUEEnvironmentScene::EndPlay(const EEndPlayReason::Type Reason) {
    if(bApplied) RestoreScene(); Super::EndPlay(Reason);
}
