// YANG2_CLIENT_AUTHORITY_ONLY: 이 검사는 로컬 계산기를 링크하는 Yang2에만 둔다.
#if WITH_DEV_AUTOMATION_TESTS
#include "../Yang2/UEYang2InstanceWeather.h"
#include "../Yang2/UEYang2Environment.h"
#include "../Yang2/UEYang2WorldClock.h"
#include "../Environment/UEEnvironmentProfile.h"
#include "../Environment/UEInstanceWeatherPresentationComponent.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYang2WeatherTest, "Heaven.Weather.Yang2LocalSource",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FYang2WeatherTest::RunTest(const FString&)
{
    // 서버 프로세스 없이 같은 계산기로 강수/적설을 만들고 공통 연출 이벤트에 전달한다.
    heaven::instance::InstanceWeather Model;
    heaven::instance::InstanceWeatherProfile Profile;
    Profile.meanTemperatureC = -8;
    Profile.initialRelativeHumidityPct = 98;
    Model.initialize(2,9001,Profile);
    auto* Bridge = NewObject<UUEFieldServerBridgeComponent>();
    auto* Presentation = NewObject<UUEInstanceWeatherPresentationComponent>();
    Presentation->SetWeatherSource(Bridge);
    float PeakSnow = 0;
    for (int Step=0;Step<120;++Step)
    {
        Model.advance(1);
        const auto Snapshot = Model.snapshot();
        PeakSnow = FMath::Max(PeakSnow,Snapshot.snowDepthM);
        TestTrue(TEXT("Local water mass conserved"),FMath::Abs(Snapshot.waterBalanceErrorKgM2)<.001f);
    }
    const auto Snapshot = Model.snapshot();
    FUEInstanceWeatherState Weather;
    Weather.RoomId=Snapshot.roomId;
    Weather.TemperatureC=Snapshot.temperatureC;
    Weather.PrecipitationMmPerHour=Snapshot.precipitationMmPerHour;
    Weather.SnowDepthM=Snapshot.snowDepthM;
    Weather.Environment=MakeYang2EnvironmentState(Snapshot.environment);
    Bridge->OnInstanceWeatherChanged.Broadcast(Weather);
    TestTrue(TEXT("Cold local instance accumulates snow"),PeakSnow>0);
    // 공기가 영하라도 한낮의 지표 복사열로 눈이 녹을 수 있어요. 최종 눈 존재를 강제하지 않아요.
    // 실제 로컬 적설값이 공통 연출로 정확히 전달되는지가 이 연결 검사의 조건이에요.
    TestTrue(TEXT("Local source reaches common presentation"),FMath::IsNearlyEqual(
        Presentation->GetPresentationState().SnowCoverage,
        FMath::Clamp(Snapshot.snowDepthM/Presentation->FullSnowCoverageDepthM,0.f,1.f)));
    TestTrue(TEXT("Local clock reaches common presentation"),Presentation->GetPresentationState().Environment.Enabled);
    Presentation->SetWeatherSource(nullptr);
    // 레벨이 달라도 같은 플레이 세션이면 첫 시간 설정을 공유해요.
    auto* Session=NewObject<UGameInstance>();
    auto* FirstWorld=NewObject<UWorld>(); FirstWorld->SetGameInstance(Session);
    auto* NextWorld=NewObject<UWorld>(); NextWorld->SetGameInstance(Session);
    heaven::instance::InstanceWeatherProfile FirstClock,NextClock;
    FirstClock.environment.startHour=3; NextClock.environment.startHour=18;
    ApplyYang2WorldClockSettings(FirstWorld,FirstClock);
    const double FirstTime=GetYang2WorldRealSeconds(FirstWorld);
    ApplyYang2WorldClockSettings(NextWorld,NextClock);
    TestEqual(TEXT("World change keeps session clock settings"),NextClock.environment.startHour,FirstClock.environment.startHour);
    TestTrue(TEXT("World change keeps elapsed time"),GetYang2WorldRealSeconds(NextWorld)>=FirstTime);
    for(const TCHAR* Kind:{TEXT("Temperate"),TEXT("Coast"),TEXT("Desert")}) {
        // 실제로 저장된 DA → 로컬 어댑터 → 서버와 같은 계산식의 연결을 검사한다.
        const FString Path=FString::Printf(TEXT("/Game/VFX/Weather/DA_Environment_%s.DA_Environment_%s"),Kind,Kind);
        const auto* Asset=LoadObject<UUEEnvironmentProfile>(nullptr,*Path);
        if(!TestNotNull(TEXT("Environment data asset"),Asset)) continue;
        ApplyYang2EnvironmentProfile(nullptr,Asset,Profile);
        TestEqual(TEXT("DA temperature copied"),Profile.meanTemperatureC,Asset->MeanTemperatureC);
        TestEqual(TEXT("DA tide copied"),Profile.environment.tideAmplitudeM,Asset->TideAmplitudeM);
        TestEqual(TEXT("DA ice traction copied"),Profile.environment.iceTractionMultiplier,Asset->IceTractionMultiplier);
        TestEqual(TEXT("DA water regions copied"),static_cast<int32>(Profile.environment.waterRegions.size()),Asset->WaterRegions.Num());
        if(!Asset->WaterRegions.IsEmpty()) TestEqual(TEXT("DA sea level copied"),Profile.environment.waterRegions.front().seaLevelCm,Asset->WaterRegions[0].SeaLevelCm);
        TestEqual(TEXT("DA sand copied"),Profile.environment.sandAvailability,Asset->SandAvailability);
        TestEqual(TEXT("DA heat exchange copied"),Profile.environment.airHeatTransferWm2K,Asset->AirHeatTransferWm2K);
        TestEqual(TEXT("DA spawn rows copied"),static_cast<int32>(Profile.spawnRules.size()),Asset->SpawnRules.Num());
        Model.initialize(1,47,Profile);for(int i=0;i<600;++i)Model.advance(1);
        const auto LocalResult=Model.snapshot();
        const auto State=MakeYang2EnvironmentState(LocalResult.environment);
        TestTrue(TEXT("New environment outputs copied"),State.SurfaceHeatFluxWm2==LocalResult.environment.surfaceHeatFluxWm2 &&
            State.VisibilityMultiplier==LocalResult.environment.visibilityMultiplier && State.ImportedWaterKgM2==LocalResult.environment.importedWaterKgM2);
        TestTrue(TEXT("Local environment finite"),FMath::IsFinite(State.TideLevelM) && FMath::IsFinite(State.SandstormIntensity));
        if(Asset->SandAvailability>0)TestTrue(TEXT("Desert DA drives a storm"),State.SandstormIntensity>.01);
    }
    return true;
}
#endif
