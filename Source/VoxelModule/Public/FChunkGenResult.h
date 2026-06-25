#pragma once

#include "CoreMinimal.h"
#include "FVoxelDataStructure.h"
#include "FChunckDataStructure.h"
//#include "FChunckMeshData.h"
#include "FChunkMeshResult.h"

struct FChunkGenResult
{
    FIntVector Coord;
    int32 LOD = 0;
    TArray<FVoxelDataStructure> Voxels;
    TArray<FChunckDataStructure> ChunkData;
    TArray<FChunkMeshResult> ChunkMeshData;
    bool bIsAllSolid;
    bool bIsAllEmpty;
};