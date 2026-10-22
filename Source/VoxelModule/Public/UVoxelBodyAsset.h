// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Structs/FVoxelBodyCell.h"
#include "UVoxelBodyAsset.generated.h"

/**
 * 
 */
UCLASS()
class VOXELMODULE_API UVoxelBodyAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "Voxel")
	double CellSize = 2.5;
	UPROPERTY(VisibleAnywhere, Category = "Voxel")
	FVector Origin = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, Category = "Voxel")
	FIntVector Dims = FIntVector::ZeroValue;
	UPROPERTY(VisibleAnywhere, Category = "Voxel")
	TArray<FVoxelBodyCell> Cells;
	UPROPERTY(VisibleAnywhere, Category = "Voxel")
	TArray<FName> BoneNames;
	UPROPERTY(VisibleAnywhere, Category = "Voxel")
	TObjectPtr<USkeleton> Skeleton = nullptr;
private:
	
};
