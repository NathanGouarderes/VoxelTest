// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FChunckDataStructure.h"
#include "EChunkVariant.h"
#include "../../VoxelModule/Public/Structs/FChunkEditLayer.h"
#include "../../VoxelModule/Public/Structs/FTerrainConfig.h"
#include "FastNoiseLite.h"
class AChunckManager;

struct FChunkGenJob
{
	int32 RenderLOD = 0;
	int32 CollisionLOD;
	bool  bNeedsCollision;
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
	TMap<int32, FVoxelDataStructure> Edits;
	TArray<FVoxelBrushOp> BrushOps;

	TSharedPtr<const FTerrainConfig, ESPMode::ThreadSafe> SharedFTerrainConfig;
	FastNoiseLite SurfaceNoise;
	FastNoiseLite CaveNoise;

	FChunkGenJob() : Coord(FIntVector::ZeroValue), Variant(EChunkVariant::Full){}
	FChunkGenJob(FIntVector InCoord, EChunkVariant InVariant, AChunckManager* Manager, int32 InLOD, int32 InGenerationId);
	
};