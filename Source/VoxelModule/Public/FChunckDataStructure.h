#pragma once
#include "CoreMinimal.h"
#include "Math/IntVector.h"
#include "FPackedQuad.h"
#include "CoreGlobals.h"
#include "FVoxelDataStructure.h"

enum EChunkLODPhase
{
	Stable,
	GeneratingData,
	AwaitingNewMesh,
};

struct FVoxelBuffer
{
	using FArray = TArray<FVoxelDataStructure>;

	FVoxelBuffer() = default;
	/** Remplace le contenu par un nouveau tableau (resultat de generation, commit de LOD). */
	FVoxelBuffer& operator=(FArray&& In)
	{
		Ptr = MakeShared<FArray, ESPMode::ThreadSafe>(MoveTemp(In));
		return *this;
	}

	int32 Num() const { return Ptr.IsValid() ? Ptr->Num() : 0; }
	bool  IsValidIndex(int32 Index) const { return Ptr.IsValid() && Ptr->IsValidIndex(Index); }
	const FVoxelDataStructure& operator[](int32 Index) const { return (*Ptr)[Index]; }
	FArray& Edit()
	{
		check(IsInGameThread());   //En Development, une ecriture depuis un worker plante ICI, pile d'appels a l'appui
		if (!Ptr.IsValid())
		{
			Ptr = MakeShared<FArray, ESPMode::ThreadSafe>();
		}
		else if (!Ptr.IsUnique())
		{
			//  Un job lit encore ce tableau : on ecrit dans une copie privee.
			Ptr = MakeShared<FArray, ESPMode::ThreadSafe>(*Ptr);
		}
		return *Ptr;
	}
private:
	TSharedPtr<FArray, ESPMode::ThreadSafe> Ptr;
};

struct FChunckDataStructure
{
	int32 id;
	int32 LOD = 0;
	int32 RenderedByTier = -1;   // -1 = personne, 0 = acteur LOD0, 1/2/3 = tier cluster
	FIntVector Coord;
	TArray<FBrickQuadsRef> BrickQuads;
	int32 MeshVersion;
	FVoxelBuffer Voxels;
	int32 GenerationId = 0;
	bool bIsDirty = true;
	bool bIsQueued = false;
	bool bIsChunckGenerated;
	bool bPendingKill = false;
	bool bIsAllSolid = false;
	bool bPendingIsAllSolid = false;
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