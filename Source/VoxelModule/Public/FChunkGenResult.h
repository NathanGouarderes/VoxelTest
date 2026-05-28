#pragma once

#include "CoreMinimal.h"
#include "FVoxelDataStructure.h"
#include "FChunckDataStructure.h"

struct FChunkGenResult
{
    FIntVector Coord;
    int32 LOD;
    TArray<FVoxelDataStructure> Voxels;
    TArray<FChunckDataStructure> ChunkData;
    bool bIsAllSolid;
    bool bIsAllEmpty;
};