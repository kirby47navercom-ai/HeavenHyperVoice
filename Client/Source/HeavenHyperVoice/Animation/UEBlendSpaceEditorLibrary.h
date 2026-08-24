// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UEBlendSpaceEditorLibrary.generated.h"

class UAnimSequence;
class UBlendSpace1D;

/** 에디터에서 1D BlendSpace 샘플과 내부 구간 데이터를 함께 다시 만드는 도구다. */
UCLASS()
class HEAVENHYPERVOICE_API UUEBlendSpaceEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Idle, Walk, Run 샘플을 넣고 UE가 사용하는 내부 구간 데이터를 재생성한다. */
	UFUNCTION(BlueprintCallable, Category = "HeavenHyperVoice|Animation|Editor")
	static bool RebuildLocomotionBlendSpace(
		UBlendSpace1D* BlendSpace,
		UAnimSequence* IdleSequence,
		UAnimSequence* WalkSequence,
		UAnimSequence* RunSequence,
		float WalkSpeed = 260.0f,
		float RunSpeed = 390.0f);
};
