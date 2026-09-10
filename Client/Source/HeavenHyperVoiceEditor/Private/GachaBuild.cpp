#include "GachaBuild.h"
#include "../../HeavenHyperVoice/Gacha/UEGachaMachine.h"
#include "../../HeavenHyperVoice/Gacha/UEGachaStudio.h"
#include "../../HeavenHyperVoice/Pokemon/UEPokemonSpeciesData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/PointLight.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "StaticMeshAttributes.h"
#include "MeshDescription.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"

namespace
{
const FString Base = TEXT("/Game/Gacha/");
bool bSaveFailed = false;

void Save(UObject* Object)
{
	UPackage* Package = Object->GetOutermost();
	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(Object);
	const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(),
		Object->IsA<UWorld>() ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Package, Object, *Filename, Args)) { bSaveFailed = true; UE_LOG(LogTemp, Error, TEXT("Gacha save failed: %s"), *Filename); }
}

template<class T> T* Asset(const FString& Relative)
{
	const FString Path = Base + Relative;
	return NewObject<T>(CreatePackage(*Path), *FPackageName::GetLongPackageAssetName(Path), RF_Public | RF_Standalone);
}

UMaterial* Material(const FString& Name, FLinearColor Color, float Metal = 0, float Roughness = .35f, bool bGlass = false, bool bGlow = false)
{
	UMaterial* M = Asset<UMaterial>(TEXT("Materials/") + Name);
	auto Add = [M](UClass* Class) { UMaterialExpression* E = NewObject<UMaterialExpression>(M, Class); M->GetExpressionCollection().AddExpression(E); return E; };
	auto Scalar = [&](float Value) { auto* E = CastChecked<UMaterialExpressionConstant>(Add(UMaterialExpressionConstant::StaticClass())); E->R = Value; return E; };
	auto* Tint = CastChecked<UMaterialExpressionVectorParameter>(Add(UMaterialExpressionVectorParameter::StaticClass()));
	Tint->ParameterName = TEXT("Tint"); Tint->DefaultValue = Color;
	auto* Data = M->GetEditorOnlyData();
	Data->BaseColor.Expression = Tint;
	Data->Metallic.Expression = Scalar(Metal);
	Data->Roughness.Expression = Scalar(Roughness);
	if (bGlow) Data->EmissiveColor.Expression = Tint;
	if (bGlass)
	{
		M->BlendMode = BLEND_Translucent;
		M->TwoSided = true;
		M->TranslucencyLightingMode = TLM_SurfacePerPixelLighting;
		auto* Fresnel = CastChecked<UMaterialExpressionFresnel>(Add(UMaterialExpressionFresnel::StaticClass()));
		auto* Multiply = CastChecked<UMaterialExpressionMultiply>(Add(UMaterialExpressionMultiply::StaticClass()));
		Multiply->A.Expression = Fresnel; Multiply->ConstB = .32f;
		auto* Sum = CastChecked<UMaterialExpressionAdd>(Add(UMaterialExpressionAdd::StaticClass()));
		Sum->A.Expression = Multiply; Sum->ConstB = .025f;
		Data->Opacity.Expression = Sum;
	}
	M->PostEditChange();
	Save(M);
	return M;
}

