// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class AChunckManager;
class AVoxelWorld;
struct FChunckDataStructure;

enum class EVoxelOccupancy : uint8
{
	Air,
	Solid,
	Unknown
};

struct FVoxelSweepHit
{
	bool bHit = false;
	bool bStartedInside = false;
	bool bUnknown = false;
	FIntVector Voxel = FIntVector::ZeroValue;
	FVector Point = FVector::ZeroVector;
	FVector Normal = FVector::ZeroVector;


};

class VOXELMODULE_API FVoxelQueryScope
{
public:
	explicit FVoxelQueryScope(AChunckManager& InManager);
	FVoxelQueryScope(const FVoxelQueryScope&) = delete;
	FVoxelQueryScope& operator=(const FVoxelQueryScope&) = delete;
	~FVoxelQueryScope();

	bool IsValid() const { return World != nullptr; }

	EVoxelOccupancy Sample(const FIntVector& VoxelGlobal);
	FVoxelSweepHit Sweep(const FVector& FromCm, const FVector& ToCm);
	bool FindNearestExit(const FVector& PosCm, FVector& OutPoint, FVector& OutNormal);
	FIntVector WorldToVoxel(const FVector& PosCm) const;

private:
	AChunckManager& ChunkManager;
	AVoxelWorld* World = nullptr;

	FIntVector CachedChunkCoord = FIntVector::ZeroValue;
	FChunckDataStructure* CachedChunk = nullptr;
	bool bHasCache = false;
};
