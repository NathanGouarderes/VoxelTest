#pragma once
#include "CoreMinimal.h"
#include "Math/IntVector.h"
#include "VoxelChunck.h"
#include "FVoxelDataStructure.h"

struct FChunckDataStructure
{
	int32 id;
	FIntVector Coord;
	TArray<FVoxelDataStructure> Voxels;
	int8 ChunckSize = 64;
	bool bIsDirty = true;	
	AVoxelChunck* VoxelChunck;
	FChunckDataStructure()
	{
		Voxels.SetNum(ChunckSize * ChunckSize * ChunckSize);
		bool bIsGenerated = false;
	}
};

