// Copyright Epic Games, Inc. All Rights Reserved.

#include "UEFollowerAnimInstance.h"

#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

namespace
{
void BuildReferenceComponentTransforms(
	const FReferenceSkeleton& ReferenceSkeleton,
	TMap<FName, FTransform>& OutTransforms)
{
	OutTransforms.Reset();
	const TArray<FTransform>& ReferencePose = ReferenceSkeleton.GetRefBonePose();
	OutTransforms.Reserve(ReferencePose.Num());
	for (int32 BoneIndex = 0; BoneIndex < ReferencePose.Num(); ++BoneIndex)
	{
		FTransform ComponentTransform = ReferencePose[BoneIndex];
		const int32 ParentIndex = ReferenceSkeleton.GetParentIndex(BoneIndex);
		if (ParentIndex != INDEX_NONE)
		{
			const FTransform* ParentTransform = OutTransforms.Find(ReferenceSkeleton.GetBoneName(ParentIndex));
			if (ParentTransform)
			{
				ComponentTransform *= *ParentTransform;
			}
		}
		ComponentTransform.NormalizeRotation();
		OutTransforms.Emplace(ReferenceSkeleton.GetBoneName(BoneIndex), ComponentTransform);
	}
}

FVector GetSafeScaleRatio(const FVector& Numerator, const FVector& Denominator)
{
	return FVector(
		FMath::IsNearlyZero(Denominator.X) ? 1.0 : Numerator.X / Denominator.X,
		FMath::IsNearlyZero(Denominator.Y) ? 1.0 : Numerator.Y / Denominator.Y,
		FMath::IsNearlyZero(Denominator.Z) ? 1.0 : Numerator.Z / Denominator.Z);
}
}

FUEFollowerAnimInstanceProxy::FUEFollowerAnimInstanceProxy()
{
	CopyPoseNode.bUseAttachedParent = true;
	CopyPoseNode.bCopyCustomAttributes = false;
}

FUEFollowerAnimInstanceProxy::FUEFollowerAnimInstanceProxy(UAnimInstance* InAnimInstance)
	: FAnimInstanceProxy(InAnimInstance)
{
	CopyPoseNode.bUseAttachedParent = true;
	CopyPoseNode.bCopyCustomAttributes = false;
}

void FUEFollowerAnimInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);
	FAnimationInitializeContext InitializeContext(this);
	CopyPoseNode.Initialize_AnyThread(InitializeContext);
}

void FUEFollowerAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	if (!CopyPoseNode.SourceMeshComponent.IsValid())
	{
		if (USkeletalMeshComponent* TargetComponent = GetSkelMeshComponent())
		{
			CopyPoseNode.SourceMeshComponent =
				Cast<USkeletalMeshComponent>(TargetComponent->GetAttachParent());
		}
	}
	if (const UUEFollowerAnimInstance* FollowerInstance = Cast<UUEFollowerAnimInstance>(InAnimInstance))
	{
		CopyPoseNode.bCopyCurves = FollowerInstance->ShouldCopyCurves();
		bUseTargetEyeReferencePose = FollowerInstance->ShouldUseTargetEyeReferencePose();
	}

	bHasLeftEyeReference = false;
	bHasRightEyeReference = false;
	SourceReferenceComponentTransforms.Reset();
	TargetReferenceComponentTransforms.Reset();
	if (const USkeletalMeshComponent* SourceComponent = CopyPoseNode.SourceMeshComponent.Get())
	{
		if (const USkeletalMesh* SourceMesh = SourceComponent->GetSkeletalMeshAsset())
		{
			BuildReferenceComponentTransforms(
				SourceMesh->GetRefSkeleton(),
				SourceReferenceComponentTransforms);
		}
	}
	if (const USkeletalMeshComponent* TargetComponent = GetSkelMeshComponent())
	{
		if (const USkeletalMesh* TargetMesh = TargetComponent->GetSkeletalMeshAsset())
		{
			const FReferenceSkeleton& ReferenceSkeleton = TargetMesh->GetRefSkeleton();
			const TArray<FTransform>& ReferencePose = ReferenceSkeleton.GetRefBonePose();
			BuildReferenceComponentTransforms(
				ReferenceSkeleton,
				TargetReferenceComponentTransforms);

			if (bUseTargetEyeReferencePose)
			{
				const int32 LeftEyeIndex = ReferenceSkeleton.FindBoneIndex(TEXT("eyes_l"));
				const int32 RightEyeIndex = ReferenceSkeleton.FindBoneIndex(TEXT("eyes_r"));
				if (ReferencePose.IsValidIndex(LeftEyeIndex))
				{
					LeftEyeReferenceTranslation = ReferencePose[LeftEyeIndex].GetTranslation();
					bHasLeftEyeReference = true;
				}
				if (ReferencePose.IsValidIndex(RightEyeIndex))
				{
					RightEyeReferenceTranslation = ReferencePose[RightEyeIndex].GetTranslation();
					bHasRightEyeReference = true;
				}
			}
		}
	}
	CopyPoseNode.PreUpdate(InAnimInstance);
}

