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
#include "Structs/FChunkJobTelemetry.h"
#include "FChunkGenResult.h"
#include "FClusterGenData.h"
#include "FMask.h"
#include "Structs/FTerrainGenerator.h"
#include "Engine/StaticMesh.h"
#include "Structs/FVoxelRayHit.h"
#include "Structs/FDesiredSetBuilder.h"
#include "ChunckManager.generated.h"

class ChunckGenWorker;
class AVoxelWorld;
class FDesiredSetBuilder;

class AClusterChunk;

struct FPendingMeshRequest { FIntVector Coord; uint64 BrickMask = 0; };

enum class EClusterBuildResult : uint8
{
	NotReady,   // un chunk de l'empreinte n'est pas encore genere : on reessaiera
	Orphan,     // aucun chunk de ce LOD dans l'empreinte : rien a afficher
	AllSolid,   // aucune cellule visible n'est de l'air : aucune face possible
	AllSolidFast,
	AllAir,
	Ready       // volume construit, a mesher
};

enum class EClusterDispatchResult : uint8
{
	NotReady,     // a garder en attente
	SkippedCheap,   // orphelin : sorti avant toute allocation
	Skipped,        // plein ou air : volume construit, ~120 us de game thread
	Dispatched    // mesh lance
};

struct FPipelineStats
{
	int64 Spawned = 0;             // chunks spawnes
	int64 GenDispatched = 0;       // jobs envoyes aux workers
	int64 GenReturned = 0;         // resultats depiles (acceptes ou non)
	int64 GenAccepted = 0;         // resultats acceptes
	int64 ChunkMeshApplied = 0;    // meshes LOD0 appliques
	int64 ClusterDispatched = 0;   // meshes de cluster lances
	int64 ClusterApplied = 0;      // meshes de cluster appliques
	int64 ClusterStale = 0;        // meshes de cluster jetes car perimes
	int64 ClusterDispatchFail = 0; // clusters pas encore prets
	int64 ClusterEmpty = 0;        // meshes calcules mais vides (ce qui reste a eviter)
	int64 ClusterOrphan = 0;       // clusters sans chunk de leur LOD (sautes)
	int64 ClusterSolid = 0;        // clusters entierement pleins (sautes)
	int64 ClusterAir = 0;
	int64 ClusterSolidFast = 0;   // pleins prouves par les drapeaux
	int64 FastSolidMiss = 0;       // mode ombre : drapeaux disaient plein, la construction dit non
	int64 ClusterBudgetHit = 0;
	int64  ClusterVisited = 0;        // entrees du tableau trie reellement parcourues
	int64  ClusterSorts = 0;          // nombre de tris
	double ClusterSortSeconds = 0.0;  // temps cumule passe a trier

};

// Charge utile d'un job de mesh par briques.
struct FBrickJobPayload
{
	FIntVector             Coord;
	int32                  MeshVersion = 0;
	uint64                 DirtyMask = 0;
	TArray<int32>          DirtyIndices;
	TArray<FVoxelDataStructure> DirtyPadded;   // plat : DirtyIndices.Num() * PB^3
	TArray<FBrickQuadsRef> Bricks;             // NumBricks, nulles pour les dirty
};

struct FClusterTierOrder
{
	TArray<FIntVector> Sorted;   // ordre de traitement ; peut contenir des entrees perimees
	bool bDirty = true;          // un cluster NOUVEAU est entre dans le set -> retrier
};

struct FClusterSlot
{
	FVoxelBuffer Voxels;
	int32 LOD = 0;
	bool bIsPresent = false;
};

struct FClusterJob
{
	FIntVector ClusterCoord = FIntVector::ZeroValue;
	FIntVector ChunkOrigin = FIntVector::ZeroValue;
	int32 LOD = 1;
	int32 NbChunks = 1;
	int32 ChunkSize = 32;
	int32 Version = 0;
	float EVS = 0.f;
	bool  bFastSaysSolid = false;
	// (NbChunk+2)^2 * 3 cases. Index : (cx+1) + (cy+1)*NW + (dz+1)*NW*NW, NW = NbChunk+2.
	TArray<FClusterSlot> Slots;
};

