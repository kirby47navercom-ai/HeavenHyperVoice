#include "UEInstanceWeatherDirector.h"
#include "UEWeatherExclusionVolume.h"
#include "Components/DecalComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "LandscapeComponent.h"
#include "LandscapeHeightfieldCollisionComponent.h"

AUEInstanceWeatherDirector::AUEInstanceWeatherDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Presentation = CreateDefaultSubobject<UUEInstanceWeatherPresentationComponent>(TEXT("Presentation"));
    Presentation->bFindWeatherSourceAutomatically = false;
    bReplicates = false; // 화면 연출만 담당한다. 권위 있는 날씨는 브릿지에서 받는다.
}
void AUEInstanceWeatherDirector::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() == NM_DedicatedServer) { SetActorTickEnabled(false); return; }
    if (SurfaceMaterial) SurfaceMID = UMaterialInstanceDynamic::Create(SurfaceMaterial,this);
}
bool AUEInstanceWeatherDirector::Trace(const FVector& Start, const FVector& End, FHitResult& Hit) const
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(WeatherSurface),true,this);
    Query.bReturnFaceIndex = true;
    if (auto* PC = GetWorld()->GetFirstPlayerController()) Query.AddIgnoredActor(PC->GetPawn());
    return GetWorld()->LineTraceSingleByChannel(Hit,Start,End,WeatherCollisionChannel,Query);
}
bool AUEInstanceWeatherDirector::IsExcluded(const FVector& Start, const FVector& End) const
{
    for (const auto& Volume : Exclusions)
        if (Volume.IsValid() && Volume->IntersectsPath(Start,End)) return true;
    return false;
}
bool AUEInstanceWeatherDirector::IsWater(const FHitResult& Hit) const
{
    return (Hit.GetActor() && Hit.GetActor()->ActorHasTag(WaterSurfaceTag)) ||
        (Hit.GetComponent() && Hit.GetComponent()->ComponentHasTag(WaterSurfaceTag));
}
bool AUEInstanceWeatherDirector::UsesWeatherMaterial(const FHitResult& Hit) const
{
    auto* Component = Hit.GetComponent();
    if (!Component) return false;
    int32 Section = 0;
    auto* Material = Component->GetMaterialFromCollisionFaceIndex(Hit.FaceIndex,Section);
    // Landscape의 쿼리는 렌더 컴포넌트가 아닌 높이필드 충돌 컴포넌트에 맞는다.
    if (const auto* LandscapeCollision = Cast<ULandscapeHeightfieldCollisionComponent>(Component))
        if (auto* Render = LandscapeCollision->GetRenderComponent()) Material = Render->GetLandscapeMaterial();
    if (!Material) Material = Component->GetMaterial(0);
    float Enabled = 0;
    // 값이 0이어도 함수가 관리하는 표면이다. 사용자가 끈 날씨를 대체 데칼로 다시 켜지 않는다.
    return Material && Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("WeatherMaterialEnabled")),Enabled);
}
void AUEInstanceWeatherDirector::SpawnImpact(const FHitResult& Hit, bool bSnow)
{
    const bool Water = IsWater(Hit);
    const bool Wall = Hit.ImpactNormal.Z < .55f;
    UNiagaraSystem* Effect = bSnow ? SnowImpact : (Water ? WaterRipple : (Wall && WallSplash ? WallSplash : RainImpact));
    auto Spawn = [&](UNiagaraSystem* System)
    {
        if (System) UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),System,
            Hit.ImpactPoint+Hit.ImpactNormal*2,FRotationMatrix::MakeFromZ(Hit.ImpactNormal).Rotator(),
            FVector::OneVector,true,true,ENCPoolMethod::AutoRelease,false);
    };
    Spawn(Effect);
    if (bSnow && !Water && !Wall) Spawn(SnowChunks);
    WetMarks.RemoveAll([](const auto& Mark) { return !Mark.IsValid(); });
    if (bSnow || Water || !WetImpactMaterial || !Hit.GetComponent() ||
        WetMarks.Num() >= FMath::Clamp(MaxWetMarks,0,64)) return;
    const float Size = Wall ? 3.f : 7.f;
    const float Life = FMath::Clamp(WetMarkLifetime,.2f,10.f);
    // 맞은 컴포넌트에 붙여 움직이는 물체에서 자국만 공중에 남지 않게 한다.
    auto* Mark = UGameplayStatics::SpawnDecalAttached(WetImpactMaterial,FVector(1,Size,Size),
        Hit.GetComponent(),NAME_None,Hit.ImpactPoint+Hit.ImpactNormal*.2f,(-Hit.ImpactNormal).Rotation(),
        EAttachLocation::KeepWorldPosition,Life);
    if (Mark) { Mark->SetFadeOut(Life*.5f,Life*.5f,false); WetMarks.Add(Mark); }
}
void AUEInstanceWeatherDirector::UpdateExclusionParameters()
{
    if (!Parameters) return;
    auto* MPC = GetWorld()->GetParameterCollectionInstance(Parameters);
    if (!MPC) return;
    // ponytail: 보이는 주변 실내 8개를 머티리얼에 전달한다. 더 필요하면 공간 마스크 RT로 확장한다.
    Exclusions.Sort([this](const auto& A,const auto& B)
    {
        const double DA=A.IsValid() ? FVector::DistSquared(A->GetActorLocation(),GetActorLocation()) : DBL_MAX;
        const double DB=B.IsValid() ? FVector::DistSquared(B->GetActorLocation(),GetActorLocation()) : DBL_MAX;
        return DA<DB;
    });
    int32 Count=0;
    for (const auto& Volume : Exclusions)
    {
        if (!Volume.IsValid() || Count>=8) continue;
        FLinearColor X,Y,Z; Volume->GetMaterialRows(X,Y,Z);
        MPC->SetVectorParameterValue(*FString::Printf(TEXT("Exclude%dX"),Count),X);
        MPC->SetVectorParameterValue(*FString::Printf(TEXT("Exclude%dY"),Count),Y);
        MPC->SetVectorParameterValue(*FString::Printf(TEXT("Exclude%dZ"),Count),Z);
        ++Count;
    }
    MPC->SetScalarParameterValue(TEXT("ExclusionCount"),Count);
}
void AUEInstanceWeatherDirector::SpawnDrop(const FVector& Center, bool bSnow)
{
    UNiagaraSystem* System = bSnow ? Snow : Rain;
    if (!System || PendingImpacts.Num() >= 256) return;
    const float Angle = FMath::FRand()*2*PI;
    const float Distance = FMath::Sqrt(FMath::FRand())*FMath::Clamp(Radius,100.f,2000.f);
    const FVector Start = Center + FVector(FMath::Cos(Angle)*Distance,FMath::Sin(Angle)*Distance,
        FMath::Clamp(FallHeight,100.f,2000.f));
    // 각 생성 위치에서 하늘을 검사하므로 출입구 안에 있어도 바깥의 비는 보인다.
    FHitResult Roof;
    if (Trace(Start,Start+FVector(0,0,SkyTraceHeight),Roof)) return;
    const float Radians = FMath::DegreesToRadians(State.WindDirectionDegrees);
    const float FallSpeed = bSnow ? 180.f : 2200.f;
    const FVector Velocity(FMath::Cos(Radians)*State.WindIntensity*180,
        FMath::Sin(Radians)*State.WindIntensity*180,-FallSpeed);
    const float MaxLife = bSnow ? 6.f : 1.5f;
    FHitResult Hit;
    const FVector End = Start + Velocity*MaxLife;
    const bool bHit = Trace(Start,End,Hit);
    const FVector Stop = bHit ? Hit.ImpactPoint : End;
    if (IsExcluded(Start,Stop)) return;
    const float Lifetime = bHit ? Hit.Time*MaxLife : MaxLife;
    if (Lifetime < .03f) return;
    auto* FX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),System,Start,
        FRotator::ZeroRotator,FVector::OneVector,true,false,ENCPoolMethod::AutoRelease,false);
    if (!FX) return;
    FX->SetVariableFloat(TEXT("User.FallLifetime"),Lifetime);
    FX->SetVariableVec3(TEXT("User.FallVelocity"),Velocity);
    FX->Activate(true);
    if (bHit) PendingImpacts.Add({GetWorld()->GetTimeSeconds()+Lifetime,Start,Stop,bSnow});
}
void AUEInstanceWeatherDirector::UpdateSurface(const FVector& Center)
{
    if (!SurfaceMID) return;
    const int32 GridRadius = FMath::Clamp(SurfaceGridRadius,1,6);
    const int32 Side = GridRadius*2+1;
    const int32 Count = Side*Side;
    const float Size = FMath::Clamp(SurfaceTileSize,50.f,400.f);
    // ponytail: 플레이어 주변 타일만 표시한다. 영구 발자국/변형이 필요하면 표면 RT로 확장한다.
    while (Tiles.Num() < Count)
    {
        auto* Tile = NewObject<UDecalComponent>(this);
        AddInstanceComponent(Tile);
        Tile->SetDecalMaterial(SurfaceMID);
        Tile->SetFadeScreenSize(0);
        Tile->RegisterComponent();
        Tiles.Add(Tile);
    }
    for (int32 i=Count;i<Tiles.Num();++i) Tiles[i]->SetVisibility(false);
    const int32 X = FMath::FloorToInt(Center.X/Size);
    const int32 Y = FMath::FloorToInt(Center.Y/Size);
    for (int32 n=0;n<12;++n)
    {
        const int32 Index = TileCursor;
        TileCursor = (TileCursor+1) % Count;
        FVector P((X+Index%Side-GridRadius+.5)*Size,(Y+Index/Side-GridRadius+.5)*Size,Center.Z);
        FHitResult Hit;
        const bool Visible = Trace(P+FVector(0,0,SkyTraceHeight),P-FVector(0,0,2000),Hit) &&
            Hit.ImpactNormal.Z > .55f && !IsWater(Hit) && !UsesWeatherMaterial(Hit) &&
            !IsExcluded(Hit.ImpactPoint,Hit.ImpactPoint+Hit.ImpactNormal*2);
        auto* Tile = Tiles[Index].Get();
        Tile->SetVisibility(Visible);
        if (!Visible) continue;
        // 얇은 데칼을 표면 법선에 맞춰 바닥 밑층과 벽까지 투영되는 일을 줄인다.
        Tile->SetWorldLocation(Hit.ImpactPoint+Hit.ImpactNormal*2);
        Tile->SetWorldRotation((-Hit.ImpactNormal).Rotation());
        Tile->DecalSize = FVector(6,Size*.65,Size*.65);
        Tile->MarkRenderStateDirty();
    }
    SurfaceMID->SetScalarParameterValue(TEXT("Wetness"),State.GroundWetness);
    SurfaceMID->SetScalarParameterValue(TEXT("Snow"),State.SnowCoverage);
}
void AUEInstanceWeatherDirector::ApplyParameters()
{
    if (!Parameters) return;
    auto* MPC = GetWorld()->GetParameterCollectionInstance(Parameters);
    if (!MPC) return;
    MPC->SetScalarParameterValue(TEXT("Wetness"),State.GroundWetness);
    MPC->SetScalarParameterValue(TEXT("Snow"),State.SnowCoverage);
    MPC->SetScalarParameterValue(TEXT("Rain"),State.RainIntensity);
    MPC->SetScalarParameterValue(TEXT("Cloud"),State.CloudAmount);
    MPC->SetScalarParameterValue(TEXT("Fog"),State.FogDensity);
    const float A = FMath::DegreesToRadians(State.WindDirectionDegrees);
    MPC->SetVectorParameterValue(TEXT("Wind"),FLinearColor(FMath::Cos(A)*State.WindIntensity,
        FMath::Sin(A)*State.WindIntensity,0,0));
}
void AUEInstanceWeatherDirector::ClearWeather()
{
    PendingImpacts.Reset();
    DropBudget = 0;
    State = {};
    for (auto Tile : Tiles) if (Tile) Tile->SetVisibility(false);
    for (auto Mark : WetMarks) if (Mark.IsValid()) Mark->DestroyComponent();
    WetMarks.Reset();
    if (Parameters)
        if (auto* MPC=GetWorld()->GetParameterCollectionInstance(Parameters))
            MPC->SetScalarParameterValue(TEXT("ExclusionCount"),0);
    ApplyParameters();
    bHadWeather = false;
}
void AUEInstanceWeatherDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    auto* PC = GetWorld()->GetFirstPlayerController();
    if (!PC || !PC->IsLocalController()) return;
    auto* Bridge = UUEFieldServerBridgeComponent::Find(PC);
    if (Source != Bridge)
    {
        ClearWeather();
        Source = Bridge;
        Presentation->SetWeatherSource(Bridge);
    }
    if (!IsValid(Source) || !Source->HasInstanceWeatherState())
    {
        if (bHadWeather) { ClearWeather(); OnWeatherVisualsUpdated(State,true); }
        return;
    }
    bHadWeather = true;
    State = Presentation->GetPresentationState();
    FVector Center; FRotator CameraRotation;
    PC->GetPlayerViewPoint(Center,CameraRotation);
    SetActorLocation(Center);
    ExclusionTimer -= DeltaSeconds;
    if (ExclusionTimer <= 0)
    {
        Exclusions.Reset();
        for (TActorIterator<AUEWeatherExclusionVolume> It(GetWorld());It;++It) Exclusions.Add(*It);
        UpdateExclusionParameters();
        ExclusionTimer = 1;
    }
    FHitResult Roof;
    bCameraSheltered = IsExcluded(Center,Center) || Trace(Center,Center+FVector(0,0,SkyTraceHeight),Roof);
    const float Intensity = FMath::Clamp(State.RainIntensity+State.SnowIntensity,0.f,1.f);
    DropBudget = FMath::Min(16.f,DropBudget+FMath::Min(DeltaSeconds,.1f)*
        FMath::Clamp(MaxDropsPerSecond,1.f,200.f)*Intensity);
    while (DropBudget>=1)
    {
        DropBudget-=1;
        SpawnDrop(Center,FMath::FRand()*Intensity<State.SnowIntensity);
    }
    for (int32 i=PendingImpacts.Num()-1;i>=0;--i)
    {
        const auto& Impact = PendingImpacts[i];
        if (Impact.Due>GetWorld()->GetTimeSeconds()) continue;
        FHitResult Hit;
        const FVector Direction = (Impact.End-Impact.Start).GetSafeNormal();
        // 충돌 직전에 재검사하여 사라진/움직인 물체의 허공에 효과가 남지 않게 한다.
        if (Trace(Impact.Start,Impact.End+Direction*10,Hit) && !IsExcluded(Impact.Start,Hit.ImpactPoint))
        {
            SpawnImpact(Hit,Impact.bSnow);
        }
        PendingImpacts.RemoveAtSwap(i);
    }
    SurfaceTimer+=DeltaSeconds;
    if (SurfaceTimer>=.1f) { SurfaceTimer=0; UpdateSurface(Center); }
    ApplyParameters();
    OnWeatherVisualsUpdated(State,bCameraSheltered);
}
void AUEInstanceWeatherDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    ClearWeather();
    for (auto Tile : Tiles) if (Tile) Tile->DestroyComponent();
    Super::EndPlay(Reason);
}
