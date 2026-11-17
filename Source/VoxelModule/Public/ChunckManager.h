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

struct FPendingMeshRequest { FIntVector Coord; uint64 BrickMask = 0; };

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
	void GenerateTerrain(FChunckDataStructure& Data, FIntVector Coord);
	void FillChunck(EChunkVariant Variant, FIntVector Coord, int32 LOD, int32 GenerationId);
	int32 GetLODForChunck(const FIntVector& Coord, const FVector& PlayerPos) const;
	FIntVector GetClusterCoord(FIntVector Coord, int LOD);
	//int32 CalculChunkLODBeforeSpawn(FIntVector Coord);
	bool BuildChunkPaddedVolumeNoLock(FIntVector Coord, int32 LOD, int32 SubSize, TArray<FVoxelDataStructure>& OutVolume);

	void GenerateAsyncGreedyMesh(FIntVector Coord/*, uint64 RequestedMask*/);
	void GenerateGreedyMesh(FChunckMeshData& OutMesh, const TArray<FVoxelDataStructure>& PaddedVoxels, FIntVector Coord, int32 LOD);
	bool CompareMask(const FMask& M1, const FMask& M2) const;

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
		const TArray<FVoxelDataStructure>& Pad, const TArray<uint8>& MaskVol, int32 SX, int32 SY, int32 SZ, float EffectiveVoxelSize, const FVector3f& OriginOffset = FVector3f::ZeroVector);
	bool TryDispatchClusterMesh(FIntVector ClusterCoord, int32 LOD);
	void ApplyClusterVolumeMesh(FIntVector ClusterCoord, int32 LOD, FChunckMeshData&& MeshData);







/***************** à ajouter ******************************************************/
Dans le terrainGenerator : 

Nouvelle struct FVoxelEditJournal.h
#pragma once                                                    // le header n'est inclus qu'une fois par unite de compilation

#include "CoreMinimal.h"                                        // types de base d'Unreal
#include "Net/Serialization/FastArraySerializer.h"              // FFastArraySerializer (module NetCore)
#include "Structs/FChunkEditLayer.h"                            // FVoxelBrushOp : adapte si ta struct est declaree ailleurs
#include "FVoxelEditJournal.generated.h"  
class AChunckManager;

USTRUCT()
struct FVoxelEditEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()
	UPROPERTY()
	FVoxelBrushOp Op;
};

USTRUCT()
struct FVoxelEditJournal : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FVoxelEditEntry> Item;
	UPROPERTY(NotReplicated)
	TObjectPtr<AChunckManager> LocalChunkManager = nullptr;

	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerialiser::FastArrayDeltaSerialize<FVoxelEditEntry, FVoxelEditJournal>(Itemps, DeltaParams, *this);
	}
};

template<>
struct TStructOpTypeTraits<FVoxelEditJournal> : public TStructOpsTypeTraits<FVoxelEditJournal>
{
		enum { WithNetDeltaSerializer = true };                     // active NetDeltaSerialize
};




Dans le ChunckManager.h

#include "Structs/FVoxelEditJournal.h"

	// Declare au moteur quelles proprietes de cet acteur sont repliquees.
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const orverride;
	void CarveSphereAt(const FVector& WorldCenter, float RadiusMeters); //SERVEUR : Point d'entree en metres, puis délégue
	void CarveSphereVoxel(const FIntVector& CenterWorld, int32 RadiusVoxel); //SERVEUR : Décide, crée l'opération, la numérote, l'applique et l'écrit dans le journal
	void ApplyBrushOpToWorld(const FVoxelBrushOp& Op); //TOUTES LES MACHINES : Applique l'opération décidée dans CarveSphereVoxel (couche d'édition, voxels chargées, et rebuild données)
	void ReveiveReplecatedBrushOp(const FVoxelBrushOp& Op); //CLIENT : Appelee par le journal a la reception : range l'operation, ne l'applique pas encore.
	UPROPERTY(Replicated)
	FVoxelEditJournal EditJournal;
	UPROPERTY(EditAnywhere, Category = "Voxel | Network", meta = (ClampMin = "1"))
	int32 MaxReplicatedOpPerFrame = 16;

en private :
	void FlushReplicatedOpBrush();	//CIENT : Applique dans l'ordre des Seq ce qui est prêt et est appélé chaque Tick
	TMap<int32, FVoxelBrushOp> PendingReplicatedOps; //CLIENT : Opérations reçues mais pas encore appliquées
	int32 NextSeqToApply = 0; //Client Prochaine Seq à appliquer. DOIT valoir la vaneul initiale de NextBrushSeq sur le serveur donc si dans le header de FVoxelBrushOp NextBrushSeq = 0 ici NextSeqToApply = 0;