struct FClusterJobResult
{
	FIntVector ClusterCoord = FIntVector::ZeroValue;
	int32 LOD = 1;
	int32 Version = 0;
	bool  bFastSaysSolid = false;
	EClusterBuildResult Build = EClusterBuildResult::NotReady;
	FChunckMeshData Mesh;
};


using FClusterResultQueue = TQueue<FClusterJobResult, EQueueMode::Mpsc>;
struct FChunkPadSnapshot
{
	FVoxelBuffer Center;
	FVoxelBuffer Neighbors[6];                     // meme ordre que AChunckManager::Dirs
	int32 NeighborLOD[6] = { 0, 0, 0, 0, 0, 0 };
};

using FDesiredResultQueue = TQueue<FDesiredResult, EQueueMode::Mpsc>;

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
	void GetAllPlayerChunks(TSet<FIntVector>& GlobalChunksToKeep);
	FIntVector GetPlayerChunck(const FVector& PlayerPos) const;
	//void GenerateTerrain(FChunckDataStructure& Data, FIntVector Coord);
	void FillChunck(EChunkVariant Variant, FIntVector Coord, int32 LOD, int32 GenerationId);
	int32 GetLODForChunck(const FIntVector& Coord, const FVector& PlayerPos) const;
	FIntVector GetClusterCoord(FIntVector Coord, int LOD);
	//int32 CalculChunkLODBeforeSpawn(FIntVector Coord);
	bool BuildChunkPaddedVolumeNoLock(FIntVector Coord, int32 LOD, int32 SubSize, TArray<FVoxelDataStructure>& OutVolume);

	void GenerateAsyncGreedyMesh(FIntVector Coord/*, uint64 RequestedMask*/);
	static bool CompareMask(const FMask& M1, const FMask& M2);

	//void GenerateAsyncGreedyMeshForCluster(FIntVector Coord);
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
	bool IsChunkGuaranteedSolid(const FIntVector& Coord, const FColumnBounds& Bounds) const;

	void ProcessLODCommits();
	void ProcessLODWatchdog();
	void NotifyDisplayApplied(FIntVector Coord);
	void RequestClusterRebuild(FIntVector ClusterCoord, int32 Tier);

	EClusterBuildResult PrepareClusterJobNoLock(FIntVector ClusterCoord, int32 LOD, FClusterJob& OutJob);
	static EClusterBuildResult BuildClusterVolumeFromSnapshot(const FClusterJob& Job,
		TArray<FVoxelDataStructure>& OutVolume, TArray<uint8>& OutMask,
		int32& OutSX, int32& OutSY, int32& OutSZ);
	void ProcessClusterResults();
	TSharedPtr<FClusterResultQueue, ESPMode::ThreadSafe> ClusterResultQueue;

	UPROPERTY(EditAnywhere, Category = "Voxel | Performance")
	float ClusterApplyBudgetMs = 3.0f;   // temps max par frame pour APPLIQUER des resultats de cluster

	// --- Phase 2 ---
	static void BuildChunkPaddedVolumeFromSnapshot(const FChunkPadSnapshot& Snap, int32 LOD, int32 SubSize,
		int32 InChunkSize, TArray<FVoxelDataStructure>& OutVolume);

	// --- Phase 3 ---
	static int32 ComputeLODForChunk(const FIntVector& Coord, const FVector& PlayerPos, int32 InChunkSize,
		int32 InVoxelSize, const TArray<float>& InLODDistances, int32 InMaxLOD);
	static FColumnBounds ComputeColumnBounds(const FTerrainGenerator& Gen, const FIntPoint& Column, int32 InChunkSize);
	bool LaunchDesiredRebuild();
	void ProcessDesiredResults();
	void ApplyDesiredSet(const FDesiredResult& Res);
	TSharedPtr<FDesiredSetBuilder, ESPMode::ThreadSafe>  DesiredBuilder;
	TSharedPtr<FDesiredResultQueue, ESPMode::ThreadSafe> DesiredResultQueue;
	bool   bDesiredInFlight = false;
	bool   bDesiredRequested = false;
	double LastDesiredLogTime = 0.0;

	UFUNCTION(Exec)
	void NukeClusters();

	// === Cluster volume meshing ===
	int32 GetNbChunkForLOD(int32 LOD) const;
	uint8 SampleGlobalVoxelMaterialNoLock(int32 GX, int32 GY, int32 GZ);
	//EClusterBuildResult BuildClusterPaddedVolume(FIntVector ClusterCoord, int32 LOD, TArray<FVoxelDataStructure>& OutVolume, TArray<uint8>& OutMask, int32& OutSX, int32& OutSY, int32& OutSZ);
	static void GenerateGreedyMeshVolume(FChunckMeshData& OutMesh,
		const TArray<FVoxelDataStructure>& Pad, const TArray<uint8>& MaskVol, int32 SX, int32 SY, int32 SZ, float EffectiveVoxelSize, const FVector3f& OriginOffset = FVector3f::ZeroVector);
	EClusterDispatchResult TryDispatchClusterMesh(FIntVector ClusterCoord, int32 LOD);
	void ApplyClusterVolumeMesh(FIntVector ClusterCoord, int32 LOD, FChunckMeshData&& MeshData);
	bool AreClusterChunksAllSolidNoLock(const FIntVector& ChunkOrigin, int32 NbChunk, int32 LOD) const;

	void CarveSphereAt(const FVector& WorldCenter, float RadiusMeters);

	void TelemetryStamp(const FIntVector& Coord, double FChunkJobTelemetry::* Field, double Value);
	void TelemetryReportAndClear(const FIntVector& Coord, int32 LOD);

	FIntVector WorldToVoxelGlobal(FVector World) const;
	FIntVector VoxelGlobalToChunk(FIntVector VoxelLocation) const;
	int32 VoxelGlobalToLocalIndex0(FIntVector G, FIntVector CC) const;

	void SetVoxelAt(const FIntVector& VoxelGlobal, const FVoxelDataStructure& NewValue);
	bool SetVoxelAtNoLock(const FIntVector& VoxelGlobal, const FVoxelDataStructure& NewValue, FIntVector& OutChunkCoord, int32& OutChunkLOD);
	void SetVoxel(int32 gx, int32 gy, int32 gz, int32 MaterialId);
	void DispatchChunkRebuilds(const TMap<FIntVector, int32>& TouchedChunks);
	void TestCoords();
	static bool ApplyBrushOp(TArray<FVoxelDataStructure>& Voxels, FIntVector ChunkCoord, int32 LOD, int32 ChunkSize, const FVoxelBrushOp& Op);
	void PopulateChunkTree(FIntVector Coord);
	float GetProceduralGroundZ(const FVector& Position);

	bool IsSolidWorld(const FIntVector& VoxelGlobal, uint8& OutMaterialId);
	bool VoxelRaycast(const FVector& OriginCm, const FVector& Dir, float RangeCm, FVoxelRayHit& OutHit);

	FColumnBounds GetColumnBounds(const FIntPoint& ColumnPoint);
	TMap<FIntPoint, FColumnBounds> ColumnBoundsCache;
	UFUNCTION(Exec)
	void MeshTrees(float Radius);

	UFUNCTION(Exec)
	void GotoPeak();

	double FillStartTime = 0.0;
	bool   bFillLogged = false;
	UPROPERTY(EditAnywhere, Category = "Voxel | Debug")
	bool bJobTelemetry = false;

	inline static FIntVector Dirs[6] = { {-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1} };
	TQueue<FIntVector> PendingUnloadQueue;

	TQueue<FIntVector> PendingSpawnQueue;

	TSet<FIntVector> PendingSpawnSet;

	TQueue<FIntVector> PendingTransitionQueue;

	TSet<FIntVector> PendingTransitionSet;


	TSet<FIntVector> PendingUnloadSet;

	TSet<FIntVector> PendingClusterTier1;
	TSet<FIntVector> PendingClusterTier2;
	TSet<FIntVector> PendingClusterTier3;

	FClusterTierOrder ClusterOrderTier1;
	FClusterTierOrder ClusterOrderTier2;
	FClusterTierOrder ClusterOrderTier3;
	FIntVector LastClusterSortCenter = FIntVector(MAX_int32);
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
	UPROPERTY(EditAnywhere, Category = "Voxel | Performance")
	int32 MaxClusterSkipPerFrame = 64;
	UPROPERTY(EditAnywhere, Category = "Voxel | Culling")
	bool bVerifyFastSolid = true;
	int64 FastSolidMiss = 0;

	UPROPERTY(EditAnywhere, Category = "Voxel")
	int32 BrickSize = 32; //32 voxels en LOD0


	//TMap<FIntVector, uint64>       DirtyChuncks;
	//TQueue<FPendingMeshRequest>    PendingMeshToApply;

	TSet<FIntVector> DirtyChuncks;
	TSet<FIntVector> PendingLODMesh;
	UPROPERTY(EditAnywhere, Category = "Voxel")
	TSubclassOf<AVoxelChunck> VoxelChunckClass;

	UPROPERTY()
	AVoxelWorld* VoxelWorld;
	UPROPERTY(EditAnywhere)
	int VoxelSize = 10;

	UPROPERTY(EditAnywhere)
	int ChunkSize = 32;

	//void UpdateVisibleChunks(const FVector& PlayerLocation);
	UPROPERTY(EditAnywhere, Category = "Voxel | FieldOfView")
	int32 HorizontalViewDistance = 40;
	UPROPERTY(EditAnywhere, Category = "Voxel | FieldOfView")

	int32 VerticalViewDistance = 20;

	FTimerHandle SafeSpawnTimer;
	void TrySafeSpawn();

	UPROPERTY(EditAnywhere, Category = "Voxel | LOD")
	TArray<float> LODDistances = { 3000.0f, 6500.0f, 7000.0f, 144000.0f };
	UPROPERTY(EditAnywhere, Category = "Voxel | LOD")
	int MaxLOD = 3;

	UPROPERTY(EditAnywhere, Category = "Voxel | Spawn")
	float PlayerSpawnHeight = 110.0f;

	UPROPERTY(EditAnywhere, Category = "Voxel | Performance")
	int32 MaxSpawnPerFrame = 3000;
	UPROPERTY(EditAnywhere, Category = "Voxel | Performance")
	int32 MaxUnloadPerFrame = 3000;
	UPROPERTY(EditAnywhere, Category = "Voxel | Performance")
	int MaxRebuildPerFrame = 3000;
	UPROPERTY(EditAnywhere, Category = "Voxel | Performance")
	int32 MaxGenPerFrameJob = 3000;

	UPROPERTY(EditAnywhere, Category = "Voxel | Culling")
	float CaveLipschitz = 1.0f;
	int64 ShadowPredicted = 0;
	int64 ShadowMiss[4] = {};
	TMap<FIntPoint, FIntVector> ColumnHeightMap;
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
	UPROPERTY(EditAnywhere, Category = "Voxel | Performance")
	int32 MaxMeshJob;
	UPROPERTY(EditAnywhere, Category = "Voxel | Performance")
	int32 MaxClusterInFlight = 128;   // places de mesh cluster, independantes du LOD0

	UPROPERTY(EditAnywhere, Category = "Voxel | Performance")
	int32 MaxChunkMeshInFlight = 32; // places de mesh LOD0
	int32 DesiredLOD;
	TMap<FIntVector, FChunkJobTelemetry> JobTelemetryMap;

	UPROPERTY() TMap<FIntVector, URealtimeMeshComponent*> ClusterPoolTier1;   // idem Tier2, Tier3
	UPROPERTY() TMap<FIntVector, URealtimeMeshComponent*> ClusterPoolTier2;
	UPROPERTY() TMap<FIntVector, URealtimeMeshComponent*> ClusterPoolTier3;
	UPROPERTY() TMap<FIntVector, TObjectPtr<UInstancedStaticMeshComponent>> ClusterPoolTreeTier0;
	TSet<FIntVector> ClusterHasGroupTier1;
	TSet<FIntVector> ClusterHasGroupTier2;
	TSet<FIntVector> ClusterHasGroupTier3;

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
	TSharedPtr<FTerrainConfig, ESPMode::ThreadSafe> TerrainConfig;
	FTerrainGenerator TerrainGenerator;

