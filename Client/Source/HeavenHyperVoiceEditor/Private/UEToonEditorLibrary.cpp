#include "UEToonEditorLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionSubstrate.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Engine/ToonProfile.h"

namespace
{
	template<class T> T* AddNode(UMaterial* Material, int32 X, int32 Y)
	{
		T* Node = NewObject<T>(Material, NAME_None, RF_Transactional);
		Node->Material = Material;
		Node->MaterialExpressionEditorX = X;
		Node->MaterialExpressionEditorY = Y;
		Node->UpdateMaterialExpressionGuid(true, false);
		Material->GetExpressionCollection().AddExpression(Node);
		return Node;
	}
	FExpressionInput Link(UMaterialExpression* Node)
	{
		FExpressionInput Input;
		Input.Connect(0, Node);
		return Input;
	}
	FExpressionInput Scalar(UMaterial* Material, const FScalarMaterialInput& Input, float Default, int32 Y)
	{
		if (Input.Expression && !Input.UseConstant) return Input;
		auto* Node = AddNode<UMaterialExpressionConstant>(Material, 200, Y);
		Node->R = Input.UseConstant ? Input.Constant : Default;
		return Link(Node);
	}
	FExpressionInput Color(UMaterial* Material, const FColorMaterialInput& Input, FLinearColor Default, int32 Y)
	{
		if (Input.Expression && !Input.UseConstant) return Input;
		auto* Node = AddNode<UMaterialExpressionConstant3Vector>(Material, 200, Y);
		Node->Constant = Input.UseConstant ? Input.Constant : Default;
		return Link(Node);
	}
	FExpressionInput Parameter(UMaterial* Material, const TCHAR* Name, float Default, int32 X, int32 Y)
	{
		auto* Node = AddNode<UMaterialExpressionScalarParameter>(Material, X, Y);
		Node->ParameterName = Name;
		Node->Group = TEXT("HHV Toon");
		Node->DefaultValue = Default;
		Node->SliderMin = 0.f;
		Node->SliderMax = FString(Name).Contains(TEXT("Exponent")) ? 10.f : 1.f;
		Node->UpdateParameterGuid(true, false);
		return Link(Node);
	}
}

bool UUEToonEditorLibrary::ConvertLegacyMaterial(UMaterial* Material, UToonProfile* Profile)
{
	if (!Material || !Profile || Material->MaterialDomain != MD_Surface || Material->bUseMaterialAttributes) return false;
	auto* Data = Material->GetEditorOnlyData();
	if (!Data) return false;
	if (auto* Existing = Cast<UMaterialExpressionSubstrateToonBSDF>(Data->FrontMaterial.Expression))
	{
		Existing->Modify();
		Existing->ToonProfile = Profile;
		Material->PostEditChange();
		return true;
	}
	// Do not discard a hand-authored Substrate graph or a translucent surface.
	if (Data->FrontMaterial.Expression || (Material->BlendMode != BLEND_Opaque && Material->BlendMode != BLEND_Masked)) return false;
	Material->Modify();
	auto* Toon = AddNode<UMaterialExpressionSubstrateToonBSDF>(Material, 1600, 0);
	Toon->ToonProfile = Profile;
	Toon->BaseColor = Color(Material, Data->BaseColor, FLinearColor(.18f,.18f,.18f), -400);
	Toon->Metallic = Scalar(Material, Data->Metallic, 0.f, -300);
	auto* Specular = AddNode<UMaterialExpressionMultiply>(Material, 800, -240);
	Specular->A = Scalar(Material, Data->Specular, .5f, -200);
	Specular->B = Parameter(Material, TEXT("ToonSpecularScale"), .5f, 400, -150);
	Toon->Specular = Link(Specular);
	auto* Roughness = AddNode<UMaterialExpressionLinearInterpolate>(Material, 1000, 0);
	Roughness->A = Scalar(Material, Data->Roughness, .5f, -100);
	Roughness->B = Parameter(Material, TEXT("ToonRoughness"), .7f, 400, 0);
	Roughness->Alpha = Parameter(Material, TEXT("ToonRoughnessOverride"), 0.f, 650, 100);
	Toon->Roughness = Link(Roughness);
	Toon->Anisotropy = Parameter(Material, TEXT("ToonAnisotropy"), 0.f, 1000, 200);
	if (Data->Normal.Expression && !Data->Normal.UseConstant)
	{
		auto* Lerp = AddNode<UMaterialExpressionLinearInterpolate>(Material, 750, 450);
		auto* Flat = AddNode<UMaterialExpressionConstant3Vector>(Material, 200, 400);
		Flat->Constant = FLinearColor(0,0,1);
		Lerp->A = Link(Flat);
		Lerp->B = Data->Normal;
		Lerp->Alpha = Parameter(Material, TEXT("ToonNormalStrength"), .5f, 400, 550);
		auto* Normal = AddNode<UMaterialExpressionNormalize>(Material, 1000, 450);
		Normal->VectorInput = Link(Lerp);
		Toon->Normal = Link(Normal);
	}
	auto* Fresnel = AddNode<UMaterialExpressionFresnel>(Material, 200, 750);
	Fresnel->ExponentIn = Parameter(Material, TEXT("ToonRimExponent"), 5.f, 0, 700);
	Fresnel->BaseReflectFraction = 0.f;
	Fresnel->Normal = Link(AddNode<UMaterialExpressionVertexNormalWS>(Material, 0, 850));
	auto* RimWeight = AddNode<UMaterialExpressionMultiply>(Material, 450, 800);
	RimWeight->A = Link(Fresnel);
	RimWeight->B = Parameter(Material, TEXT("ToonRimStrength"), .025f, 200, 950);
	auto* Rim = AddNode<UMaterialExpressionMultiply>(Material, 800, 800);
	Rim->A = Toon->BaseColor;
	Rim->B = Link(RimWeight);
	auto* Glow = AddNode<UMaterialExpressionMultiply>(Material, 800, 1100);
	Glow->A = Toon->BaseColor;
	Glow->B = Parameter(Material, TEXT("ToonEmissionStrength"), 0.f, 400, 1150);
	auto* Emission = AddNode<UMaterialExpressionAdd>(Material, 1000, 850);
	Emission->A = Color(Material, Data->EmissiveColor, FLinearColor::Black, 1000);
	Emission->B = Link(Rim);
	auto* Sum = AddNode<UMaterialExpressionAdd>(Material, 1300, 900);
	Sum->A = Link(Emission);
	Sum->B = Link(Glow);
	Toon->EmissiveColor = Link(Sum);
	Data->FrontMaterial.Connect(0, Toon);
	// Opacity mask, WPO, UVs and all original named parameters remain intact.
	Material->PostEditChange();
	Material->MarkPackageDirty();
	return true;
}

void UUEToonEditorLibrary::SetInstanceProfile(UMaterialInstanceConstant* Instance, UToonProfile* Profile)
{
	if (!Instance || !Profile) return;
	Instance->Modify();
	Instance->ToonProfileOverride = Profile;
	Instance->bOverrideToonProfile = true;
	Instance->PostEditChange();
	Instance->MarkPackageDirty();
}

UToonProfile* UUEToonEditorLibrary::GetInstanceProfile(const UMaterialInstanceConstant* Instance)
{
	return Instance && Instance->bOverrideToonProfile ? Instance->ToonProfileOverride.Get() : nullptr;
}
