#include "UEEnvironmentScene.h"
#include "UEEnvironmentWaves.h"
#include "WaterBodyOceanActor.h"
#include "WaterBodyOceanComponent.h"
float AUEEnvironmentScene::GetOceanSeaLevelCm() const {
    return Ocean ? Ocean->GetActorLocation().Z+Ocean->GetWaterBodyComponent()->GetHeightOffset() : 0;
}

void UUEEnvironmentWaves::GenerateGerstnerWaves_Implementation(TArray<FGerstnerWave>& OutWaves) const {
    // 반복 무늬가 덜 보이도록 길이가 다른 네 파도를 겹친다. 합친 진폭은 HeightCm의 절반이다.
    OutWaves.Reset();
    for(int i=0;i<4;++i) {
        FGerstnerWave Wave; const float A=FMath::DegreesToRadians(DirectionDegrees+(i-1.5f)*17);
        Wave.Direction=FVector(FMath::Cos(A),FMath::Sin(A),0);
        Wave.WaveLength=FMath::Max(50.f,LengthCm/(1+i*.6f));
        Wave.Amplitude=FMath::Max(.001f,HeightCm*.125f);
        Wave.Steepness=FMath::Clamp(Steepness,0.f,.8f); OutWaves.Add(Wave);
    }
}
void AUEEnvironmentScene::UpdateOcean() {
    if(!Ocean) return;
    // Water의 SetHeightOffset은 음수를 0으로 잘라 버린다. 기준 위치와 양의 오프셋을
    // 함께 옮겨서 썰물도 원래 배치 수위 아래로 내려가게 한다. 최종 높이는 원래 높이+조석이다.
    const float TideCm=State.Environment.TideLevelM*100;
    TideBiasCm=FMath::Max(TideBiasCm,FMath::Abs(TideCm));
    Ocean->GetRootComponent()->SetMobility(EComponentMobility::Movable);
    const FVector Base=OceanLocation-FVector(0,0,TideBiasCm);
    if(!Ocean->GetActorLocation().Equals(Base,.01)) Ocean->SetActorLocation(Base);
    const float Offset=OceanOffset+TideBiasCm+TideCm;
    // 프로필의 허용 오차와 갱신 간격으로 물 구역 재구성 비용을 제한해요.
    if(FMath::Abs(Ocean->GetWaterBodyComponent()->GetHeightOffset()-Offset)>FMath::Max(.25f,TideToleranceCm))
        Ocean->GetWaterBodyComponent()->SetHeightOffset(Offset);
    if(!RuntimeWaves) {
        RuntimeWaves=NewObject<UGerstnerWaterWaves>(this);
        WaveGenerator=NewObject<UUEEnvironmentWaves>(RuntimeWaves);
        RuntimeWaves->GerstnerWaveGenerator=WaveGenerator;
        Ocean->SetWaterWaves(RuntimeWaves);
    }
    const float Height=State.Environment.WaveHeightM*100;
    if(FMath::Abs(WaveGenerator->HeightCm-Height)>FMath::Max(.1f,WaveToleranceCm) ||
       FMath::Abs(FMath::FindDeltaAngleDegrees(WaveGenerator->DirectionDegrees,State.WindDirectionDegrees))>FMath::Max(.1f,WaveDirectionToleranceDegrees) ||
       !FMath::IsNearlyEqual(WaveGenerator->LengthCm,WaveLengthCm) || !FMath::IsNearlyEqual(WaveGenerator->Steepness,WaveSteepness)) {
        WaveGenerator->HeightCm=Height; WaveGenerator->LengthCm=WaveLengthCm;
        WaveGenerator->DirectionDegrees=State.WindDirectionDegrees; WaveGenerator->Steepness=WaveSteepness;
        RuntimeWaves->RecomputeWaves(true);
    }
}
