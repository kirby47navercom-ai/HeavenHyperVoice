// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UEServerMapFile.h"

class HEAVENHYPERVOICE_API FUEServerHeightMapBuilder
{
public:
	static void BuildFromGroundObbs(
		const TArray<FUEServerMapObb>& GroundObbs,
		float CellSize,
		FUEServerHeightMapData& OutHeightMap
	);
};
