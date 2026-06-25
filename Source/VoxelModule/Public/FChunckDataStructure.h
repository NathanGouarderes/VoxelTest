#pragma once
#include "CoreMinimal.h"
#include "Math/IntVector.h"
#include "FVoxelDataStructure.h"

struct FChunckDataStructure
{
	int32 id;
	int32 LOD = 0;
	FIntVector Coord;
	TArray<FVoxelDataStructure> Voxels;
	int32 GenerationId = 0;
	bool bIsDirty = true;	
	bool bIsQueued = false;
	bool bIsChunckGenerated;
	bool bPendingKill = false;
	TWeakObjectPtr<class AVoxelChunck> VoxelChunck;
	FChunckDataStructure()
	{
		bIsChunckGenerated = false;
	}
};

