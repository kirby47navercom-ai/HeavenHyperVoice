#if WITH_DEV_AUTOMATION_TESTS
#include "../Environment/UEInstanceWeatherDirector.h"
#include "../Environment/UEWeatherExclusionVolume.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/Material.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInstanceWeatherTest, "Heaven.Weather.PresentationAndExposure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInstanceWeatherTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false)
        .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &Init);
    auto* Owner = World->SpawnActor<AActor>();
    auto* Bridge = NewObject<UUEFieldServerBridgeComponent>(Owner);
    auto* Presentation = NewObject<UUEInstanceWeatherPresentationComponent>(Owner);
    Presentation->RegisterComponent();
    Presentation->SetWeatherSource(Bridge);
    FUEInstanceWeatherState Weather;
    Weather.TemperatureC = -2;
    Weather.PrecipitationMmPerHour = Presentation->HeavyPrecipitationMmPerHour;
    Weather.SnowDepthM = Presentation->FullSnowCoverageDepthM;
    Weather.WindDirectionDegrees = 359;
    Bridge->OnInstanceWeatherChanged.Broadcast(Weather);
    TestEqual(TEXT("Cold precipitation is snow"), Presentation->GetPresentationState().SnowIntensity, 1.f);
    TestEqual(TEXT("Full accumulated snow"), Presentation->GetPresentationState().SnowCoverage, 1.f);
    Weather.TemperatureC = 2;
    Weather.WindDirectionDegrees = 1;
    Bridge->OnInstanceWeatherChanged.Broadcast(Weather);
    Presentation->TickComponent(1, LEVELTICK_All, nullptr);
    const auto Visual = Presentation->GetPresentationState();
    TestTrue(TEXT("Warm snapshot transitions to rain"), Visual.RainIntensity > Visual.SnowIntensity);
    TestTrue(TEXT("Wind crosses north by short arc"), Visual.WindDirectionDegrees < 2 || Visual.WindDirectionDegrees > 358);
    Presentation->SetWeatherSource(nullptr);
    TestEqual(TEXT("Detach clears rainfall"), Presentation->GetPresentationState().RainIntensity, 0.f);

    auto* Director = World->SpawnActor<AUEInstanceWeatherDirector>();
    auto* Landscape = World->SpawnActor<ALandscape>();
    Landscape->LandscapeMaterial = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/InstanceMap/Plain/Landscape/M_Landscape_GrassSoil.M_Landscape_GrassSoil"));
    auto* Render = NewObject<ULandscapeComponent>(Landscape);
    auto* Collision = NewObject<ULandscapeHeightfieldCollisionComponent>(Landscape);
    Collision->SetRenderComponent(Render);
    FHitResult LandscapeHit(Landscape,Collision,FVector::ZeroVector,FVector::UpVector);
    TestTrue(TEXT("Landscape collision resolves managed ground material"),Director->UsesWeatherMaterial(LandscapeHit));
    auto* Roof = World->SpawnActor<AActor>();
    auto* Box = NewObject<UBoxComponent>(Roof);
    Roof->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(100,100,10));
    Box->SetCollisionProfileName(TEXT("BlockAll"));
    Box->RegisterComponent();
    Box->SetWorldLocation(FVector(0,0,300));
    FHitResult Hit;
    TestTrue(TEXT("Roof intercepts precipitation"), Director->Trace(FVector(0,0,800), FVector::ZeroVector, Hit));
    TestTrue(TEXT("Impact is roof top"), FMath::IsNearlyEqual(Hit.ImpactPoint.Z,310.,1.));
    TestFalse(TEXT("Open sky outside roof"), Director->Trace(FVector(500,0,0), FVector(500,0,1000), Hit));
    Roof->Tags.Add(TEXT("WeatherWater"));
    Director->Trace(FVector(0,0,800), FVector::ZeroVector, Hit);
    TestTrue(TEXT("Tagged water uses ripple"), Director->IsWater(Hit));
    auto* Exclusion = World->SpawnActor<AUEWeatherExclusionVolume>();
    Exclusion->Bounds->SetBoxExtent(FVector(100,50,50));
    Exclusion->SetActorLocationAndRotation(FVector(500,0,100),FRotator(0,45,0));
    Director->Exclusions.Add(Exclusion);
    TestTrue(TEXT("Rotated exclusion blocks crossing"), Director->IsExcluded(FVector(500,0,500),FVector(500,0,0)));
    TestFalse(TEXT("Outside path stays exposed"), Director->IsExcluded(FVector(1000,0,500),FVector(1000,0,0)));
    Exclusion->SetActorScale3D(FVector(2,3,1));
    FLinearColor RowX,RowY,RowZ;
    Exclusion->GetMaterialRows(RowX,RowY,RowZ);
    const FVector Sample = Exclusion->Bounds->GetComponentTransform().TransformPosition(FVector(50,10,20));
    auto Dot = [&](const FLinearColor& Row) { return Sample.X*Row.R+Sample.Y*Row.G+Sample.Z*Row.B+Row.A; };
    TestTrue(TEXT("Material box matches rotated scaled bounds"),
        FMath::IsNearlyEqual(Dot(RowX),.5,1.e-5) && FMath::IsNearlyEqual(Dot(RowY),.2,1.e-5) &&
        FMath::IsNearlyEqual(Dot(RowZ),.4,1.e-5));
    Director->WetImpactMaterial = UMaterial::GetDefaultMaterial(MD_DeferredDecal);
    Director->MaxWetMarks = 1;
    Director->SpawnImpact(Hit,false);
    TestEqual(TEXT("Water does not receive wet decal"),Director->WetMarks.Num(),0);
    Roof->Tags.Reset();
    Director->SpawnImpact(Hit,true);
    TestEqual(TEXT("Snow does not receive wet decal"),Director->WetMarks.Num(),0);
    Director->SpawnImpact(Hit,false);
    Director->SpawnImpact(Hit,false);
    TestEqual(TEXT("Wet impact budget is bounded"),Director->WetMarks.Num(),1);
    if (!Director->WetMarks.IsEmpty())
        TestTrue(TEXT("Wet mark follows hit component"),Director->WetMarks[0]->GetAttachParent()==Box);
    Director->PendingImpacts.Add({1,FVector::ZeroVector,FVector::ZeroVector,false});
    Director->ClearWeather();
    TestEqual(TEXT("Disconnect cancels queued impacts"), Director->PendingImpacts.Num(),0);
    TestEqual(TEXT("Disconnect removes wet marks"),Director->WetMarks.Num(),0);
    World->DestroyWorld(false);
    return true;
}
#endif