// 색상 덮개, 흰 몸체, 이음매, 버튼에 각각 머테리얼을 지정할 수 있는 닫힌 구체를 만든다.
UStaticMesh* BallMesh(const FString& Name, int32 Tier, UMaterialInterface* Cap, UMaterialInterface* White, UMaterialInterface* Black, UMaterialInterface* Accent)
{
	UStaticMesh* Mesh = Asset<UStaticMesh>(TEXT("Meshes/") + Name);
	TArray<UMaterialInterface*> Materials = {Cap, White, Black, Accent};
	FMeshDescription D;
	FStaticMeshAttributes A(D); A.Register();
	auto Positions = A.GetVertexPositions(); auto Normals = A.GetVertexInstanceNormals(); auto Tangents = A.GetVertexInstanceTangents();
	auto Signs = A.GetVertexInstanceBinormalSigns(); auto UV = A.GetVertexInstanceUVs(); UV.SetNumChannels(1);
	TArray<FPolygonGroupID> Groups;
	for (int32 I = 0; I < Materials.Num(); ++I)
	{
		const FName Slot(*FString::Printf(TEXT("Part%d"), I));
		Mesh->GetStaticMaterials().Add(FStaticMaterial(Materials[I], Slot, Slot));
		const FPolygonGroupID G = D.CreatePolygonGroup(); Groups.Add(G); A.GetPolygonGroupMaterialSlotNames()[G] = Slot;
	}
	constexpr int32 Rings = 32, Slices = 64;
	for (int32 R = 0; R < Rings; ++R) for (int32 S = 0; S < Slices; ++S)
	{
		const float Theta = PI * (R + .5f) / Rings, Phi = 2 * PI * (S + .5f) / Slices;
		const FVector N(FMath::Sin(Theta)*FMath::Cos(Phi), FMath::Sin(Theta)*FMath::Sin(Phi), FMath::Cos(Theta));
		int32 Group = N.Z >= 0 ? 0 : 1;
		if (FMath::Abs(N.Z) < .10f) Group = 2;
		if (Tier == 1 && N.Z > .12f && FMath::Abs(N.Y) > .42f && FMath::Abs(N.Y) < .73f) Group = 3;
		if (Tier == 2 && N.Z > .12f && ((FMath::Abs(N.Y) > .4f && FMath::Abs(N.Y) < .67f) || N.Z > .94f)) Group = 3;
		if (N.X < -.955f) Group = 1;
		else if (N.X < -.925f) Group = 2;
		TArray<FVertexInstanceID> V;
		for (const FIntPoint Offset : {FIntPoint(0,0),FIntPoint(1,0),FIntPoint(1,1),FIntPoint(0,1)})
		{
			const float T = PI * (R+Offset.X) / Rings, P = 2*PI*(S+Offset.Y)/Slices;
			const FVector3f Normal(FMath::Sin(T)*FMath::Cos(P),FMath::Sin(T)*FMath::Sin(P),FMath::Cos(T));
			const FVertexID Vertex = D.CreateVertex(); Positions[Vertex] = Normal * 11.f;
			const FVertexInstanceID VI = D.CreateVertexInstance(Vertex); V.Add(VI);
			Normals[VI] = Normal; Tangents[VI] = FVector3f(-FMath::Sin(P),FMath::Cos(P),0); Signs[VI] = 1;
			UV.Set(VI,0,FVector2f(float(S+Offset.Y)/Slices,float(R+Offset.X)/Rings));
		}
		D.CreatePolygon(Groups[Group], V);
	}
	FStaticMeshSourceModel& Source = Mesh->AddSourceModel();
	Source.BuildSettings.bRecomputeNormals = false;
	Source.BuildSettings.bRecomputeTangents = false;
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bBuildSimpleCollision = true;
	Mesh->BuildFromMeshDescriptions({&D}, Params);
	Save(Mesh);
	return Mesh;
}

