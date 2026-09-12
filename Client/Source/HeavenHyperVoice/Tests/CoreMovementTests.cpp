#if WITH_DEV_AUTOMATION_TESTS
#include "../Character/UEPlayerCharacter.h"
#include "../Movement/UECoreCollisionSubsystem.h"
#include "../Movement/UECoreMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

namespace
{
struct FCoreTestScene
{
	UWorld* World;
	APawn* Pawn;
	UUECoreMovementComponent* Movement;

	FCoreTestScene(const TCHAR* PackagePath = nullptr)
	{
		const auto Settings = UWorld::InitializationValues()
		                          .AllowAudioPlayback(false)
		                          .CreatePhysicsScene(true)
		                          .CreateNavigation(false)
		                          .CreateAISystem(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None,
		                            PackagePath ? CreatePackage(PackagePath) : nullptr, true,
		                            ERHIFeatureLevel::Num, &Settings);
		Pawn = World->SpawnActor<APawn>();
		auto* Capsule = NewObject<UCapsuleComponent>(Pawn);
		Pawn->SetRootComponent(Capsule);
		Capsule->InitCapsuleSize(34, 88);
		Capsule->SetCollisionProfileName(TEXT("Pawn"));
		Capsule->RegisterComponent();
		Movement = NewObject<UUECoreMovementComponent>(Pawn);
		Movement->RegisterComponent();
		Movement->SetUpdatedComponent(Capsule);
		Movement->bRunWithoutController = true;
		Movement->Activate(true);
		Reset(FVector(0, 0, 88.1));
	}

	~FCoreTestScene()
	{
		World->DestroyWorld(false);
	}

	UBoxComponent* Box(FVector Center, FVector Extent, FRotator Rotation = FRotator::ZeroRotator)
	{
		auto* Actor = World->SpawnActor<AActor>();
		auto* Box = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box);
		Box->SetBoxExtent(Extent);
		Box->SetCollisionProfileName(TEXT("ServerGround"));
		Box->RegisterComponent();
		Box->SetWorldLocationAndRotation(Center, Rotation);
		World->GetSubsystem<UUECoreCollisionSubsystem>()->RebuildFromScene();
		return Box;
	}

	void Reset(FVector Position)
	{
		Pawn->SetActorLocation(Position);
		Movement->StopMovementImmediately();
		Movement->OnTeleported();
	}

	void Tick(float Dt, FVector Input = FVector::ZeroVector)
	{
		Movement->AddInputVector(Input);
		Movement->TickComponent(Dt, LEVELTICK_All, &Movement->PrimaryComponentTick);
	}

