#include "PokemonAnimationEditorLibrary.h"
#include "Animation/BlendSpace.h"

int32 UPokemonAnimationEditorLibrary::GetBlendSpaceSegmentCount(UBlendSpace* BlendSpace)
{
	return BlendSpace ? BlendSpace->GetBlendSpaceData().Segments.Num() : 0;
}

bool UPokemonAnimationEditorLibrary::RebuildBlendSpace(UBlendSpace* BlendSpace)
{
	if (!BlendSpace) return false;
	BlendSpace->Modify();
	BlendSpace->ValidateSampleData();
	BlendSpace->ResampleData();
	BlendSpace->MarkPackageDirty();
	return !BlendSpace->GetBlendSpaceData().IsEmpty() || !BlendSpace->GetGridSamples().IsEmpty();
}

bool UPokemonAnimationEditorLibrary::NeedsBlendSpaceRebuild(UBlendSpace* BlendSpace)
{
	if (!BlendSpace) return true;
	const auto& Samples = BlendSpace->GetBlendSamples();
	const auto& Segments = BlendSpace->GetBlendSpaceData().Segments;
	if (Segments.IsEmpty()) return BlendSpace->GetGridSamples().IsEmpty();
	const auto& Axis = BlendSpace->GetBlendParameter(0);
	const float Range = Axis.Max - Axis.Min;
	if (Range <= 0.f) return true;
	for (const auto& Segment : Segments)
	{
		for (int32 Vertex = 0; Vertex < 2; ++Vertex)
		{
			const int32 Index = Segment.SampleIndices[Vertex];
			if (!Samples.IsValidIndex(Index) || !Samples[Index].Animation) return true;
			const float Expected = (Samples[Index].SampleValue.X - Axis.Min) / Range;
			if (!FMath::IsNearlyEqual(Expected, Segment.Vertices[Vertex], 0.0001f)) return true;
		}
	}
	return false;
}