UBlueprint* Blueprint(const FString& Relative, UClass* Parent)
{
	const FString Path = Base + Relative;
	UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(Parent, CreatePackage(*Path), *FPackageName::GetLongPackageAssetName(Path), BPTYPE_Normal,
		UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	FKismetEditorUtilities::CompileBlueprint(BP);
	return BP;
}

UStaticMeshComponent* Part(UBlueprint* BP, const TCHAR* Name, UStaticMesh* Mesh, UMaterialInterface* Mat, FVector Position, FVector Scale,
	FRotator Rotation = FRotator::ZeroRotator, const USceneComponent* Parent = nullptr, FName Tag = NAME_None)
{
	USCS_Node* Node = BP->SimpleConstructionScript->CreateNode(UStaticMeshComponent::StaticClass(), Name);
	BP->SimpleConstructionScript->AddNode(Node);
	if (Parent) Node->SetParent(Parent);
	UStaticMeshComponent* C = CastChecked<UStaticMeshComponent>(Node->ComponentTemplate);
	C->SetStaticMesh(Mesh); if (Mat) C->SetMaterial(0,Mat);
	C->SetRelativeLocation(Position); C->SetRelativeScale3D(Scale); C->SetRelativeRotation(Rotation);
	C->SetMobility(EComponentMobility::Movable);
	C->SetCollisionEnabled(Tag == TEXT("GachaHandle") ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	C->SetCollisionResponseToAllChannels(ECR_Ignore); C->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
	if (!Tag.IsNone()) C->ComponentTags.Add(Tag);
	return C;
}

FSlateBrush Brush(FLinearColor Color, float Radius=12) { return FSlateRoundedBoxBrush(Color,Radius); }

UWidgetBlueprint* MakeUI()
{
	const FString Path = Base + TEXT("UI/WBP_GachaStudio");
	auto* BP = CastChecked<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(UUEGachaStudioWidget::StaticClass(), CreatePackage(*Path), TEXT("WBP_GachaStudio"), BPTYPE_Normal,
		UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
	UWidgetTree* Tree = BP->WidgetTree;
	UScaleBox* Scale = Tree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(),TEXT("ResponsiveScale"));
	Scale->SetStretch(EStretch::ScaleToFit); Scale->SetVisibility(ESlateVisibility::SelfHitTestInvisible); Tree->RootWidget=Scale;
	USizeBox* Size = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("DesignSize"));
	Size->SetWidthOverride(1920); Size->SetHeightOverride(1080); Size->SetVisibility(ESlateVisibility::SelfHitTestInvisible); Scale->AddChild(Size);
	UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("RootCanvas"));
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible); Size->AddChild(Root);
	auto Place = [](UCanvasPanel* Parent,UWidget* W,FVector2D P,FVector2D S,int32 Z=0) { auto* Slot=Parent->AddChildToCanvas(W); Slot->SetPosition(P); Slot->SetSize(S); Slot->SetZOrder(Z); };
	auto Text = [&](UCanvasPanel* Parent,const TCHAR* Name,const FString& Value,FVector2D P,FVector2D S,int32 FontSize=22,FLinearColor Color=FLinearColor(.85f,.9f,.94f))
	{
		auto* W=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name); W->SetText(FText::FromString(Value));
		FSlateFontInfo F=W->GetFont();F.Size=FontSize;W->SetFont(F);W->SetColorAndOpacity(Color);W->SetAutoWrapText(true);
		W->SetVisibility(ESlateVisibility::HitTestInvisible);Place(Parent,W,P,S,2);return W;
	};
	auto Panel = [&](const TCHAR* Name,FVector2D P,FVector2D S,FLinearColor Color)
	{
		auto* W=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),Name);W->SetBrush(Brush(FLinearColor::White));W->SetBrushColor(Color);W->SetPadding(FMargin(0));Place(Root,W,P,S);return W;
	};
	auto Button = [&](const TCHAR* Name,const TCHAR* Label,FVector2D P,FVector2D S)
	{
		auto* W=Tree->ConstructWidget<UButton>(UButton::StaticClass(),Name);
		FButtonStyle Style;Style.SetNormal(Brush(FLinearColor(.035f,.052f,.073f))).SetHovered(Brush(FLinearColor(.085f,.16f,.21f)))
			.SetPressed(Brush(FLinearColor(.02f,.035f,.05f))).SetDisabled(Brush(FLinearColor(.025f,.028f,.03f)));
		W->SetStyle(Style);Place(Root,W,P,S,3);
		auto* T=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),*FString(Name).Append(TEXT("Label")));
		T->SetText(FText::FromString(Label));FSlateFontInfo F=T->GetFont();F.Size=23;T->SetFont(F);T->SetColorAndOpacity(FLinearColor(.92f,.88f,.72f));W->AddChild(T);return W;
	};
	Panel(TEXT("TypePanel"),{40,210},{294,464},FLinearColor(.012f,.021f,.036f,.94f));
	Panel(TEXT("PoolPanel"),{1490,210},{390,640},FLinearColor(.012f,.021f,.036f,.94f));
	Panel(TEXT("InstructionPanel"),{476,877},{960,185},FLinearColor(.012f,.021f,.036f,.94f));
	Text(Root,TEXT("StudioTitle"),TEXT("포켓몬 캡슐"),{60,48},{900,80},42);
	Text(Root,TEXT("StudioSubtitle"),TEXT("작은 한 바퀴, 새로운 만남"),{63,126},{900,50},20,FLinearColor(.52f,.66f,.72f));
	Text(Root,TEXT("TypeTitle"),TEXT("타입 선택"),{66,234},{245,40},20);
	const TCHAR* Names[]={TEXT("FireButton"),TEXT("WaterButton"),TEXT("GrassButton"),TEXT("NormalButton"),TEXT("ElectricButton")};
	const TCHAR* Labels[]={TEXT("불꽃"),TEXT("물"),TEXT("풀"),TEXT("노말"),TEXT("전기")};
	for(int32 I=0;I<5;++I) Button(Names[I],Labels[I],{62,290.f+I*71},{250,58});
	Text(Root,TEXT("MachineTitle"),TEXT("불꽃"),{1520,236},{325,52},31);
	Text(Root,TEXT("PoolCaption"),TEXT("만날 수 있는 포켓몬"),{1520,302},{325,38},19,FLinearColor(.52f,.66f,.72f));
	Text(Root,TEXT("PoolText"),TEXT(""),{1520,359},{325,480},20);
	Text(Root,TEXT("TurnsText"),TEXT("0 / 3"),{517,900},{170,50},28);
	Text(Root,TEXT("StatusText"),TEXT("손잡이를 시계 방향으로 돌려 주세요"),{706,902},{685,75},22);
	auto* Progress=Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(),TEXT("TurnProgress"));
	Place(Root,Progress,{517,1018},{870,9},3);
	Panel(TEXT("StageOne"),{517,969},{270,13},FLinearColor(.06f,.08f,.12f));
	Panel(TEXT("StageTwo"),{817,969},{270,13},FLinearColor(.06f,.08f,.12f));
	Panel(TEXT("StageThree"),{1117,969},{270,13},FLinearColor(.06f,.08f,.12f));
	auto* Result=Panel(TEXT("ResultPanel"),{40,701},{294,300},FLinearColor(.012f,.021f,.036f,.96f));
	UCanvasPanel* ResultRoot=Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("ResultRoot"));Result->AddChild(ResultRoot);
	auto* Icon=Tree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("ResultIcon"));Place(ResultRoot,Icon,{70,12},{150,150});
	Text(ResultRoot,TEXT("ResultName"),TEXT(""),{24,181},{245,47},27);
	Text(ResultRoot,TEXT("ResultRarity"),TEXT(""),{24,238},{245,38},22);
	Result->SetVisibility(ESlateVisibility::Collapsed);
	Button(TEXT("AgainButton"),TEXT("한 번 더 뽑기"),{1490,887},{390,67})->SetVisibility(ESlateVisibility::Collapsed);
	FKismetEditorUtilities::CompileBlueprint(BP);
	Save(BP);return BP;
}
int32 WriteMap(UClass* MachineClass, UClass* ModeClass, const TArray<UUEGachaPool*>& Pools)
{
	if (FPackageName::DoesPackageExist(Base + TEXT("Maps/L_GachaStudio"))) return 1;
	UStaticMesh* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterial* Floor=LoadObject<UMaterial>(nullptr,*(Base+TEXT("Materials/M_StudioFloor.M_StudioFloor")));
	UMaterial* Black=LoadObject<UMaterial>(nullptr,*(Base+TEXT("Materials/M_Obsidian.M_Obsidian")));
	const FString MapPath=Base+TEXT("Maps/L_GachaStudio");
	UWorld* World=UWorld::CreateWorld(EWorldType::Editor,false,TEXT("L_GachaStudio"),CreatePackage(*MapPath));
	World->SetFlags(RF_Public | RF_Standalone);
	World->EditorViews.SetNum(4);
	World->EditorViews[0] = FLevelViewportInfo(FVector(-950,0,205), FRotator(-2,0,0), 10000);
	World->GetWorldSettings()->DefaultGameMode=ModeClass;
	World->GetWorldSettings()->bForceNoPrecomputedLighting=true;
	auto StageMesh=[&](const TCHAR* Name,UStaticMesh* Mesh,UMaterialInterface* Mat,FVector P,FVector S)
	{
		auto* A=World->SpawnActor<AStaticMeshActor>(P,FRotator::ZeroRotator);A->SetActorLabel(Name);A->GetStaticMeshComponent()->SetStaticMesh(Mesh);A->GetStaticMeshComponent()->SetMaterial(0,Mat);A->SetActorScale3D(S);return A;
	};
	StageMesh(TEXT("StudioFloor"),Cube,Floor,{0,700,-12},{35,34,.2f});
	StageMesh(TEXT("StudioBackdrop"),Cube,Floor,{210,700,260},{.2f,34,5.4f});
	for(int32 T=0;T<5;++T)
	{
		auto* Machine=World->SpawnActor<AUEGachaMachine>(MachineClass,FVector(0,T*350,0),FRotator::ZeroRotator);Machine->Pool=Pools[T];Machine->SetActorLabel(FString::Printf(TEXT("Gacha_%s"),*Pools[T]->MachineName.ToString()));
		StageMesh(*FString::Printf(TEXT("Pedestal_%d"),T),Cylinder,Black,{0,T*350.f,-1},{1.7f,1.7f,.18f});
		auto* Light=World->SpawnActor<APointLight>(FVector(-205,T*350.f-135,350),FRotator::ZeroRotator);CastChecked<UPointLightComponent>(Light->GetLightComponent())->SetIntensity(14000);CastChecked<UPointLightComponent>(Light->GetLightComponent())->SetAttenuationRadius(950);CastChecked<UPointLightComponent>(Light->GetLightComponent())->SetSourceRadius(95);CastChecked<UPointLightComponent>(Light->GetLightComponent())->SetMobility(EComponentMobility::Movable);
		auto* Rim=World->SpawnActor<APointLight>(FVector(65,T*350.f+150,300),FRotator::ZeroRotator);CastChecked<UPointLightComponent>(Rim->GetLightComponent())->SetIntensity(10000);CastChecked<UPointLightComponent>(Rim->GetLightComponent())->SetAttenuationRadius(650);CastChecked<UPointLightComponent>(Rim->GetLightComponent())->SetSourceRadius(70);CastChecked<UPointLightComponent>(Rim->GetLightComponent())->SetLightColor(Pools[T]->TypeColor);CastChecked<UPointLightComponent>(Rim->GetLightComponent())->SetMobility(EComponentMobility::Movable);
	}
	auto* Sun=World->SpawnActor<ADirectionalLight>(FVector(0,0,500),FRotator(-45,-30,0));Sun->GetLightComponent()->SetIntensity(2.5f);Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
	auto* Sky=World->SpawnActor<ASkyLight>();Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);Sky->GetLightComponent()->SetIntensity(1.4f);
	Save(World);
	World->DestroyWorld(false);
	return bSaveFailed ? 1 : 0;
}

}

