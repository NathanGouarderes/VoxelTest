// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelChunck.h"
#include "FChunckDataStructure.h"
#include "FastNoiseLite.h"
#include "Engine/World.h"
#include "HAL/RunnableThread.h"
#include "EChunkVariant.h"
#include "FChunckGenJob.h"
#include "FChunkGenResult.h"
#include "FClusterGenData.h"
#include "ChunckManager.generated.h"

class ChunckGenWorker;
class AVoxelWorld;
class AClusterChunk;

UCLASS()
class VOXELMODULE_API AChunckManager : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AChunckManager();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	void RegisterDirtyChunk(FIntVector Coord);
	void SpawnChunk(FIntVector Coord, int32 LOD);
	void UpdateVisibleChunks(const TSet<FIntVector>& ChunksToKeep);
	void GetAllPlayerChunks(TSet<FIntVector>& GlobalChunksToKeep);
	FIntVector GetPlayerChunck(const FVector& PlayerPos) const;
	void GenerateTerrain(FChunckDataStructure& Data, FIntVector Coord);
	void FillChunck(EChunkVariant Variant, FIntVector Coord);
	int32 GetLODForChunck(const FIntVector& Coord, const FVector& PlayerPos) const;
	FIntVector GetClusterCoord(FIntVector Coord, int LOD);
	void ApplyMeshToCluster(const TArray<FChunckMeshData>& ChunkMeshData, TMap<FIntVector, UProceduralMeshComponent*>& ClusterPool, FIntVector ClusterCoord, int32 LOD);
	float GetNoise(float WorldX, float WorldY);
	void InitNoise();

	TSet<FIntVector> DirtyChuncks;
	UPROPERTY(EditAnywhere, Category = "Voxel")
	TSubclassOf<AVoxelChunck> VoxelChunckClass;

	UPROPERTY()
	AVoxelWorld* VoxelWorld;
	UPROPERTY(EditAnywhere)
	int VoxelSize = 10;

	UPROPERTY(EditAnywhere)
	int ChunkSize = 32;

	//void UpdateVisibleChunks(const FVector& PlayerLocation);
	int32 HorizontalViewDistance = 20;
	int32 VerticalViewDistance = 15;

	UPROPERTY(EditAnywhere, Category = "Voxel | LOD")
	TArray<float> LODDistances = { 3200.0f, 8000.0f, 16000.0f, 32000.0f };
	UPROPERTY(EditAnywhere, Category = "Voxel | LOD")
	int MaxLOD = 3;

	UPROPERTY(EditAnywhere, Category = "Voxel | Spawn")
	float PlayerSpawnHeight = 110.0f;

	UPROPERTY(EditAnywhere, Category = "Voxel | Performance")
	int32 MaxSpawnPerFrame = 80;
	UPROPERTY(EditAnywhere, Category = "Voxel | Performance")
	int MaxRebuildPerFrame = 60;
	float LastUpdateTime = 0.0f;
	bool bForceUpdate = true;
	FIntVector LastPlayerChunk = FIntVector::ZeroValue;
	TMap<APawn*, FIntVector> LastPlayerChunks;
	bool bNeedUpdate;
	TQueue<AVoxelChunck*, EQueueMode::Mpsc> PendingMeshToApply;
	TQueue<FIntVector> ChunckGenerationQueue;
	TQueue<FChunkGenJob, EQueueMode::Mpsc> ChunckGenerationJobQueue;
	TQueue<FChunkGenResult, EQueueMode::Mpsc> ChunckGenerationResult;
	TQueue<FIntVector, EQueueMode::Mpsc> PendingClusterCoordToApply;
	TQueue<FClusterGenResult, EQueueMode::Mpsc> ClusterGenerationResult;
	TArray<FRunnableThread*> WorkerThreads;
	TArray<ChunckGenWorker*> Workers;
	FCriticalSection DequeueMutex;
	//TArray<TObjectPtr<AClusterChunk>> ClusterPool;

	int32 NumWorkers = 6;

	int32 NumThreads;
	int MaxGenPerFrame;

	int32 CurrentMeshJob;
	int32 MaxMeshJob;
	int32 DesiredLOD;

	TMap<FIntVector, UProceduralMeshComponent*> ClusterPoolTier1;
	TMap<FIntVector, UProceduralMeshComponent*> ClusterPoolTier2;
	TMap<FIntVector, UProceduralMeshComponent*> ClusterPoolTier3;

	TMap<FIntVector, TArray<FChunckMeshData>> ClusterMapTier1;
	TMap<FIntVector, TArray<FChunckMeshData>> ClusterMapTier2;
	TMap<FIntVector, TArray<FChunckMeshData>> ClusterMapTier3;


	//Bruit
	FastNoiseLite SurfaceNoise;
	FastNoiseLite CaveNoise;
	UPROPERTY(EditAnywhere)
	float SurfaceFrequency;     // 2D → collines larges et naturelles 0.006
	UPROPERTY(EditAnywhere)
    float SurfaceAmplitude;      // hauteur des montagnes
	UPROPERTY(EditAnywhere)
    int   BaseHeight;               // niveau moyen du sol
	UPROPERTY(EditAnywhere)
    float CaveFrequency;        // 3D → taille des grottes
	UPROPERTY(EditAnywhere)
    float CaveThreshold;      // plus bas = plus de grottes
	UPROPERTY(EditAnywhere)
    int   SeaLevel;         // niveau de la mer (lacs + océan)

	std::atomic<bool> bIsShuttingDown;

};
