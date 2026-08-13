// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FChunckDataStructure.h"
#include "EChunkVariant.h"
#include "FVoxelDataStructure.h"
#include "FastNoiseLite.h"
class AChunckManager;

/*
Monde (cm)        → voxel global :   gx = FloorToInt(WorldX / VoxelSize)
Voxel global      → chunk :          cx = FloorDivInt(gx, ChunkSize)
Voxel global      → local LOD0 :     lx = gx - cx * ChunkSize        [0, 128[
Local LOD0        → index :          idx0 = lx + ly*128 + lz*128*128

Cellule stockage au LOD L → local LOD0 :   lx0 = x << L
*/

struct FChunkGenJob
{
	int32 LOD = 0;
	int32 GenerationId = 0;
	FIntVector Coord;
	EChunkVariant Variant;
	int32 ChunkSize = 128; // Valeur par défaut de sécurité
	float SurfaceAmplitude = 50.0f;
	float SurfaceWavelength = 2500.0f;
	int32 BaseHeight = 64;
	float CaveFrequency = 0.01f;
	float CaveThreshold = 0.5f;
	int32 SeaLevel = 24;
	FastNoiseLite SurfaceNoise;
	FastNoiseLite CaveNoise;
    TMap<int32, FVoxelDataStructure> Edits;

	FChunkGenJob() : Coord(FIntVector::ZeroValue), Variant(EChunkVariant::Full){}
	FChunkGenJob(FIntVector InCoord, EChunkVariant InVariant, AChunckManager* Manager, int32 InLOD, int32 InGenerationId);
	
};
