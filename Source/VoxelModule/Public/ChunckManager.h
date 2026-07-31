// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelChunck.h"
#include "FChunckDataStructure.h"
#include "FastNoiseLite.h"
#include "Engine/World.h"
#include "HAL/RunnableThread.h"
#include "Engine/TimerHandle.h"
#include "EChunkVariant.h"
#include "FChunckGenJob.h"
#include "FChunkGenResult.h"
#include "FClusterGenData.h"
#include "FMask.h"
#include "Engine/StaticMesh.h"
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
	void SpawnChunk(FIntVector Coord, int32 LOD, int32 GenerationId);
	void UpdateVisibleChunks(const TSet<FIntVector>& ChunksToKeep);
	void GetAllPlayerChunks(TSet<FIntVector>& GlobalChunksToKeep);
	FIntVector GetPlayerChunck(const FVector& PlayerPos) const;
	void GenerateTerrain(FChunckDataStructure& Data, FIntVector Coord);
	void FillChunck(EChunkVariant Variant, FIntVector Coord, int32 LOD, int32 GenerationId);
	int32 GetLODForChunck(const FIntVector& Coord, const FVector& PlayerPos) const;
	FIntVector GetClusterCoord(FIntVector Coord, int LOD);
	//int32 CalculChunkLODBeforeSpawn(FIntVector Coord);
	bool BuildChunkPaddedVolumeNoLock(FIntVector Coord, int32 LOD, int32 SubSize,
                                                  TArray<FVoxelDataStructure>& OutVolume);

	void GenerateAsyncGreedyMesh(FIntVector Coord);
	void GenerateGreedyMesh(FChunckMeshData& OutMesh, const TArray<FVoxelDataStructure>& PaddedVoxels, FIntVector Coord, int32 LOD);
	void CreateQuad(const FMask& Mask, const FIntVector& AxisMask, int32 Width, int32 Height, const FIntVector& V1, const FIntVector& V2, const FIntVector& V3, const FIntVector& V4, int32& VertexCount, FChunckMeshData& MeshData, int32 LOD);
	bool CompareMask(const FMask& M1, const FMask& M2) const;
	bool IsVoxelSolidLocal(int x, int y, int z, const TArray<FVoxelDataStructure>& LocalVoxels, int32 PaddedSize);

	//void GenerateAsyncGreedyMeshForCluster(FIntVector Coord);
	void ApplyMeshToCluster(const TArray<FChunckMeshData>& ChunkMeshData, TMap<FIntVector, UProceduralMeshComponent*>& ClusterPool, FIntVector ClusterCoord, int32 LOD);
	float GetNoise(float WorldX, float WorldY);
	void InitNoise();
	void MarkVisibilityClean();
	bool ShouldRebuildVisibility() const;
	bool ResolveVoxelWorldIfNeeded();
	void RebuildDesiredChunkSet(TMap <FIntVector, int32>& OutChunksToKeep);
	void BuildStreamingQueues(const TMap <FIntVector, int32>& DesiredChunks);
	void ProcessSpawnQueue();
	void ProcessGenerationQueue();
	void ProcessGenerationResults();
	void ProcessDirtyChunks();
	void ProcessMeshJobs();
	void ProcessPendingClusters();
	void ProcessUnloadQueue();
	void ProcessTransitionQueue();
	bool UpdatePlayerChunkState();

	bool IsChunkGuaranteedEmpty(const FIntVector& Coord) const;

	void ProcessLODCommits();
	void ProcessLODWatchdog();
	void NotifyDisplayApplied(FIntVector Coord);
	void RequestClusterRebuild(FIntVector ClusterCoord, int32 Tier);

	UFUNCTION(Exec)
	void NukeClusters();

	// === Cluster volume meshing ===
	int32 GetNbChunkForLOD(int32 LOD) const;
	bool SampleGlobalVoxelSolidNoLock(int32 GX, int32 GY, int32 GZ);
	bool BuildClusterPaddedVolume(FIntVector ClusterCoord, int32 LOD,
		TArray<FVoxelDataStructure>& OutVolume, TArray<uint8>& OutMask, int32& OutSX, int32& OutSY, int32& OutSZ);
	void GenerateGreedyMeshVolume(FChunckMeshData& OutMesh,
		const TArray<FVoxelDataStructure>& Pad, const TArray<uint8>& MaskVol, int32 SX, int32 SY, int32 SZ, float EffectiveVoxelSize, const FVector& OriginOffset = FVector::ZeroVector);
	bool TryDispatchClusterMesh(FIntVector ClusterCoord, int32 LOD);
	void ApplyClusterVolumeMesh(FIntVector ClusterCoord, int32 LOD, const FChunckMeshData& Mesh);

	TQueue<FIntVector> PendingUnloadQueue;

	TQueue<FIntVector> PendingSpawnQueue;

	TSet<FIntVector> PendingSpawnSet;

	TQueue<FIntVector> PendingTransitionQueue;

	TSet<FIntVector> PendingTransitionSet;


	TSet<FIntVector> PendingUnloadSet;

	TSet<FIntVector> PendingClusterTier1;
	TSet<FIntVector> PendingClusterTier2;
	TSet<FIntVector> PendingClusterTier3;
	//TSet<FIntVector> PendingClusterTier4;
	//TSet<FIntVector> PendingClusterTier5;

	TMap<FIntVector, int32> ClusterMeshVersionTier1;
	TMap<FIntVector, int32> ClusterMeshVersionTier2;
	TMap<FIntVector, int32> ClusterMeshVersionTier3;


	bool bVisibilityInitialized;

	bool bNeedsInitialBuild = true;

	bool bPlayerChangedChunk;

	bool bStreamingSettingsDirty;

	bool bForceVisibilityRefresh;

	UPROPERTY(EditAnywhere, Category = "Voxel | Performance")
	int32 MaxClusterDispatchPerFrame = 64;

	TSet<FIntVector> DirtyChuncks;
	TSet<FIntVector> PendingLODMesh;
	UPROPERTY(EditAnywhere, Category = "Voxel")
	TSubclassOf<AVoxelChunck> VoxelChunckClass;

	UPROPERTY()
	AVoxelWorld* VoxelWorld;
	UPROPERTY(EditAnywhere)
	int VoxelSize = 10;

	UPROPERTY(EditAnywhere)
	int ChunkSize = 128;//32;

	//void UpdateVisibleChunks(const FVector& PlayerLocation);
	int32 HorizontalViewDistance = 40;
	int32 VerticalViewDistance = 10;

	FTimerHandle SafeSpawnTimer;
	void TrySafeSpawn();

	UPROPERTY(EditAnywhere, Category = "Voxel | LOD")
	TArray<float> LODDistances = { 4000.0f, 4500.0f, 7000.0f, 144000.0f/*, 50000.0f, 150000.0f*/ };
	UPROPERTY(EditAnywhere, Category = "Voxel | LOD")
	int MaxLOD = 3;

	UPROPERTY(EditAnywhere, Category = "Voxel | Spawn")
	float PlayerSpawnHeight = 110.0f;

	UPROPERTY(EditAnywhere, Category = "Voxel | Performance")
	int32 MaxSpawnPerFrame = 2000;
	UPROPERTY(EditAnywhere, Category = "Voxel | Performance")
	int MaxRebuildPerFrame = 200;
	float LastUpdateTime = 0.0f;
	bool bForceUpdate = true;
	FIntVector LastPlayerChunk = FIntVector::ZeroValue;
	TMap<APawn*, FIntVector> LastPlayerChunks;
	bool bNeedUpdate;
	TQueue<FIntVector, EQueueMode::Mpsc> PendingMeshToApply;
	TQueue<FIntVector> ChunckGenerationQueue;
	TQueue<FChunkGenJob, EQueueMode::Mpsc> ChunckGenerationJobQueue;
	TQueue<FChunkGenResult, EQueueMode::Mpsc> ChunckGenerationResult;
	TQueue<FIntVector, EQueueMode::Mpsc> PendingClusterCoordToApply;
	TQueue<FClusterGenResult, EQueueMode::Mpsc> ClusterGenerationResult;
	TArray<FRunnableThread*> WorkerThreads;
	TArray<ChunckGenWorker*> Workers;
	FCriticalSection DequeueMutex;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
	TObjectPtr<UMaterialInterface> ClusterMaterial;
	//TArray<TObjectPtr<AClusterChunk>> ClusterPool;

	int32 NumWorkers = 14;

	int32 NumThreads;
	int MaxGenPerFrame;

	int32 CurrentMeshJob = 0;
	int32 CurrentClusterMeshJob = 0;
	int32 MaxClusterMeshJob = 3;
	int32 MaxMeshJob;
	int32 DesiredLOD;

	TMap<FIntVector, UStaticMeshComponent*> ClusterPoolTier1;
	TMap<FIntVector, UStaticMeshComponent*> ClusterPoolTier2;
	TMap<FIntVector, UStaticMeshComponent*> ClusterPoolTier3;

	TMap<FIntVector, TArray<FChunckMeshData>> ClusterMapTier1;
	TMap<FIntVector, TArray<FChunckMeshData>> ClusterMapTier2;
	TMap<FIntVector, TArray<FChunckMeshData>> ClusterMapTier3;


	//Bruit
	FastNoiseLite SurfaceNoise;
	FastNoiseLite CaveNoise;
	UPROPERTY(EditAnywhere)
	float SurfaceFrequency;     // 2D → collines larges et naturelles 0.006
	UPROPERTY(EditAnywhere, Category = "Voxel | Terrain")
	float SurfaceWavelength = 2500.0f;   // en voxels
	UPROPERTY(EditAnywhere, Category = "Voxel | Terrain")
	float SurfaceAmplitude = 600.0f;     // en voxels (±60 m)
	UPROPERTY(EditAnywhere)
	int   BaseHeight;               // niveau moyen du sol
	UPROPERTY(EditAnywhere)
	float CaveFrequency;        // 3D → taille des grottes
	UPROPERTY(EditAnywhere)
	float CaveThreshold;      // plus bas = plus de grottes
	UPROPERTY(EditAnywhere)
	int   SeaLevel;         // niveau de la mer (lacs + océan)

int32 NextGenerationId = 1;
TSet<FIntVector> PendingCommitSet;
TSet<FIntVector> ChunksAwaitingMesh;

UPROPERTY(EditAnywhere, Category = "Voxel | LOD")
float LODSwapWatchdogSeconds = 5.0f;

UPROPERTY(EditAnywhere, Category = "Voxel | LOD")
int32 MaxConcurrentLODTransitions = 512;


	std::atomic<bool> bIsShuttingDown;


	int PendingMeshClusterCount;
	int PendingClusterGenCount;

};
