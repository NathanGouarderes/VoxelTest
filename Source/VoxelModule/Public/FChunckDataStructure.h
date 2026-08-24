#pragma once
#include "CoreMinimal.h"
#include "Math/IntVector.h"
#include "FPackedQuad.h"
#include "FVoxelDataStructure.h"

enum EChunkLODPhase
{
	Stable,
	GeneratingData,
	AwaitingNewMesh,
};

struct FChunckDataStructure
{
	int32 id;
	int32 LOD = 0;
	int32 RenderedByTier = -1;   // -1 = personne, 0 = acteur LOD0, 1/2/3 = tier cluster
	FIntVector Coord;
	TArray<FBrickQuadsRef> BrickQuads;
	int32 MeshVersion;
	TArray<FVoxelDataStructure> Voxels;
	int32 GenerationId = 0;
	bool bIsDirty = true;
	bool bIsQueued = false;
	bool bIsChunckGenerated;
	bool bPendingKill = false;
	EChunkLODPhase Phase = EChunkLODPhase::Stable;
	TWeakObjectPtr<class AVoxelChunck> VoxelChunck;

	TArray<FVoxelDataStructure> PendingVoxels;
	int32 PendingLOD = INDEX_NONE;
	int32 PendingGenerationId = 0;

	int32 ReleaseLOD = INDEX_NONE;
	FIntVector ReleaseClusterCoord = FIntVector::ZeroValue;


	double PhaseEnteredTime = 0.0;
	FChunckDataStructure()
	{
		bIsChunckGenerated = false;
	}
};