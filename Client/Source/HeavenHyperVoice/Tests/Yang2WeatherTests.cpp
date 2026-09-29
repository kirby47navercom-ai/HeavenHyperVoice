// YANG2_CLIENT_AUTHORITY_ONLY: 이 검사는 로컬 계산기를 링크하는 Yang2에만 둔다.
#if WITH_DEV_AUTOMATION_TESTS
#include "../Yang2/UEYang2InstanceWeather.h"
#include "../Yang2/UEYang2Environment.h"
#include "../Environment/UEEnvironmentProfile.h"
#include "../Environment/UEInstanceWeatherPresentationComponent.h"
#include "Misc/AutomationTest.h"

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
    Weather.TemperatureC=Snapshot.temperatureC;
    Weather.PrecipitationMmPerHour=Snapshot.precipitationMmPerHour;
    Weather.SnowDepthM=Snapshot.snowDepthM;
    Weather.Environment=MakeYang2EnvironmentState(Snapshot.environment);
    Bridge->OnInstanceWeatherChanged.Broadcast(Weather);
    TestTrue(TEXT("Cold local instance accumulates snow"),PeakSnow>0);
    TestTrue(TEXT("Local source reaches common presentation"),Presentation->GetPresentationState().SnowCoverage>0);
    TestTrue(TEXT("Local clock reaches common presentation"),Presentation->GetPresentationState().Environment.Enabled);
    Presentation->SetWeatherSource(nullptr);
    for(const TCHAR* Kind:{TEXT("Temperate"),TEXT("Coast"),TEXT("Desert")}) {
        // 실제로 저장된 DA → 로컬 어댑터 → 서버와 같은 계산식의 연결을 검사한다.
        const FString Path=FString::Printf(TEXT("/Game/VFX/Weather/DA_Environment_%s.DA_Environment_%s"),Kind,Kind);
        const auto* Asset=LoadObject<UUEEnvironmentProfile>(nullptr,*Path);
        if(!TestNotNull(TEXT("Environment data asset"),Asset)) continue;
        ApplyYang2EnvironmentProfile(nullptr,Asset,Profile);
        TestEqual(TEXT("DA temperature copied"),Profile.meanTemperatureC,Asset->MeanTemperatureC);
        TestEqual(TEXT("DA tide copied"),Profile.environment.tideAmplitudeM,Asset->TideAmplitudeM);
        TestEqual(TEXT("DA sand copied"),Profile.environment.sandAvailability,Asset->SandAvailability);
        Model.initialize(1,47,Profile);for(int i=0;i<600;++i)Model.advance(1);
        const auto State=MakeYang2EnvironmentState(Model.snapshot().environment);
        TestTrue(TEXT("Local environment finite"),FMath::IsFinite(State.TideLevelM) && FMath::IsFinite(State.SandstormIntensity));
        if(Asset->SandAvailability>0)TestTrue(TEXT("Desert DA drives a storm"),State.SandstormIntensity>.01);
    }
    return true;
}
#endif