UHHVGachaBuildCommandlet::UHHVGachaBuildCommandlet()
{
	IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;
}

int32 UHHVGachaBuildCommandlet::Main(const FString& Params)
{
	// 디자이너에서 팀원이 수정했을 수 있는 기존 에셋은 절대 덮어쓰지 않는다.
	if (FPackageName::DoesPackageExist(Base+TEXT("Blueprints/BP_GachaMachine")))
	{
		UE_LOG(LogTemp, Error, TEXT("Gacha assets already exist. Edit them in the editor; this authoring commandlet will not overwrite them."));return 1;
	}
	UMaterial* Chrome=Material(TEXT("M_Chrome"),FLinearColor(.48f,.55f,.61f),.85f,.19f);
	UMaterial* White=Material(TEXT("M_Porcelain"),FLinearColor(.9f,.94f,.97f),.2f,.24f);
	UMaterial* Black=Material(TEXT("M_Obsidian"),FLinearColor(.009f,.013f,.02f),.15f,.27f);
	UMaterial* Red=Material(TEXT("M_MonsterRed"),FLinearColor(.65f,.008f,.018f),.2f,.2f);
	UMaterial* Blue=Material(TEXT("M_SuperBlue"),FLinearColor(.01f,.08f,.72f),.2f,.2f);
	UMaterial* Gold=Material(TEXT("M_HyperGold"),FLinearColor(1,.56f,.018f),.45f,.24f);
	UMaterial* Glass=Material(TEXT("M_Glass"),FLinearColor(.7f,.87f,.97f),0,.08f,true);
	UMaterial* Glow=Material(TEXT("M_RevealGlow"),FLinearColor(.03f,.4f,3),0,.4f,false,true);
	UMaterial* Floor=Material(TEXT("M_StudioFloor"),FLinearColor(.022f,.031f,.046f),.35f,.3f);
	TArray<UStaticMesh*> Balls={BallMesh(TEXT("SM_MonsterBall"),0,Red,White,Black,Red),BallMesh(TEXT("SM_SuperBall"),1,Blue,White,Black,Red),BallMesh(TEXT("SM_HyperBall"),2,Black,White,Black,Gold)};
	UStaticMesh* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
	UBlueprint* BP=Blueprint(TEXT("Blueprints/BP_GachaMachine"),AUEGachaMachine::StaticClass());
	auto* Default=CastChecked<AUEGachaMachine>(BP->GeneratedClass->GetDefaultObject());
	const USceneComponent* Root=Default->GetRootComponent();
	Part(BP,TEXT("BaseFoot"),Cylinder,Black,{0,0,9},{1.27f,1.27f,.18f},FRotator::ZeroRotator,Root);
	Part(BP,TEXT("LowerChrome"),Cylinder,Chrome,{0,0,25},{1.23f,1.23f,.16f},FRotator::ZeroRotator,Root);
	Part(BP,TEXT("Chassis"),Cylinder,Chrome,{0,0,94},{1.18f,1.18f,1.25f},FRotator::ZeroRotator,Root);
	Part(BP,TEXT("ChamberCollar"),Cylinder,Chrome,{0,0,160},{1.08f,1.08f,.16f},FRotator::ZeroRotator,Root);
	auto* GlassPart=Part(BP,TEXT("GlassChamber"),Sphere,Glass,{0,0,235},{1.68f,1.68f,1.68f},FRotator::ZeroRotator,Root);
	GlassPart->SetCastShadow(false);GlassPart->TranslucencySortPriority=1;
	Part(BP,TEXT("ChromeLid"),Cylinder,Chrome,{0,0,315},{.68f,.68f,.09f},FRotator::ZeroRotator,Root);
	Part(BP,TEXT("LidHandle"),Sphere,Chrome,{0,0,324},{.24f,.24f,.18f},FRotator::ZeroRotator,Root);
	Part(BP,TEXT("CrankPlate"),Cube,Chrome,{-58,0,117},{.10f,.87f,.7f},FRotator::ZeroRotator,Root);
	Part(BP,TEXT("CrankDial"),Cylinder,Black,{-4,0,0},{.64f,.64f,.07f},FRotator(90,0,0),Default->HandlePivot,TEXT("GachaHandle"));
	Part(BP,TEXT("CrankGrip"),Cube,Chrome,{-10,0,0},{.11f,.57f,.13f},FRotator::ZeroRotator,Default->HandlePivot,TEXT("GachaHandle"));
	Part(BP,TEXT("CrankGripLeft"),Sphere,Chrome,{-10,-26,0},{.13f,.13f,.13f},FRotator::ZeroRotator,Default->HandlePivot,TEXT("GachaHandle"));
	Part(BP,TEXT("CrankGripRight"),Sphere,Chrome,{-10,26,0},{.13f,.13f,.13f},FRotator::ZeroRotator,Default->HandlePivot,TEXT("GachaHandle"));
	Part(BP,TEXT("CrankCenter"),Sphere,Chrome,{-15,0,0},{.09f,.16f,.16f},FRotator::ZeroRotator,Default->HandlePivot,TEXT("GachaHandle"));
	Part(BP,TEXT("DeliveryMouth"),Cube,Black,{-61,0,62},{.06f,.64f,.44f},FRotator::ZeroRotator,Root);
	Part(BP,TEXT("MouthLintel"),Cube,White,{-65,0,85},{.13f,.71f,.07f},FRotator::ZeroRotator,Root);
	Part(BP,TEXT("DeliveryTray"),Cube,Chrome,{-83,0,24},{.56f,.76f,.08f},FRotator::ZeroRotator,Root);
	Part(BP,TEXT("TrayLip"),Cube,Chrome,{-111,0,29},{.06f,.76f,.13f},FRotator::ZeroRotator,Root);
	Part(BP,TEXT("RevealStrip"),Cube,Glow,{-65,0,159},{.03f,.65f,.025f},FRotator::ZeroRotator,Root,TEXT("StageGlow"));
	FRandomStream Random(41027);
	TArray<FVector> Positions;
	for (int32 I=0;I<30;++I)
	{
		FVector P;
		for(int32 Attempt=0;Attempt<20000;++Attempt)
		{
			P=FVector(Random.FRandRange(-55,55),Random.FRandRange(-55,55),Random.FRandRange(-59,28));
			bool bFits=P.SizeSquared()<FMath::Square(64.f);
			for(const FVector& Other:Positions) if(FVector::DistSquared(P,Other)<FMath::Square(22.2f)) bFits=false;
			if(bFits)break;
		}
		Positions.Add(P);
		const int32 Tier=I<21?0:I<28?1:2;
		Part(BP,*FString::Printf(TEXT("Capsule_%02d"),I),Balls[Tier],nullptr,P+FVector(0,0,235),FVector(1),FRotator(Random.FRandRange(0,360),Random.FRandRange(0,360),Random.FRandRange(0,360)),Root,TEXT("GachaCapsule"));
	}
	FKismetEditorUtilities::CompileBlueprint(BP);
	Default=CastChecked<AUEGachaMachine>(BP->GeneratedClass->GetDefaultObject());
	for(auto* Mesh:Balls)Default->BallMeshes.Add(Mesh);
	Default->HandlePivot->SetRelativeLocation(FVector(-65,0,117));
	Default->RewardBall->SetStaticMesh(Balls[0]);Default->RewardBall->SetRelativeLocation(FVector(-105,0,40));Default->RewardBall->SetRelativeScale3D(FVector(1.3f));Default->RewardBall->SetVisibility(false);
	Default->Camera->SetRelativeLocation(FVector(-950,0,205));Default->Camera->SetRelativeRotation(FRotator(-2,0,0));Default->Camera->SetFieldOfView(55);
	Default->StageLight->SetRelativeLocation(FVector(-100,0,164));Default->StageLight->SetAttenuationRadius(250);
	Save(BP);

	const TCHAR* TypeNames[]={TEXT("불꽃"),TEXT("물"),TEXT("풀"),TEXT("노말"),TEXT("전기")};
	const TCHAR* TypeIds[]={TEXT("Fire"),TEXT("Water"),TEXT("Grass"),TEXT("Normal"),TEXT("Electric")};
	const TCHAR* Pokemon[5][5]={{TEXT("불꽃숭이"),TEXT("영치코"),TEXT("폭타"),TEXT("파이어로"),TEXT("윈디")},
		{TEXT("팽도리"),TEXT("개굴반장"),TEXT("블로스터"),TEXT("누오"),TEXT("갸라도스")},
		{TEXT("모부기"),TEXT("나무돌이"),TEXT("버섯모"),TEXT("눈설왕"),TEXT("드레디어")},
		{TEXT("나옹"),TEXT("노고치"),TEXT("게을킹"),TEXT("잠만보"),TEXT("붉은달다투곰")},
		{TEXT("꼬링크"),TEXT("데덴네"),TEXT("파치리스"),TEXT("전룡"),TEXT("로토무")}};
	const int32 Dex[5][5]={{390,256,323,663,59},{393,657,693,195,130},{387,253,286,460,549},{52,206,289,143,901},{403,702,417,181,479}};
	const FLinearColor Colors[]={FLinearColor(.9f,.09f,.035f),FLinearColor(.03f,.33f,.85f),FLinearColor(.1f,.6f,.24f),FLinearColor(.62f,.52f,.38f),FLinearColor(1,.65f,.025f)};
	TArray<UUEGachaPool*> Pools;
	for(int32 T=0;T<5;++T)
	{
		auto* Pool=Asset<UUEGachaPool>(FString(TEXT("Data/DA_Gacha_"))+TypeIds[T]);Pool->MachineName=FText::FromString(TypeNames[T]);Pool->TypeColor=Colors[T];Pool->DisplayOrder=T;
		for(int32 I=0;I<5;++I)
		{
			FUEGachaEntry E;E.DisplayName=FText::FromString(Pokemon[T][I]);E.DexNumber=Dex[T][I];E.Rarity=I<2?EUEGachaRarity::Normal:I<4?EUEGachaRarity::Rare:EUEGachaRarity::SuperRare;E.Weight=I<2?35:I<4?12.5f:5;
			const FString SpeciesPath=FString::Printf(TEXT("/Game/Pokemon/SpeciesData/%s/DA_%s.DA_%s"),Pokemon[T][I],Pokemon[T][I],Pokemon[T][I]);
			E.Species=LoadObject<UUEPokemonSpeciesData>(nullptr,*SpeciesPath);Pool->Entries.Add(E);
			if(!E.Species)UE_LOG(LogTemp, Warning,TEXT("Gacha species visual missing; display name and dex remain usable: %s"),Pokemon[T][I]);
		}
		Save(Pool);Pools.Add(Pool);
	}
	UWidgetBlueprint* UI=MakeUI();
	UBlueprint* Controller=Blueprint(TEXT("Blueprints/BP_GachaStudioController"),AUEGachaStudioController::StaticClass());
	CastChecked<AUEGachaStudioController>(Controller->GeneratedClass->GetDefaultObject())->StudioWidgetClass=UI->GeneratedClass;Save(Controller);
	UBlueprint* Mode=Blueprint(TEXT("Blueprints/BP_GachaStudioGameMode"),AUEGachaStudioGameMode::StaticClass());
	CastChecked<AGameModeBase>(Mode->GeneratedClass->GetDefaultObject())->PlayerControllerClass=Controller->GeneratedClass;Save(Mode);
	if (WriteMap(BP->GeneratedClass,Mode->GeneratedClass,Pools) != 0) bSaveFailed = true;
	UE_LOG(LogTemp, Display,TEXT("GACHA_AUTHORING_FINISHED: %s"),bSaveFailed?TEXT("FAILED"):TEXT("SUCCESS"));
	return bSaveFailed?1:0;
}
