// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FChunckDataStructure.h"
#include "FVoxelDataStructure.h"
#include "ChunckManager.h"
#include "FChunkMeshResult.h"
#include "HAL/CriticalSection.h"
#include "VoxelWorld.generated.h"

UCLASS()
class VOXELMODULE_API AVoxelWorld : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AVoxelWorld();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	void GenerateWorld();
	FORCEINLINE int GetVoxelIndex(int x, int y, int z);
	FVoxelDataStructure& GetVoxel(FChunckDataStructure& Chunk, int x, int y, int z);
	void ProcessDirtyChunks();

	static constexpr int ChunckSize = 32;

	TMap<FIntVector, FChunckDataStructure> Chuncks;
	FCriticalSection ChunckMutex;
	TQueue<FChunkMeshResult> MeshUploadQueue;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel")
	AChunckManager* ChunckManager;
	

};
