// YANG2_CLIENT_AUTHORITY_ONLY: 이 검사는 로컬 계산기를 링크하는 Yang2에만 둔다.
#if WITH_DEV_AUTOMATION_TESTS
#include "../Yang2/UEYang2InstanceWeather.h"
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
    Bridge->OnInstanceWeatherChanged.Broadcast(Weather);
    TestTrue(TEXT("Cold local instance accumulates snow"),PeakSnow>0);
    TestTrue(TEXT("Local source reaches common presentation"),Presentation->GetPresentationState().SnowCoverage>0);
    Presentation->SetWeatherSource(nullptr);
    return true;
}
#endif
