#pragma once
#include "CoreMinimal.h"
#include "FTerrainGenerator.h"

struct FColumnBounds
{
	float MinH = 0.0f;
	float MaxH = 0.0f;
};
struct FDesiredPlayer
{
	FVector    Pos = FVector::ZeroVector;
	FIntVector Center = FIntVector::ZeroValue;
};
struct FDesiredRequest
{
	TArray<FDesiredPlayer> Players;
	int32 HR = 0;
	int32 VR = 0;
	int32 ChunkSize = 32;
	int32 VoxelSize = 10;
	TArray<float> LODDistances;
	int32 MaxLOD = 3;
	int32 MaxSurfaceVoxel = 0;
	TSet<FIntVector> Edited;
	TSharedPtr<const FTerrainConfig, ESPMode::ThreadSafe> Config;
	FIntVector SortCenter = FIntVector::ZeroValue;
};
struct FDesiredResult
{
	bool bValid = false;
	TMap<FIntVector, int32> Desired;                  // coord -> LOD
	TArray<FIntVector> SortedCoords;                  // les memes, triees par distance a SortCenter
	TArray<TPair<FIntPoint, FColumnBounds>> NewColumns;
	int32  CulledAir = 0;
	int32  ColumnCacheSize = 0;
	double ComputeMs = 0.0;
};

class FDesiredSetBuilder
{
public:
	void Compute(const FDesiredRequest& Req, FDesiredResult & Out);
private:
	FTerrainGenerator Generator;
	TMap<FIntPoint, FColumnBounds> ColumnCache;
};