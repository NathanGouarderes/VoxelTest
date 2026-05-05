// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FChunckDataStructure.h"
#include "EChunkVariant.h"

struct FChunkGenJob
{
	FIntVector Coord;
	EChunkVariant Variant;

	FChunkGenJob() : Coord(FIntVector::ZeroValue), Variant(EChunkVariant::Full){}

	FChunkGenJob(FIntVector InCoord, EChunkVariant InVariant) : Coord(InCoord), Variant(InVariant) {}
};