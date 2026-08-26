// Copyright Epic Games, Inc. All Rights Reserved.

#include "UEBlendSpaceEditorLibrary.h"

#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"

bool UUEBlendSpaceEditorLibrary::RebuildLocomotionBlendSpace(
	UBlendSpace1D* BlendSpace,
	UAnimSequence* IdleSequence,
	UAnimSequence* WalkSequence,
	UAnimSequence* RunSequence,
	const float WalkSpeed,
	const float RunSpeed)
{
#if WITH_EDITOR
	if (!BlendSpace || !IdleSequence || !WalkSequence || !RunSequence)
	{
		return false;
	}

	BlendSpace->Modify();
	if (!BlendSpace->GetSkeleton())
	{
		// 메시 재임포트 뒤 Skeleton 참조만 끊긴 BlendSpace는 샘플의 현재 스켈레톤으로 복구한다.
		BlendSpace->SetSkeleton(IdleSequence->GetSkeleton());
	}

	while (BlendSpace->GetNumberOfBlendSamples() > 0)
	{
		BlendSpace->DeleteSample(BlendSpace->GetNumberOfBlendSamples() - 1);
	}

	const int32 IdleIndex = BlendSpace->AddSample(IdleSequence, FVector::ZeroVector);
	const int32 WalkIndex = BlendSpace->AddSample(WalkSequence, FVector(WalkSpeed, 0.0f, 0.0f));
	const int32 RunIndex = BlendSpace->AddSample(RunSequence, FVector(RunSpeed, 0.0f, 0.0f));
	if (IdleIndex == INDEX_NONE || WalkIndex == INDEX_NONE || RunIndex == INDEX_NONE)
	{
		return false;
	}

	BlendSpace->ValidateSampleData();
	BlendSpace->ResampleData();
	BlendSpace->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}