	void Advance(int Frames, FVector Input = FVector::ZeroVector, float Dt = 1.f / 60.f)
	{
		for (int Frame = 0; Frame < Frames; ++Frame)
			Tick(Dt, Input);
	}
};

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHHVCoreFloorTest, "HHV.Movement.Core.FloorJumpAndFrameRate",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHHVCoreFloorTest::RunTest(const FString&)
{
	FCoreTestScene Scene;
	Scene.Box(FVector(0, 0, -20), FVector(3000, 3000, 20));
	Scene.Advance(5);
	TestTrue(TEXT("Grounded on portable baked collision"), Scene.Movement->IsMovingOnGround());
	Scene.Tick(1.f / 60, FVector(1, 0, 0));
	TestTrue(TEXT("Acceleration rather than instant max speed"),
	         Scene.Movement->Velocity.X > 0 && Scene.Movement->Velocity.X < 260);
	Scene.Advance(60, FVector(1, 1, 0));
	TestTrue(TEXT("Diagonal speed bounded"), Scene.Movement->Velocity.Size2D() <= 260.1);
	Scene.Advance(45);
	TestTrue(TEXT("Stops with braking"), Scene.Movement->Velocity.IsNearlyZero(.1));
	Scene.Movement->RequestCoreJump();
	Scene.Tick(1.f / 60);
	TestTrue(TEXT("Jump sets falling and upward velocity"),
	         Scene.Movement->IsFalling() && Scene.Movement->Velocity.Z > 0);
	Scene.Advance(100);
	TestTrue(TEXT("Lands on ground"), Scene.Movement->IsMovingOnGround());
	TestTrue(TEXT("Capsule rests above floor"), FMath::Abs(Scene.Pawn->GetActorLocation().Z - 88.1) < .5);
	FVector Positions[3];
	const int Rates[] = {30, 60, 120};

	for (int Index = 0; Index < 3; ++Index)
	{
		Scene.Reset(FVector(0, 0, 88.1));
		Scene.Advance(5);
		Scene.Advance(Rates[Index] * 2, FVector(1, 0, 0), 1.f / Rates[Index]);
		Positions[Index] = Scene.Pawn->GetActorLocation();
	}
	TestTrue(TEXT("30/60Hz same fixed-step displacement"), Positions[0].Equals(Positions[1], .05));
	TestTrue(TEXT("60/120Hz same fixed-step displacement"), Positions[1].Equals(Positions[2], .05));
	Scene.Movement->MaxWalkSpeed = 100;
	Scene.Reset(FVector(0, 0, 88.1));
	Scene.Advance(60, FVector(1, 0, 0));
	TestTrue(TEXT("Details tuning affects core"), Scene.Movement->Velocity.X <= 100.1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHHVCoreCollisionTest, "HHV.Movement.Core.WallStepSlopeLedge",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHHVCoreCollisionTest::RunTest(const FString&)
{
	{
		FCoreTestScene S;
		S.Box(FVector(0, 0, -20), FVector(2000, 2000, 20));
		S.Box(FVector(150, 0, 250), FVector(1, 1000, 250));
		S.Advance(120, FVector(1, .3, 0));
		TestTrue(TEXT("Thin wall stops capsule"), S.Pawn->GetActorLocation().X < 116);
		TestTrue(TEXT("Slides along wall"), S.Pawn->GetActorLocation().Y > 100);
		TestTrue(TEXT("Wall slide state exposed"), S.Movement->IsWallSliding());
		TestTrue(TEXT("Export wall replay"),
		         S.Movement->ExportCoreReplay(FPaths::ProjectSavedDir() / TEXT("CoreMovement/Wall")));
		S.Box(FVector(0, 200, 250), FVector(1000, 1, 250));
		S.Reset(FVector(0, 0, 88.1));
		S.Advance(120, FVector(1, 1, 0));
		TestTrue(TEXT("Inside corner blocks both axes"),
		         S.Pawn->GetActorLocation().X < 116 && S.Pawn->GetActorLocation().Y < 166);
	}
	{
		FCoreTestScene S;
		S.Box(FVector(0, 0, -20), FVector(2000, 2000, 20));
		S.Box(FVector(350, 0, 15), FVector(200, 500, 15));
		S.Advance(100, FVector(1, 0, 0));
		TestTrue(TEXT("Export step replay"),
		         S.Movement->ExportCoreReplay(FPaths::ProjectSavedDir() / TEXT("CoreMovement/Step")));
		TestTrue(TEXT("Climbs 30cm step"),
		         S.Pawn->GetActorLocation().X > 300 && S.Pawn->GetActorLocation().Z > 117);
		S.Reset(FVector(0, 800, 88.1));
		S.Box(FVector(350, 800, 50), FVector(200, 100, 50));
		S.Advance(100, FVector(1, 0, 0));
		TestTrue(TEXT("Rejects 100cm step"), S.Pawn->GetActorLocation().X < 117);
	}
	{
		FCoreTestScene S;
		S.Box(FVector(0, 0, -20), FVector(1500, 500, 20), FRotator(25, 0, 0));
		S.Reset(FVector(0, 0, 110));
		S.Advance(30);
		const double StartZ = S.Pawn->GetActorLocation().Z;
		S.Advance(100, FVector(1, 0, 0));
		TestTrue(TEXT("Walks up 25 degree slope"),
		         S.Pawn->GetActorLocation().X > 300 && S.Pawn->GetActorLocation().Z > StartZ + 100);
		TestTrue(TEXT("Slope keeps grounding"), S.Movement->IsMovingOnGround());
		TestTrue(TEXT("Export slope replay"),
		         S.Movement->ExportCoreReplay(FPaths::ProjectSavedDir() / TEXT("CoreMovement/Slope")));
	}
	{
		FCoreTestScene S;
		S.Box(FVector(-450, 0, -20), FVector(550, 500, 20));
		S.Advance(100, FVector(1, 0, 0));
		TestTrue(TEXT("Walks off ledge and falls"),
		         S.Movement->IsFalling() && S.Pawn->GetActorLocation().Z < 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHHVCorePlayerTest, "HHV.Movement.Core.PlayerHasNoCharacterMovement",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHHVCorePlayerTest::RunTest(const FString&)
{
	const auto* Defaults = GetDefault<AUEPlayerCharacter>();
	TestEqual(TEXT("Movement is the independent Pawn component"), Defaults->GetMovementComponent(),
	          static_cast<UPawnMovementComponent*>(Defaults->GetCoreMovement()));
	TestNotNull(TEXT("Capsule retained"), Defaults->GetCapsuleComponent());
	TestNotNull(TEXT("Mesh retained"), Defaults->GetMesh());
	TArray<UActorComponent*> Components;
	Defaults->GetComponents(Components);

	for (auto* Component : Components)
	{
		for (UClass* Class = Component->GetClass(); Class; Class = Class->GetSuperClass())
			TestNotEqual(TEXT("No CharacterMovement in player component inheritance"), Class->GetName(),
			             FString(TEXT("CharacterMovementComponent")));
	}
	TestTrue(TEXT("Local movement test defaults to no field connection"), Defaults->bLocalMovementTest);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHHVPortableCollisionTest, "HHV.Movement.Core.PortableCollisionReplay",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHHVPortableCollisionTest::RunTest(const FString&)
{
	FCoreTestScene S;
	auto* Floor = S.Box(FVector(0, 0, -20), FVector(3000, 3000, 20));
	auto* Wall = S.Box(FVector(250, 0, 250), FVector(1, 1000, 250));
	auto* Snapshot = S.World->GetSubsystem<UUECoreCollisionSubsystem>();
	const FString Base = FPaths::ProjectSavedDir() / TEXT("CoreMovement/PortableReplay");
	TestTrue(TEXT("Save canonical snapshot"), Snapshot->SaveCollisionFile(Base + TEXT(".hhvcollision")));
	// No engine collision remains. The player must still stand, jump and hit the wall.
	Floor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TestTrue(TEXT("Reload canonical snapshot"), Snapshot->LoadCollisionFile(Base + TEXT(".hhvcollision")));
	S.Advance(10);
	TestTrue(TEXT("Ground without engine collision"), S.Movement->IsMovingOnGround());
	S.Movement->RequestCoreJump();
	S.Advance(120, FVector(1, .3, 0));
	TestTrue(TEXT("Portable wall still blocks"), S.Pawn->GetActorLocation().X < 216);
	TestTrue(TEXT("Portable floor still supports"), S.Movement->IsMovingOnGround());
	TestTrue(TEXT("Export client inputs and states for standalone verifier"),
	         S.Movement->ExportCoreReplay(Base));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHHVCoreDebugRenderingTest,
                                 "HHV.Movement.Core.IgnoreGameplayDebuggerRendering",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHHVCoreDebugRenderingTest::RunTest(const FString&)
{
	FCoreTestScene S;
	// PIE spawns this component at runtime; an editor-only map bake never sees it.
	UClass* DebugClass = LoadClass<UPrimitiveComponent>(
	    nullptr, TEXT("/Script/GameplayDebugger.GameplayDebuggerRenderingComponent"));

	if (!TestNotNull(TEXT("Actual PIE debugger rendering class"), DebugClass))
		return false;
	auto* DebugActor = S.World->SpawnActor<AActor>();
	auto* Debug = NewObject<UPrimitiveComponent>(DebugActor, DebugClass);
	DebugActor->SetRootComponent(Debug);
	Debug->SetCollisionProfileName(TEXT("BlockAll"));
	Debug->RegisterComponent();
	TestTrue(TEXT("Debug renderer advertises query collision"), Debug->IsQueryCollisionEnabled());
	S.Box(FVector(0, 0, -20), FVector(2000, 2000, 20));
	auto* Snapshot = S.World->GetSubsystem<UUECoreCollisionSubsystem>();

	if (!TestTrue(TEXT("Debug rendering does not prevent snapshot preparation"), Snapshot->EnsureReady()))
		return false;
	TestEqual(TEXT("Only floor geometry is exported"), Snapshot->TriangleCount, 12);
	S.Advance(10);
	TestTrue(TEXT("Player leaves initial falling state"), S.Movement->IsMovingOnGround());
	S.Advance(60, FVector(0, 1, 0));
	TestTrue(TEXT("Directional input moves player"), S.Pawn->GetActorLocation().Y > 100);
	TestTrue(TEXT("Directional input turns player"), FMath::Abs(S.Pawn->GetActorRotation().Yaw - 90) < 1);
	S.Movement->RequestCoreJump();
	S.Tick(1.f / 60.f);
	TestTrue(TEXT("Jump input reaches core"), S.Movement->IsFalling() && S.Movement->Velocity.Z > 0);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHHVCorePresetFilterTest, "HHV.Movement.Core.ExportOnlySharedPresets",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHHVCorePresetFilterTest::RunTest(const FString&)
{
	FCoreTestScene S;
	S.Box(FVector(0, 0, -20), FVector(2000, 2000, 20));
	auto* Wall = S.Box(FVector(150, 0, 250), FVector(1, 1000, 250));
	Wall->SetCollisionProfileName(TEXT("ServerWall"));
	auto* DecoyActor = S.World->SpawnActor<AActor>();
	DecoyActor->Tags.Add(TEXT("ServerGround"));
	auto* Decoy = NewObject<UBoxComponent>(DecoyActor, TEXT("ServerWall_NameOnly"));
	DecoyActor->SetRootComponent(Decoy);
	Decoy->SetBoxExtent(FVector(500, 500, 500));
	Decoy->ComponentTags.Add(TEXT("ServerWall"));
	Decoy->SetCollisionProfileName(TEXT("BlockAll"));
	Decoy->RegisterComponent();
	auto* UnsupportedActor = S.World->SpawnActor<AActor>();
	auto* Sphere = NewObject<USphereComponent>(UnsupportedActor);
	UnsupportedActor->SetRootComponent(Sphere);
	Sphere->SetCollisionProfileName(TEXT("BlockAll"));
	Sphere->RegisterComponent();
	auto* Snapshot = S.World->GetSubsystem<UUECoreCollisionSubsystem>();
	TestTrue(TEXT("Unselected geometry and tags do not cause export failure"), Snapshot->RebuildFromScene());
	TestEqual(TEXT("Only the two selected presets are exported"), Snapshot->TriangleCount, 24);
	const auto Hash = Snapshot->GetCollision()->hash();
	S.Advance(90, FVector(1, 0, 0));
	TestTrue(TEXT("Decoy box is ignored and selected wall blocks"),
	         S.Pawn->GetActorLocation().X > 100 && S.Pawn->GetActorLocation().X < 116);
	Sphere->SetCollisionProfileName(TEXT("ServerWall"));
	AddExpectedError(TEXT("Core collision bake: unsupported blocking component"),
	                 EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Unsupported selected obstacle fails export"), Snapshot->RebuildFromScene());
	TestEqual(TEXT("Failed export retains the previous snapshot"), Snapshot->GetCollision()->hash(), Hash);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHHVCoreSavedMapTest, "HHV.Movement.Core.LoadSavedMapInPIE",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHHVCoreSavedMapTest::RunTest(const FString&)
{
	// Empty live scene: this must load the exported artifact, not collect world components.
	FCoreTestScene S(TEXT("/Game/Level/UEDPIE_77_PlayerTestLevel"));
	auto* Snapshot = S.World->GetSubsystem<UUECoreCollisionSubsystem>();
	TestEqual(TEXT("PIE suffix is removed but package folder is kept"), Snapshot->GetCollisionRelativePath(),
	          FString(TEXT("Level/PlayerTestLevel.hhvcollision")));

	if (!TestTrue(TEXT("Default client path loads the shipped snapshot"), Snapshot->EnsureReady()))
		return false;
	TestTrue(TEXT("Saved map has collision even in an empty live scene"), Snapshot->TriangleCount > 0);
	TestEqual(TEXT("Loaded file is the map default"), Snapshot->LoadedFile,
	          Snapshot->GetDefaultCollisionFile());
	return true;
}
#endif
