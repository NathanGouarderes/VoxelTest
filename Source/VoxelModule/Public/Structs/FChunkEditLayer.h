#pragma once
#include "CoreMinimal.h"
#include "Math/IntVector.h"
#include "../FVoxelDataStructure.h"
#include "FChunkEditLayer.generated.h"


UENUM()
enum class EVoxelBrushType : int8
{
	Sphere,
	Cone,
	Laser,
};

USTRUCT()
struct FVoxelBrushOp
{
	GENERATED_BODY()
	UPROPERTY()
	EVoxelBrushType Type = EVoxelBrushType::Sphere;
	UPROPERTY()
	FIntVector CenterVoxel = FIntVector::ZeroValue;
	UPROPERTY()
	int32 RadiusVoxels = 0;
	UPROPERTY()
	int8 MaterialId = 0;
	UPROPERTY()
	int32 Seq = 0;
};

USTRUCT()
struct FChunkEditLayer
{
	GENERATED_BODY()
	TMap<int32, FVoxelDataStructure> Edits; // la clef est l'index local du voxel édité (de 0 à 127)
	UPROPERTY()
	uint32 Revision = 0;
	UPROPERTY()
	TArray<FVoxelBrushOp> BrushOps;
};