#pragma once

#include "CoreMinimal.h"
#include "FVoxelDataStructure.h"

struct FChunkGenResult
{
    FIntVector Coord;
    TArray<FVoxelDataStructure> Voxels;
};