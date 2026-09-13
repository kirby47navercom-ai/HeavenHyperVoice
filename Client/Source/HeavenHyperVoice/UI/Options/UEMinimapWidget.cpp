#include "UEMinimapWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"

void UUEMinimapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!GetWorld() || !GetWorld()->IsGameWorld() || CaptureActor) return;
	UMaterialInstanceDynamic* Material = MapImage->GetDynamicMaterial();
	if (!Material) return;
	Target = NewObject<UTextureRenderTarget2D>(this);
	Target->RenderTargetFormat = RTF_RGBA8;
	Target->ClearColor = FLinearColor(.04f, .07f, .08f, 1.f);
	Target->InitAutoFormat(256, 256);
	Material->SetTextureParameterValue(TEXT("MapTexture"), Target);
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	CaptureActor = GetWorld()->SpawnActor<AActor>(Params);
	if (!CaptureActor) return;
	Capture = NewObject<USceneCaptureComponent2D>(CaptureActor);
	CaptureActor->SetRootComponent(Capture);
	CaptureActor->AddInstanceComponent(Capture);
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->ProjectionType = ECameraProjectionMode::Orthographic;
	Capture->OrthoWidth = ViewWidth;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Capture->TextureTarget = Target;
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetVolumetricFog(false);
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetMotionBlur(false);
	Capture->ShowFlags.SetDynamicShadows(false);
	Capture->ShowFlags.SetTemporalAA(false);
	Capture->RegisterComponent();
	LocationText->SetText(FText::FromString(GetWorld()->GetMapName().Contains(TEXT("Goldenrod"))
		? TEXT("금빛시티") : TEXT("탐험 필드")));
}

void UUEMinimapWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
	Super::NativeTick(Geometry, DeltaSeconds);
	APlayerController* Controller = GetOwningPlayer();
	APawn* Player = GetOwningPlayerPawn();
	if (!Capture || !Controller || !Player) return;
	// North is world +X. Keep terrain north-up and rotate only the two markers.
	PlayerArrow->SetRenderTransformAngle(Player->GetActorRotation().Yaw);
	ViewArrow->SetRenderTransformAngle(Controller->GetControlRotation().Yaw);
	Elapsed += DeltaSeconds;
	if (Elapsed < UpdateInterval) return;
	Elapsed = 0.f;
	Capture->HiddenActors.Reset();
	Capture->HiddenActors.Add(Player);
	Capture->SetWorldLocationAndRotation(Player->GetActorLocation() + FVector(0,0,CaptureHeight), FRotator(-90,0,0));
	Capture->CaptureScene();
}

void UUEMinimapWidget::NativeDestruct()
{
	if (CaptureActor) CaptureActor->Destroy();
	Capture = nullptr;
	CaptureActor = nullptr;
	Target = nullptr;
	Super::NativeDestruct();
}
