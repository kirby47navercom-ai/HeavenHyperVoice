#include "UEInstanceWeatherDirector.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"

bool AUEInstanceWeatherDirector::SampleFall(const FVector& Center, bool bSnow, FVector& Start,
    FVector& Stop, FVector& Velocity, float& Lifetime, bool& bHit) const
{
    const float Angle=FMath::FRand()*2*PI;
    const float Distance=FMath::Sqrt(FMath::FRand())*FMath::Clamp(Radius,100.f,2000.f);
    Start=Center+FVector(FMath::Cos(Angle)*Distance,FMath::Sin(Angle)*Distance,FMath::Clamp(FallHeight,100.f,2000.f));
    FHitResult Roof;
    if(Trace(Start,Start+FVector(0,0,SkyTraceHeight),Roof)) return false;
    const float Radians=FMath::DegreesToRadians(State.WindDirectionDegrees);
    Velocity=FVector(FMath::Cos(Radians)*State.WindIntensity*180,
        FMath::Sin(Radians)*State.WindIntensity*180,bSnow ? -180.f : -2200.f);
    const float MaxLife=bSnow ? 6.f : 1.5f;
    FHitResult Hit; const FVector End=Start+Velocity*MaxLife;
    bHit=Trace(Start,End,Hit); Stop=bHit ? Hit.ImpactPoint : End;
    Lifetime=bHit ? Hit.Time*MaxLife : MaxLife;
    return Lifetime>=.03f && !IsExcluded(Start,Stop);
}
void AUEInstanceWeatherDirector::SpawnDrop(const FVector& Center, bool bSnow)
{
    // 충돌 표현만 적은 수의 CPU 경로로 샘플링해요. 낙하 입자는 아래 지속 이미터가 담당해요.
    if(PendingImpacts.Num()>=128) return;
    FVector Start,Stop,Velocity; float Lifetime=0; bool bHit=false;
    if(SampleFall(Center,bSnow,Start,Stop,Velocity,Lifetime,bHit) && bHit)
        PendingImpacts.Add({GetWorld()->GetTimeSeconds()+Lifetime,Start,Stop,bSnow});
}
void AUEInstanceWeatherDirector::UpdatePrecipitation(const FVector& Center,float Dt)
{
    const int32 Count=FMath::Clamp(PrecipitationColumnsPerType,1,32);
    ColumnTimer-=FMath::Max(0.f,Dt);
    const bool Refresh=ColumnTimer<=0;
    if(Refresh) ColumnTimer=.1f;
    auto Update=[&](TArray<TObjectPtr<UNiagaraComponent>>& Columns,UNiagaraSystem* System,float Intensity,bool bSnow)
    {
        if(!System) { for(auto FX:Columns) if(FX) FX->DeactivateImmediate(); return; }
        const bool New=Columns.Num()<Count;
        while(Columns.Num()<Count) {
            auto* FX=NewObject<UNiagaraComponent>(this);
            FX->bAutoActivate=false; FX->SetAutoDestroy(false);
            AddInstanceComponent(FX); FX->SetAsset(System); FX->RegisterComponent(); Columns.Add(FX);
        }
        for(int32 i=0;i<Columns.Num();++i) {
            auto* FX=Columns[i].Get();
            if(i>=Count || Intensity<.001f) { FX->DeactivateImmediate(); continue; }
            FX->SetVariableFloat(TEXT("User.SpawnRate"),FMath::Clamp(MaxDropsPerSecond,1.f,200.f)*Intensity/Count);
            if(!New && (!Refresh || i!=ColumnCursor%Count) && FX->IsActive()) continue;
            if(FX->IsActive()) if(const auto* Path=ColumnPaths.Find(FX)) {
                FHitResult Roof;
                if(Trace(Path->Start,Path->Start+FVector(0,0,SkyTraceHeight),Roof) || IsExcluded(Path->Start,Path->Stop)) {
                    FX->DeactivateImmediate(); continue;
                }
                // 눈의 긴 수명을 매 샘플링마다 끊지 않아요. 이동한 경우에만 열을 다시 배치해요.
                if(FVector::DistSquared(Path->Center,Center)<FMath::Square(FMath::Max(100.f,Radius*.25f))) {
                    const float A=FMath::DegreesToRadians(State.WindDirectionDegrees);
                    const FVector Velocity(FMath::Cos(A)*State.WindIntensity*180,FMath::Sin(A)*State.WindIntensity*180,bSnow ? -180.f : -2200.f);
                    const float Life=bSnow ? 6.f : 1.5f;
                    FHitResult Hit; const FVector End=Path->Start+Velocity*Life;
                    const bool bHit=Trace(Path->Start,End,Hit); const FVector Stop=bHit ? Hit.ImpactPoint : End;
                    if(IsExcluded(Path->Start,Stop) || (bHit && Hit.Time*Life<.03f)) { FX->DeactivateImmediate(); continue; }
                    FX->SetVariableVec3(TEXT("User.FallVelocity"),Velocity);
                    FX->SetVariableFloat(TEXT("User.FallLifetime"),bHit ? Hit.Time*Life : Life);
                    ColumnPaths[FX].Stop=Stop;
                    continue;
                }
            }
            FVector Start,Stop,Velocity; float Lifetime=0; bool bHit=false;
            if(!SampleFall(Center,bSnow,Start,Stop,Velocity,Lifetime,bHit)) { FX->DeactivateImmediate(); continue; }
            // 경로를 갱신할 때만 이 작은 열을 재시작해요. 새 지붕/차단 구역을 지나지 않게 해요.
            FX->DeactivateImmediate(); FX->SetWorldLocation(Start);
            FX->SetVariableFloat(TEXT("User.FallLifetime"),Lifetime);
            FX->SetVariableVec3(TEXT("User.FallVelocity"),Velocity); FX->Activate(true);
            ColumnPaths.Add(FX,{Center,Start,Stop});
        }
    };
    // ponytail: 낙하는 최대 64개 좁은 열, 충돌은 별도 샘플이에요. 완전한 입자별 동적 충돌은 GPU Collision 이미터로 확장해요.
    Update(RainColumns,Rain,State.RainIntensity,false);
    Update(SnowColumns,Snow,State.SnowIntensity,true);
    if(Refresh) ++ColumnCursor;
}