dans le chunckmanager.cpp
#include "Net/UnrealNetwork.h"

Constructeur :

	bReplicates = true; // sans ca, aucune propriete de cet acteur ne part sur le reseau
    bAlwaysRelevant = true; // pertinent pour TOUS les clients, a toute distance (sinon coupe a 150 m)
    EditJournal.LocalChunkManager = this; // le journal saura quel manager prevenir a la reception

Dans Tick, juste après ResolveVoxelWorldIfNeeded(); :
	FlushReplicatedOpBrush();
Les fonctions, qui remplacent l’ancien CarveVoxelsAt et l’ancien CarveSphereAt :
void AChunckManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AChunckManager, EditJournal);  //Le journal est envoye a tous les clients
}
void AChunckManager::CarveSphereAt(const FVector& WorldCenter, float RadiusMeters)
{
	const FIntVector CenterVoxel = WorldToVoxelGlobal(WorldCenter);
	const int32 RadiusVoxels = FMath::Max(1, FMath::CeilToInt(RadiusMeters * 100.f / VoxelSize));
	CarveSpereVoxels(CenterVoxel, RadiusVoxels);
}

void AChunckManager::CarveSphereVoxels(const FIntVector& CenterVoxel, int32 RadiusVoxels)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_CarveSphereVoxels);
	if(!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("CarveSphereVoxels appele sans autorite : ignore (passe par un RPC serveur)"));
        return;
	}
	if (!IsValid(VoxelWorld) || RadiusVoxels <= 0)
    {
        return;
    }
	FVoxelBrusOp Op;
	Op.Type = EVoxelBrushType::Sphere;
	Op.Center = CenterVoxel;
	Op.RadiusVoxels = RadiusVoxels;
	Op.MaterialId = 0;
	Op.Seq = NextSeqBrush++;
}

/**********************************************************************************/










	FIntVector WorldToVoxelGlobal(FVector World);
	FIntVector VoxelGlobalToChunk(FIntVector VoxelLocation);
	int32 VoxelGlobalToLocalIndex0(FIntVector G, FIntVector CC);

	void SetVoxelAt(FIntVector VoxelGlobal, FVoxelDataStructure VoxelNewValue);
	void SetVoxel(int32 gx, int32 gy, int32 gz, int32 MaterialId);
	void DispatchChunkRebuilds(const TMap<FIntVector, int32>& TouchedChunks);

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
	int ChunkSize = 128;//32;

	//void UpdateVisibleChunks(const FVector& PlayerLocation);
	int32 HorizontalViewDistance = 10;
	int32 VerticalViewDistance = 10;

	FTimerHandle SafeSpawnTimer;
	void TrySafeSpawn();

	UPROPERTY(EditAnywhere, Category = "Voxel | LOD")
	TArray<float> LODDistances = { 3000.0f, 6500.0f, 100000.0f, 144000.0f };
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

	UPROPERTY() TMap<FIntVector, URealtimeMeshComponent*> ClusterPoolTier1;   // idem Tier2, Tier3
	UPROPERTY() TMap<FIntVector, URealtimeMeshComponent*> ClusterPoolTier2;
	UPROPERTY() TMap<FIntVector, URealtimeMeshComponent*> ClusterPoolTier3;
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

int32 NextGenerationId = 1;
TSet<FIntVector> PendingCommitSet;
TSet<FIntVector> ChunksAwaitingMesh;

UPROPERTY(EditAnywhere, Category = "Voxel | LOD")
float LODSwapWatchdogSeconds = 30.0f;

UPROPERTY(EditAnywhere, Category = "Voxel | LOD")
int32 MaxConcurrentLODTransitions = 512;


	std::atomic<bool> bIsShuttingDown;


	int PendingMeshClusterCount;
	int PendingClusterGenCount;

TQueue<FIntVector> PendingTransitionQueueCritical;
TQueue<FIntVector> PendingSpawnQueueCritical;
TQueue<FIntVector> ChunckGenerationQueueCritical;
TQueue<FChunkGenJob, EQueueMode::Mpsc> ChunckGenerationJobQueueCritical;
TQueue<FIntVector> PendingMeshToApplyCritical;

TSet<FIntVector> CriticalSet;
FVector SmoothedVelocity;
UPROPERTY(EditAnywhere) float CriticalHorizonSeconds = 0.8f;
UPROPERTY(EditAnywhere) int32 MaxCriticalRadiusChunks = 8;
UPROPERTY(EditAnywhere) int32 MaxCriticalTransitionsInFlight = 32;

};
