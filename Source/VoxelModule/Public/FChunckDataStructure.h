#pragma once
#include "CoreMinimal.h"
#include "Math/IntVector.h"
#include "FVoxelDataStructure.h"

struct FChunckDataStructure
{
	FIntVector Coord;
	TArray<FVoxelDataStructure> Voxels;
	bool bIsDirty;	

};