int32 NextGenerationId = 1;
TSet<FIntVector> PendingCommitSet;
TSet<FIntVector> ChunksAwaitingMesh;

UPROPERTY(EditAnywhere, Category = "Voxel | LOD")
float LODSwapWatchdogSeconds = 30.0f;

UPROPERTY(EditAnywhere, Category = "Voxel | LOD")
int32 MaxConcurrentLODTransitions = 512;

int32 NextBrushSeq = 0;
FCriticalSection TelemetryMutex;
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
TObjectPtr<UMaterialInterface> VoxelMaterial;
UPROPERTY(EditAnywhere) 
float TelemetryAlertThresholdSeconds = 0.25f;

// --- Semis d'arbres (debug, etape 2) --------------------------------
UPROPERTY(EditAnywhere, Category = "Trees")
int32 TreeWorldSeed = 1337;

UPROPERTY(EditAnywhere, Category = "Trees")
float TreeForestWeveLength = 200000.0f;

UPROPERTY(EditAnywhere, Category = "Trees")
float TreeGlobalDensityScale = 1.0f;

UPROPERTY(EditAnywhere, Category = "Trees")
float TreeMinDensity = 0.0f;

UPROPERTY(EditAnywhere, Category = "Trees")
float TreeCellSize = 1000.0f;

