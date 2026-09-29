#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Environment/UEEnvironmentState.h"
#include "../Environment/UEEnvironmentWaves.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnvironmentCycleTest,"Heaven.Weather.EnvironmentCycles",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEnvironmentCycleTest::RunTest(const FString&) {
    // 자정/연말/북쪽 경계에서 반 바퀴 역행하는 문제를 잡는다.
    FUEEnvironmentState A,B; A.Enabled=B.Enabled=true;
    A.DayFraction=A.YearFraction=.99; B.DayFraction=B.YearFraction=.01;
    A.SunAzimuthDegrees=359; B.SunAzimuthDegrees=1;
    A.TideLevelM=-1; B.TideLevelM=1;
    BlendEnvironment(A,B,.5);
    TestTrue(TEXT("Midnight follows short arc"),A.DayFraction<.001 || A.DayFraction>.999);
    TestTrue(TEXT("Year wraps smoothly"),A.YearFraction<.001 || A.YearFraction>.999);
    TestTrue(TEXT("Sun crosses north"),A.SunAzimuthDegrees<.01 || A.SunAzimuthDegrees>359.99);
    TestTrue(TEXT("Tide crosses mean level"),FMath::IsNearlyZero(A.TideLevelM));
    auto* Waves=NewObject<UUEEnvironmentWaves>();Waves->HeightCm=100;
    TArray<FGerstnerWave> Parts;Waves->GenerateGerstnerWaves_Implementation(Parts);
    double Amplitude=0;for(const auto& Part:Parts)Amplitude+=Part.Amplitude;
    TestEqual(TEXT("Four native waves"),Parts.Num(),4);
    TestTrue(TEXT("Wave height is twice amplitude"),FMath::IsNearlyEqual(Amplitude*2,100.));
    return true;
}
#endif
