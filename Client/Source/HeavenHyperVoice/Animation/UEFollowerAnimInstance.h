// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_CopyPoseFromMesh.h"
#include "UEFollowerAnimInstance.generated.h"

/**
 * 부모 SkeletalMeshComponent의 포즈를 본 이름으로 복사하는 프록시다.
 * 머리, 머리카락, 의상처럼 본 순서가 다른 커마 파츠에 사용한다.
 */
USTRUCT()
struct FUEFollowerAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FUEFollowerAnimInstanceProxy();
	explicit FUEFollowerAnimInstanceProxy(UAnimInstance* InAnimInstance);

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;
	virtual void UpdateAnimationNode(const FAnimationUpdateContext& InContext) override;

private:
	FAnimNode_CopyPoseFromMesh CopyPoseNode;
	bool bUseTargetEyeReferencePose = false;
	bool bHasLeftEyeReference = false;
	bool bHasRightEyeReference = false;
	FVector LeftEyeReferenceTranslation = FVector::ZeroVector;
	FVector RightEyeReferenceTranslation = FVector::ZeroVector;
	TMap<FName, FTransform> SourceReferenceComponentTransforms;
	TMap<FName, FTransform> TargetReferenceComponentTransforms;
};

/**
 * 커마 파츠 전용 AnimInstance다.
 * 별도 AnimBlueprint 없이 부착된 부모 메시의 현재 포즈를 그대로 따라간다.
 */
UCLASS(Transient, NotBlueprintable)
class HEAVENHYPERVOICE_API UUEFollowerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** 원본 파츠 AnimBP와 동일하게 곡선 복사 여부를 설정한다. */
	void SetCopyCurves(bool bInCopyCurves);
	/** 몸 포즈 복사 뒤 머리 메시 고유의 눈 위치를 복원할지 설정한다. */
	void SetUseTargetEyeReferencePose(bool bInUseTargetEyeReferencePose);

	bool ShouldCopyCurves() const { return bCopyCurves; }
	bool ShouldUseTargetEyeReferencePose() const { return bUseTargetEyeReferencePose; }

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	bool bCopyCurves = false;
	bool bUseTargetEyeReferencePose = false;
};