UPROPERTY(EditAnywhere, Category = "Trees")
float TreeJitterRatio = 0.5f;
UPROPERTY(EditAnywhere, Category = "Trees")
float TreeScaleMin = 0.8f;
UPROPERTY(EditAnywhere, Category = "Trees")
float TreeScaleMax = 1.3f;


UPROPERTY(EditAnywhere, Category = "Trees")
TObjectPtr<UStaticMesh> TreeMesh;
UPROPERTY(EditAnywhere, Category = "Trees")
TMap<FIntVector, TObjectPtr<UInstancedStaticMeshComponent>> TreeISMPerChunk;

/** Console : TreeDebugScatter 200 */
UFUNCTION(Exec)
void TreeDebugScatter(float RadiusMeters);
UFUNCTION(Exec)
void MeasureCaveSlope(int32 Samples);


std::atomic<bool> bIsShuttingDown;


int PendingMeshClusterCount;
int PendingClusterGenCount;
TMap<FIntVector, int32> ChunkAlreadyBuilt;
TMap<FIntVector, int32> ChunkAlreadyBuiltTier1;
TMap<FIntVector, int32> ChunkAlreadyBuiltTier2;
TMap<FIntVector, int32> ChunkAlreadyBuiltTier3;

TMap<FIntVector, uint64> LastRequestFrameClusterTier1;
TMap<FIntVector, uint64> LastRequestFrameClusterTier2;
TMap<FIntVector, uint64> LastRequestFrameClusterTier3;

