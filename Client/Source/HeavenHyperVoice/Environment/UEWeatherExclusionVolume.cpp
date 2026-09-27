#include "UEWeatherExclusionVolume.h"
#include "Components/BoxComponent.h"
AUEWeatherExclusionVolume::AUEWeatherExclusionVolume()
{
    Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("WeatherBounds"));
    SetRootComponent(Bounds);
    Bounds->SetBoxExtent(FVector(500,500,300));
    Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Bounds->SetHiddenInGame(true);
}
bool AUEWeatherExclusionVolume::ContainsPoint(const FVector& Point) const
{
    return FBox(-Bounds->GetUnscaledBoxExtent(), Bounds->GetUnscaledBoxExtent())
        .IsInsideOrOn(Bounds->GetComponentTransform().InverseTransformPosition(Point));
}
bool AUEWeatherExclusionVolume::IntersectsPath(const FVector& Start, const FVector& End) const
{
    const FVector A = Bounds->GetComponentTransform().InverseTransformPosition(Start);
    const FVector B = Bounds->GetComponentTransform().InverseTransformPosition(End);
    const FBox Box(-Bounds->GetUnscaledBoxExtent(), Bounds->GetUnscaledBoxExtent());
    return Box.IsInsideOrOn(A) || Box.IsInsideOrOn(B) || FMath::LineBoxIntersection(Box,A,B,B-A);
}