bool FUEFollowerAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	CopyPoseNode.Evaluate_AnyThread(Output);
	const FBoneContainer& BoneContainer = Output.Pose.GetBoneContainer();
	FCSPose<FCompactPose> ComponentPose;
	ComponentPose.InitPose(Output.Pose);
	TArray<FBoneTransform> RetargetedTransforms;
	RetargetedTransforms.Reserve(TargetReferenceComponentTransforms.Num());

	for (const TPair<FName, FTransform>& TargetReference : TargetReferenceComponentTransforms)
	{
		const FTransform* SourceReference = SourceReferenceComponentTransforms.Find(TargetReference.Key);
		if (!SourceReference)
		{
			continue;
		}

		const int32 MeshBoneIndex = BoneContainer.GetPoseBoneIndexForBoneName(TargetReference.Key);
		const FCompactPoseBoneIndex CompactIndex =
			BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshBoneIndex));
		if (!CompactIndex.IsValid())
		{
			continue;
		}

		const FTransform SourceCurrent = ComponentPose.GetComponentSpaceTransform(CompactIndex);
		FTransform TargetCurrent = TargetReference.Value;
		TargetCurrent.SetTranslation(
			TargetReference.Value.GetTranslation() +
			(SourceCurrent.GetTranslation() - SourceReference->GetTranslation()));
		TargetCurrent.SetRotation((
			SourceCurrent.GetRotation() *
			SourceReference->GetRotation().Inverse() *
			TargetReference.Value.GetRotation()).GetNormalized());
		TargetCurrent.SetScale3D(
			TargetReference.Value.GetScale3D() *
			GetSafeScaleRatio(SourceCurrent.GetScale3D(), SourceReference->GetScale3D()));
		RetargetedTransforms.Emplace(CompactIndex, TargetCurrent);
	}

	if (!RetargetedTransforms.IsEmpty())
	{
		RetargetedTransforms.Sort([](const FBoneTransform& Left, const FBoneTransform& Right)
		{
			return Left.BoneIndex.GetInt() < Right.BoneIndex.GetInt();
		});
		ComponentPose.SafeSetCSBoneTransforms(RetargetedTransforms);
		FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(ComponentPose, Output.Pose);
	}

	if (bUseTargetEyeReferencePose)
	{
		// 몸과 머리의 눈 본 위치가 서로 다르므로 회전은 복사하고 위치만 머리 기준으로 되돌린다.
		const auto RestoreEyeTranslation = [&Output, &BoneContainer](
			const FName BoneName,
			const bool bHasReference,
			const FVector& ReferenceTranslation)
		{
			if (!bHasReference)
			{
				return;
			}

			const int32 MeshBoneIndex = BoneContainer.GetPoseBoneIndexForBoneName(BoneName);
			const FCompactPoseBoneIndex CompactIndex =
				BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshBoneIndex));
			if (CompactIndex.IsValid())
			{
				Output.Pose[CompactIndex].SetTranslation(ReferenceTranslation);
			}
		};

		RestoreEyeTranslation(TEXT("eyes_l"), bHasLeftEyeReference, LeftEyeReferenceTranslation);
		RestoreEyeTranslation(TEXT("eyes_r"), bHasRightEyeReference, RightEyeReferenceTranslation);
	}
	return true;
}

void FUEFollowerAnimInstanceProxy::UpdateAnimationNode(const FAnimationUpdateContext& InContext)
{
	UpdateCounter.Increment();
	CopyPoseNode.Update_AnyThread(InContext);
}

FAnimInstanceProxy* UUEFollowerAnimInstance::CreateAnimInstanceProxy()
{
	return new FUEFollowerAnimInstanceProxy(this);
}

void UUEFollowerAnimInstance::SetCopyCurves(bool bInCopyCurves)
{
	bCopyCurves = bInCopyCurves;
}

void UUEFollowerAnimInstance::SetUseTargetEyeReferencePose(bool bInUseTargetEyeReferencePose)
{
	bUseTargetEyeReferencePose = bInUseTargetEyeReferencePose;
}

void UUEFollowerAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}