TMap<FIntVector, uint64> FirstRequestFrameClusterTier1;
TMap<FIntVector, uint64> FirstRequestFrameClusterTier2;
TMap<FIntVector, uint64> FirstRequestFrameClusterTier3;

UPROPERTY(EditAnywhere, Category = "Voxel | Performance")
int32 ClusterSettleFrames = 5;      // silence requis avant dispatch

UPROPERTY(EditAnywhere, Category = "Voxel | Performance")
int32 ClusterMaxWaitFrames = 30;    // plafond de patience
UPROPERTY(EditAnywhere, Category = "Voxel | Performance")
float GenDispatchBudgetMs = 2.0f;
UPROPERTY(EditAnywhere, Category = "Voxel | Performance")
float ClusterBudgetMs = 8.0f;

FVector LastRebuildPosition = FVector::ZeroVector;

UPROPERTY(EditAnywhere, Category = "Voxel | LOD")
float LODRebuildDistance = 400.0f;

int64 LastGenCount = 0;

UPROPERTY()
int32 AbsoluteStepCeiling = 8000;

FPipelineStats PipeTotal;      // cumule depuis BeginPlay
FPipelineStats PipeLast;       // cliche de la derniere ligne de log
double PipeLastLogTime = 0.0;
int32 PipeFrames = 0;          // frames ecoulees depuis la derniere ligne
int32 LastClusterBudget = 0;

};


