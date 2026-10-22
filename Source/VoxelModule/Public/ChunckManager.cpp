#include "ChunckManager.h"
#include "FChunckDataStructure.h"
#include "GameFramework/GameModeBase.h"  
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformMisc.h"
#include "RealtimeMeshComponent.h"
#include "RealtimeMeshSimple.h"
#include "Interface/Core/RealtimeMeshBuilder.h"
#include "FChunkGenResult.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "ChunckGenWorker.h"
#include "DebugMacro.h"
#include "VoxelWorld.h"
#include "Structs/FDesiredSetBuilder.h"
#include "VoxelMath.h"
#include "Structs/FtreeScatter.h"
#include "DrawDebugHelpers.h"
#include "Components/InstancedStaticMeshComponent.h"


#define PI (3.1415926535897932f)

// Le padding est construit par Memcpy : la structure doit rester copiable octet a octet.
static_assert(TIsTriviallyCopyConstructible<FVoxelDataStructure>::Value,
              "FVoxelDataStructure doit rester trivialement copiable (Memcpy du padding)");

// Sets default values
AChunckManager::AChunckManager() :
    SurfaceWavelength(2500.0f)
    , SurfaceAmplitude(600.0f)
    , BaseHeight(408)
    , CaveFrequency(0.038f)
    , CaveThreshold(0.42f)
    , SeaLevel(24)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_AChunckManager);
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;
    VoxelWorld = nullptr;
    VoxelChunckClass = AVoxelChunck::StaticClass();
    bNeedUpdate = false;
    NumThreads = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
    CurrentMeshJob = 0;
}

// Called when the game starts or when spawned
void AChunckManager::BeginPlay()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_BeginPlay);
    Super::BeginPlay();
    GetWorld()->GetTimerManager().SetTimer(SafeSpawnTimer, this, &AChunckManager::TrySafeSpawn, 0.2f, true);
    {
        TArray<AActor*> Mgrs;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), AChunckManager::StaticClass(), Mgrs);
        AChunckManager* Keeper = nullptr;
        for (AActor* A : Mgrs)
        {
            AChunckManager* M = Cast<AChunckManager>(A);
            if (!M) continue;
            if (!Keeper) { Keeper = M; continue; }
            // On garde le manager PLACÉ (chargé du niveau => porteur de tes réglages),
            // sinon le plus ancien. Départage déterministe dans tous les cas.
            if (M->IsNetStartupActor() != Keeper->IsNetStartupActor())
            {
                if (M->IsNetStartupActor()) Keeper = M;
            }
            else if (M->GetUniqueID() < Keeper->GetUniqueID()) Keeper = M;
        }
        if (Keeper && Keeper != this)
        {
            UE_LOG(LogTemp, Error, TEXT("ChunckManager: doublon %s auto-détruit (on garde %s)."),
                *GetName(), *Keeper->GetName());
            Destroy();
            return;   // surtout PAS de workers ni d'init sur un doublon
        }

    }

    ClusterResultQueue = MakeShared<FClusterResultQueue, ESPMode::ThreadSafe>();
    DesiredResultQueue = MakeShared<FDesiredResultQueue, ESPMode::ThreadSafe>();
    DesiredBuilder = MakeShared<FDesiredSetBuilder, ESPMode::ThreadSafe>();
    bDesiredInFlight = false;
    bDesiredRequested = false;

    NumThreads = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
    MaxClusterMeshJob = FMath::Max(1, MaxClusterInFlight);      // places de mesh cluster
    MaxMeshJob = FMath::Max(1, MaxChunkMeshInFlight);
    const int32 NumGenWorkers = FMath::Clamp(NumWorkers, 1, NumThreads);  // threads de generation
    MaxGenPerFrame = FMath::Max(64, MaxGenPerFrameJob);

    InitNoise();
    TerrainConfig = MakeShared<FTerrainConfig, ESPMode::ThreadSafe>();
    FTerrainConfig& Cfg = *TerrainConfig;
    Cfg.Global.BaseHeight = 408;
    Cfg.Global.MasterSeed = 1337;
    Cfg.Global.CaveFrequency = 0.038f;
    Cfg.Global.CaveThreshold = 0.42f;
    Cfg.Global.WarpStrength = 0.0f;
    Cfg.Global.BaseHeight = BaseHeight;
    Cfg.Global.MasterSeed = 1337;
    Cfg.Global.CaveFrequency = 0.038f;
    Cfg.Global.CaveThreshold = 0.42f;    

    FTerrainLayer Base;
    Base.Type = ETerrainLayerType::Fractal;
    Base.Wavelength = 2500.0f;      // taille des motifs de la couche
    Base.MaskWavelength = 0.0f;     // pas de masque : couche présente partout
    Base.Amplitude = 600.0f;
    Base.Octaves = 5;
    Base.SeedOffset = 100;
    Base.Weight = 1.0f;
    /*FTerrainFieldGate LowErosion;
    LowErosion.Field = FName(TEXT("Erosion"));
    LowErosion.Low = -0.15f;
    LowErosion.High = 0.15f;
    LowErosion.bInvert = true;
    Base.Gates.Add(LowErosion)*/
    Cfg.Layers.Add(Base);

    FTerrainLayer Ridged;
    Ridged.Type = ETerrainLayerType::Ridged;
    Ridged.Wavelength = 20000.0f;      // sommets tous les ~2 km DANS un massif
    /*FTerrainFieldGate OnChain;
    OnChain.Field = FName(TEXT("Mountain chains"));
    OnChain.Low = 0.55f;
    OnChain.High = 0.80f;
    Ridged.Gates.Add(OnChain);*/
    Ridged.Amplitude = 4000.0f;        // 400 m de dénivelé
    Ridged.Gain = 0.6f;
    Ridged.Octaves = 5;
    Ridged.SeedOffset = 200;
    Ridged.Weight = 1.0f;
    Ridged.RidgeStrength = 2.0f;
    //FTerrainFieldGate Test;
    //Test.Field = FName(TEXT("Innexistant"));
    //Ridged.Gates.Add(Test);
    //Cfg.Layers.Add(Ridged);

    

    FTerrainLayer Plaine;
    Plaine.Type = ETerrainLayerType::Fractal;
    Plaine.Amplitude = 5;
    Plaine.Gain = 0.6f;
    Plaine.Octaves = 2;
    Plaine.SeedOffset = 205;
    Plaine.Weight = 0.01f;
    //Plaine.MaskLow = 0.15;
    //Plaine.MaskHigh = 0.40;
    //Plaine.MaskWavelength = 1;
    Plaine.Wavelength = 25000;
    Cfg.Layers.Add(Plaine);


    /*FControlFieldConfig Erosion;
    Erosion.Name = FName(TEXT("Erosion"));
    Erosion.Wavelength = 200000.0f;   // 20 km : regions d'environ 10 km
    Erosion.Octaves = 3;
    Erosion.SeedOffset = 1200;        // doit differer des autres graines
    Erosion.bRemapToUnit = false;     // sert de porte, pas de multiplicateur
    Cfg.ControlFields.Add(Erosion);*/

    /*FControlFieldConfig MountainChains;
    MountainChains.Name = FName(TEXT("Mountain chains"));
    MountainChains.Type = ETerrainLayerType::Ridged;
    MountainChains.Wavelength = 500000.0f;    // 50 km
    MountainChains.Octaves = 2;               // peu d'octaves : des lignes nettes, pas de dentelle
    MountainChains.SeedOffset = 1300;
    MountainChains.bRemapToUnit = false;
    Cfg.ControlFields.Add(MountainChains);*/


    TerrainGenerator.SetConfig(TerrainConfig);
    for (int32 i = 0; i < 6; ++i)
    {
        const float VX = 137.0f + i * 30000.0f;   // ~3 km entre points
        const float VY = 293.0f + i * 17000.0f;
        const float H = TerrainGenerator.ComputeHeight(VX, VY);
        //UE_LOG(LogTemp, Warning, TEXT("    (%8.0f,%8.0f)  H=%8.0f vx  (%.0f m)"), VX, VY, H, H * VoxelSize * 0.01f);
    }

    //UE_LOG(LogTemp, Warning, TEXT("=== TERRAIN : Wavelength=%.1f  Amplitude=%.1f  BaseHeight=%d ==="),  SurfaceWavelength, SurfaceAmplitude, BaseHeight);
    ChunckGenWorker::ResetStats();
    for (int32 i = 0; i < MaxMeshJob; i++)
    {
        ChunckGenWorker* Worker = new ChunckGenWorker(this, ChunckGenerationJobQueue);
        FRunnableThread* Thread = FRunnableThread::Create(Worker, *FString::Printf(TEXT("ChunckWorker_%d"), i));
        Workers.Add(Worker);
        WorkerThreads.Add(Thread);
    }

    for (int32 i = 0; i < 5; ++i)
    {
        const float VX = i * SurfaceWavelength * 0.5f;
        const float N = SurfaceNoise.GetNoise(VX / SurfaceWavelength, 0.0f);
        //UE_LOG(LogTemp, Warning, TEXT("    x=%6.0f vx  bruit=%+.3f  hauteur=%5d vx  (%.1f m)"), VX, N,       BaseHeight + FMath::FloorToInt(N * SurfaceAmplitude), (BaseHeight + N * SurfaceAmplitude) * VoxelSize * 0.01f);
    }
    UE_LOG(LogTemp, Display, TEXT("BUDGETS | clusters=%d | mesh LOD0=%d | workers=%d | gen/frame=%d"),
        MaxClusterMeshJob, MaxMeshJob, NumGenWorkers, MaxGenPerFrame);
}

void AChunckManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_EndPlay);

    bIsShuttingDown = true;
    for (ChunckGenWorker* Worker : Workers)
    {
        if (Worker)
        {
            Worker->Stop();
        }
    }
    for (FRunnableThread* Thread : WorkerThreads)
    {
        if (Thread)
        {
            Thread->WaitForCompletion();
            delete Thread;
        }
    }

    for (ChunckGenWorker* Worker : Workers)
    {
        if (Worker)
        {
            delete Worker;
        }
    }
    Workers.Empty();
    WorkerThreads.Empty();
    Super::EndPlay(EndPlayReason);

}

// Called every frame
void AChunckManager::Tick(float DeltaTime)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_Tick);

    Super::Tick(DeltaTime);
    ResolveVoxelWorldIfNeeded();
    UpdatePlayerChunkState();
    ProcessDesiredResults();
    bDesiredRequested |= ShouldRebuildVisibility();
    if (bDesiredRequested && !bDesiredInFlight)
    {
        if (LaunchDesiredRebuild())
        {
            MarkVisibilityClean();
            bDesiredRequested = false;
        }
    }
    ProcessTransitionQueue();
    /*
    TMap<FIntVector, int32> OutChunksToKeep;
    if (ShouldRebuildVisibility())
    {
        RebuildDesiredChunkSet(OutChunksToKeep);
        BuildStreamingQueues(OutChunksToKeep);
        MarkVisibilityClean();
    }
    */
    ProcessTransitionQueue();      //marque les transitions, ne détruit RIEN
    ProcessSpawnQueue();
    ProcessGenerationQueue();
    ProcessGenerationResults();
    ProcessLODCommits();           
    ProcessDirtyChunks();
    ProcessMeshJobs();
    ProcessClusterResults();
    ProcessPendingClusters();
    ProcessLODWatchdog();          //filet anti-superposition permanente
    ProcessUnloadQueue();
    if (FillStartTime == 0.0) FillStartTime = FPlatformTime::Seconds();

    if (!bFillLogged && PipeTotal.Spawned > 0)
    {
        const int32 Waiting = PendingClusterTier1.Num() + PendingClusterTier2.Num() + PendingClusterTier3.Num();
        const bool bIdle =
            PendingSpawnSet.Num() == 0
            && ChunckGenerationQueue.IsEmpty()
            && PipeTotal.GenDispatched == PipeTotal.GenReturned   // plus rien chez les workers
            && DirtyChuncks.Num() == 0 && PendingMeshToApply.IsEmpty() && CurrentMeshJob == 0
            && Waiting == 0 && CurrentClusterMeshJob == 0;

        if (bIdle && !bFillLogged)
        {
            bFillLogged = true;
            UE_LOG(LogTemp, Display, TEXT("FILL | monde initial affiche en %.2f s"),
                FPlatformTime::Seconds() - FillStartTime);
        }
    }
    ++PipeFrames;
    const double PipeNow = FPlatformTime::Seconds();
    if (PipeLastLogTime == 0.0)
    {
        PipeLastLogTime = PipeNow;   // premiere frame : on initialise seulement
        PipeLast = PipeTotal;
        PipeFrames = 0;
    }
    else if (PipeNow - PipeLastLogTime >= 1.0)
    {
        const double Dt = PipeNow - PipeLastLogTime;
        auto Rate = [Dt](int64 Cur, int64 Prev) { return (double)(Cur - Prev) / Dt; };
        const FPipelineStats& C = PipeTotal;
        const FPipelineStats& P = PipeLast;

        // Ce qui ATTEND entre deux etapes (cumuls, pas debits).
        const int64 GenInFlight = C.GenDispatched - C.GenReturned;          // chez les workers ou en file de resultats
        const int32 ClustersWaiting = PendingClusterTier1.Num() + PendingClusterTier2.Num() + PendingClusterTier3.Num();

        UE_LOG(LogTemp, Display,
            TEXT("PIPELINE | %.0f FPS | /s : spawn=%.0f gen envoyes=%.0f revenus=%.0f acceptes=%.0f | mesh LOD0=%.0f | clusters lances=%.0f appliques=%.0f perimes=%.0f | pas prets=%.0f | sautes : orphelins=%.0f pleins=%.0f | vides restants=%.0f, Air=%.0f, rapides=%.0f fastmiss=%.0f, BudgetCluster=%.0f, ClusterVisited=%.0f, ClusterSorts=%.0f, ClusterSortSeconds=%.2f, en vol=%d/%d"),
            PipeFrames / Dt,
            Rate(C.Spawned, P.Spawned),
            Rate(C.GenDispatched, P.GenDispatched),
            Rate(C.GenReturned, P.GenReturned),
            Rate(C.GenAccepted, P.GenAccepted),
            Rate(C.ChunkMeshApplied, P.ChunkMeshApplied),
            Rate(C.ClusterDispatched, P.ClusterDispatched),
            Rate(C.ClusterApplied, P.ClusterApplied),
            Rate(C.ClusterStale, P.ClusterStale),
            Rate(C.ClusterDispatchFail, P.ClusterDispatchFail),
            Rate(C.ClusterOrphan, P.ClusterOrphan),
            Rate(C.ClusterSolid, P.ClusterSolid),
            Rate(C.ClusterEmpty, P.ClusterEmpty),
            Rate(C.ClusterAir, P.ClusterAir),
            Rate(C.ClusterSolidFast, P.ClusterSolidFast),
            Rate(C.FastSolidMiss, P.FastSolidMiss),
            Rate(C.ClusterBudgetHit, P.ClusterBudgetHit),
            Rate(C.ClusterVisited, P.ClusterVisited),
            Rate(C.ClusterSorts, P.ClusterSorts),
            (C.ClusterSortSeconds - P.ClusterSortSeconds) * 1000.0 / FMath::Max(1, PipeFrames),
            CurrentClusterMeshJob, MaxClusterMeshJob);

        PipeLast = PipeTotal;
        PipeLastLogTime = PipeNow;
        PipeFrames = 0;
    }
}

void AChunckManager::MarkVisibilityClean()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_MarkVisibilityClean);

    bNeedsInitialBuild = false;

    bPlayerChangedChunk = false;

    bStreamingSettingsDirty = false;

    bForceVisibilityRefresh = false;

    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        if (APawn* Pawn = PC->GetPawn())
            LastRebuildPosition = Pawn->GetActorLocation();
}

int32 AChunckManager::GetLODForChunck(const FIntVector& Coord, const FVector& PlayerPos) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_GetLODForChunck);

    return ComputeLODForChunk(Coord, PlayerPos, ChunkSize, VoxelSize, LODDistances, MaxLOD);
}



void AChunckManager::GetAllPlayerChunks(TSet<FIntVector>& GlobalChunksToKeep)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_GetAllPlayerChunks);

    TArray<AActor*> Players;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APawn::StaticClass(), Players);
    float ChunkWorldSize = VoxelSize * ChunkSize;
    for (AActor* Player : Players)
    {
        APawn* Pawn = Cast<APawn>(Player);
        if (!Pawn) continue;
        FVector PlayerPos = Pawn->GetActorLocation();
        FIntVector PlayerChunk = GetPlayerChunck(PlayerPos);

        if (LastPlayerChunks.FindOrAdd(Pawn) != PlayerChunk)
        {
            //Deadzone pour éviter les micro-sauts
            if (FMath::Abs(LastPlayerChunks[Pawn].X - PlayerChunk.X) > 0 ||
                FMath::Abs(LastPlayerChunks[Pawn].Y - PlayerChunk.Y) > 0 ||
                FMath::Abs(LastPlayerChunks[Pawn].Z - PlayerChunk.Z) > 0)
            {
                LastPlayerChunks[Pawn] = PlayerChunk;
                bNeedUpdate = true;
            }
        }

        for (int32 dx = -HorizontalViewDistance; dx <= HorizontalViewDistance; dx++)
            for (int32 dy = -HorizontalViewDistance; dy <= HorizontalViewDistance; dy++)
                for (int32 dz = -VerticalViewDistance; dz <= VerticalViewDistance; dz++)
                {
                    float DistSq = dx * dx + dy * dy;

                    if (DistSq <= HorizontalViewDistance * HorizontalViewDistance)
                    {
                        GlobalChunksToKeep.Add(PlayerChunk + FIntVector(dx, dy, dz));
                    }
                }
    }
}

FIntVector AChunckManager::GetPlayerChunck(const FVector& PlayerPos) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_GetPlayerChunck);

    float ChunkWorldSize = VoxelSize * ChunkSize;

    return FIntVector(
        FMath::FloorToInt(PlayerPos.X / ChunkWorldSize),
        FMath::FloorToInt(PlayerPos.Y / ChunkWorldSize),
        FMath::FloorToInt(PlayerPos.Z / ChunkWorldSize)
    );
}



void AChunckManager::RegisterDirtyChunk(FIntVector Coord)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_RegisterDirtyChunk);

    if (!VoxelWorld)
    {
        UE_LOG(LogTemp, Error, TEXT("  AChunckManager::RegisterDirtyChunk(FIntVector Coord) → Impossible : VoxelWorld NULL"));
        return;
    }
    {
        FScopeLock Lock(&VoxelWorld->ChunckMutex);
        if (VoxelWorld->Chuncks.Find(Coord))
        {
            FChunkGenResult Result;
            Result.bIsAllSolid = false;
            DirtyChuncks.Add(Coord);
            //UE_LOG(LogTemp, Warning, TEXT(" RegisterDirtyChunk(FIntVector Coord)  → Chunk %s enfilé (existe dans la map)"), *Coord.ToString());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT(" RegisterDirtyChunk(FIntVector Coord)  → Chunk %s NON ENFILÉ (pas encore créé dans VoxelWorld)"), *Coord.ToString());
        }
    }

}

/*
void AChunckManager::GenerateTerrain(FChunckDataStructure& Data, FIntVector Coord)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_GenerateTerrain);

    for (int x = 0; x < ChunkSize; x++)
        for (int y = 0; y < ChunkSize; y++)
        {
            float WorldX = (Coord.X * ChunkSize + x) * 0.08f;
            float WorldY = (Coord.Y * ChunkSize + y) * 0.08f;

            float Noise = FMath::PerlinNoise2D(FVector2D(WorldX, WorldY));

            int Surface = 12 + FMath::FloorToInt(Noise * 14.0f);

            for (int z = 0; z < ChunkSize; z++)
            {
                int index = x + y * ChunkSize + z * ChunkSize * ChunkSize;
                int GlobalZ = Coord.Z * ChunkSize + z;

                Data.Voxels[index].Material.Id = (GlobalZ < Surface) ? 1 : 0;
            }
        }
}

*/

float AChunckManager::GetNoise(float WorldX, float WorldY)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_GetNoise);

    float Noise = 0.0f;
    float Amplitude = 1.0f;
    float Frequency = 1.0f;

    for (int Octave = 0; Octave < 5; Octave++)
    {
        Noise += Amplitude * FMath::PerlinNoise2D(FVector2D(WorldX * Frequency, WorldY * Frequency));
        Amplitude *= 0.5f;      // chaque octave est 2x plus petit
        Frequency *= 2.0f;      // chaque octave est 2x plus détaillé
    }
    return Noise;
}

void AChunckManager::InitNoise()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_InitNoise);

    SurfaceNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    SurfaceNoise.SetFrequency(1);
    CaveNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    CaveNoise.SetFrequency(CaveFrequency);
}



void AChunckManager::FillChunck(EChunkVariant Variant, FIntVector Coord, int32 LOD, int32 GenerationId)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_FillChunck);

    FChunkGenJob Job(Coord, Variant, this, LOD, GenerationId);
    //if (!ChunckData) return;
    int32 TotalSize = ChunkSize * ChunkSize * ChunkSize;
    if (IsValid(VoxelWorld))
    {
        FScopeLock Lock(&VoxelWorld->ChunckMutex);

        if (const FChunkEditLayer* Layer = VoxelWorld->EditLayers.Find(Coord))
        {
            NathanDebug(TEXT("Le Chunk (%d, %d? %d) est édité"), Coord.X, Coord.Y, Coord.Z);
            Job.Edits = Layer->Edits;
            Job.BrushOps = Layer->BrushOps;
        }
    }
    Job.SharedFTerrainConfig = TerrainConfig;
    TelemetryStamp(Coord, &FChunkJobTelemetry::TEnqueue, FPlatformTime::Seconds());
    ChunckGenerationJobQueue.Enqueue(Job);
}

FIntVector AChunckManager::GetClusterCoord(FIntVector Coord, int LOD)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_GetClusterCoord);

    int32 NbChunk = GetNbChunkForLOD(LOD);


    FIntVector ClusterCoord(
        FMath::FloorToInt((float)Coord.X / NbChunk),
        FMath::FloorToInt((float)Coord.Y / NbChunk),
        Coord.Z
    );

    return ClusterCoord;
}

//Crée l'acteur AVoxelChunk, ajoute la coordonnée dans le voxelWorld et enqueue dans ChunckGenerationQueue la coordonnée
void AChunckManager::SpawnChunk(FIntVector Coord, int32 LOD, int32 GenerationId)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_SpawnChunk);

    if (!VoxelWorld || !VoxelChunckClass || !IsValid(VoxelWorld))
        return;

    if (IsChunkGuaranteedEmpty(Coord))
    {
        return;
    }

    { // garde : déjà présent → no-op (spawn uniquement sur le GT)
        FScopeLock Lock(&VoxelWorld->ChunckMutex);
        if (VoxelWorld->Chuncks.Contains(Coord)) return;
    }

    FVector Location = FVector(Coord) * ChunkSize * VoxelSize;
    FChunckDataStructure NewData;
    int32 SubSize = ChunkSize >> LOD;
    TArray<FVoxelDataStructure> InitialVoxelDataStructure;
    InitialVoxelDataStructure.SetNum(SubSize * SubSize * SubSize);
    NewData.Voxels = MoveTemp(InitialVoxelDataStructure);

    if (LOD == 0)
    {
        AVoxelChunck* VoxelChunck = GetWorld()->SpawnActor<AVoxelChunck>(
            VoxelChunckClass, Location, FRotator::ZeroRotator);

        if (!IsValid(VoxelChunck))
        {
            UE_LOG(LogTemp, Error, TEXT("AChunckManager::SpawnChunk --> VoxelChunck null"));
            return;
        }

        NewData.VoxelChunck = VoxelChunck;
        VoxelChunck->SetChunckManager(this);
        VoxelChunck->Coord = Coord;
    }

    {
        FScopeLock Lock(&VoxelWorld->ChunckMutex);
        NewData.GenerationId = GenerationId; // ← paramètre, pas 1
        NewData.LOD = LOD;
        VoxelWorld->Chuncks.Add(Coord, MoveTemp(NewData));
    }
    //UE_LOG(LogTemp, Warning, TEXT("AChunckManager::SpawnChunk --> ChunkLOD pour (%d, %d, %d) : %d"), NewData.Coord.X, NewData.Coord.Y, NewData.Coord.Z, LOD);
    ChunckGenerationQueue.Enqueue(Coord);
}

void AChunckManager::GenerateAsyncGreedyMesh(FIntVector Coord) // POUR LE LOD0
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_GenerateAsyncGreedyMesh);

    bool bDispatched = false;
    AVoxelChunck* VoxelChunk = nullptr;

    // Filet UNIQUE. Tout chemin de sortie qui n'a pas lance le job rend le slot ET
    // debloque l'acteur. Sans ca, un chunk sorti tot reste bIsQueued=true a vie :
    // ProcessDirtyChunks ne le re-enfilera plus jamais -> geometrie perimee permanente.
    ON_SCOPE_EXIT
    {
        if (!bDispatched)
        {
            CurrentMeshJob = FMath::Max(0, CurrentMeshJob - 1);
            if (IsValid(VoxelChunk)) VoxelChunk->bIsQueued = false;
        }
    };

    if (bIsShuttingDown || !IsValid(VoxelWorld)) return;

    int32 CapturedGenerationId = 0;
    FChunkPadSnapshot Snap;

    {
        FScopeLock Lock(&VoxelWorld->ChunckMutex);

        FChunckDataStructure* ChunkData = VoxelWorld->Chuncks.Find(Coord);
        if (!ChunkData) return;
        if (ChunkData->LOD != 0) return;
        if (!ChunkData->bIsChunckGenerated) return;

        VoxelChunk = ChunkData->VoxelChunck.Get();
        if (!IsValid(VoxelChunk) || VoxelChunk->bIsBeingDestroyed)
        {
            VoxelChunk = nullptr;
            return;
        }

        // Ancien echec de BuildChunkPaddedVolumeNoLock : on le detecte ICI, sur le game thread.
        if (ChunkData->Voxels.Num() != ChunkSize * ChunkSize * ChunkSize)
        {
            UE_LOG(LogTemp, Error, TEXT("GenerateAsyncGreedyMesh (%d,%d,%d) : taille de voxels incoherente"),
                Coord.X, Coord.Y, Coord.Z);
            return;
        }

        CapturedGenerationId = ChunkData->GenerationId;

        // Photographie : 7 copies de POINTEURS.
        Snap.Center = ChunkData->Voxels;
        for (int32 i = 0; i < 6; ++i)
        {
            if (const FChunckDataStructure* N = VoxelWorld->Chuncks.Find(Coord + Dirs[i]))
            {
                Snap.Neighbors[i] = N->Voxels;
                Snap.NeighborLOD[i] = N->LOD;
            }
        }
    }

    TWeakObjectPtr<AVoxelChunck>   WeakChunk(VoxelChunk);
    TWeakObjectPtr<AChunckManager> WeakManager(this);
    const int32 CS = ChunkSize;
    const float EVS = VoxelSize;

    bDispatched = true;

    Async(EAsyncExecution::ThreadPool,
        [WeakManager, WeakChunk, Snap = MoveTemp(Snap), CS, EVS, Coord, CapturedGenerationId]() mutable
        {
            // Plus AUCUN acces au manager sur ce thread.
            TArray<FVoxelDataStructure> Padded;
            AChunckManager::BuildChunkPaddedVolumeFromSnapshot(Snap, 0, CS, CS, Padded);
            Snap = FChunkPadSnapshot();   // rend les tableaux partages au plus tot

            FChunckMeshData MeshData;
            MeshData.ChunckCoord = Coord;
            static const TArray<uint8> EmptyMask;
            AChunckManager::GenerateGreedyMeshVolume(MeshData, Padded, EmptyMask, CS, CS, CS, EVS);
            const double TMeshed = FPlatformTime::Seconds();   // mesure ici, appliquee sur le game thread

            AsyncTask(ENamedThreads::GameThread,
                [WeakManager, WeakChunk, MeshData = MoveTemp(MeshData), Coord, CapturedGenerationId, TMeshed]() mutable
                {
                    AChunckManager* Manager = WeakManager.Get();

                    if (Manager) Manager->CurrentMeshJob = FMath::Max(0, Manager->CurrentMeshJob - 1);
                    if (WeakChunk.IsValid()) WeakChunk->bIsQueued = false;

                    if (!Manager || !IsValid(Manager->VoxelWorld)) return;
                    Manager->TelemetryStamp(Coord, &FChunkJobTelemetry::TMeshed, TMeshed);

                    bool bStale = true;
                    {
                        FScopeLock Lock(&Manager->VoxelWorld->ChunckMutex);
                        if (const FChunckDataStructure* Data = Manager->VoxelWorld->Chuncks.Find(Coord))
                            bStale = (Data->GenerationId != CapturedGenerationId) || (Data->LOD != 0);
                    }
                    if (bStale) return;

                    if (WeakChunk.IsValid() && !WeakChunk->bIsBeingDestroyed)
                    {
                        WeakChunk->ApplyMesh(MoveTemp(MeshData));
                        ++Manager->PipeTotal.ChunkMeshApplied;
                        Manager->TelemetryStamp(Coord, &FChunkJobTelemetry::TApplied, FPlatformTime::Seconds());
                        Manager->TelemetryReportAndClear(Coord, 0);
                        Manager->NotifyDisplayApplied(Coord);
                    }
                });
        });
}

int32 AChunckManager::ComputeLODForChunk(const FIntVector& Coord, const FVector& PlayerPos, int32 InChunkSize,
    int32 InVoxelSize, const TArray<float>& InLODDistances, int32 InMaxLOD)
{
    const FVector Center = FVector(Coord) * InChunkSize * InVoxelSize + FVector(InChunkSize * InVoxelSize * 0.5f);
    const float Dist = FVector::Dist(PlayerPos, Center);
    for (int32 i = 0; i < InLODDistances.Num(); ++i)
    {
        if (Dist < InLODDistances[i]) return FMath::Clamp(i, 0, InMaxLOD);
    }
    return InMaxLOD;
}

FColumnBounds AChunckManager::ComputeColumnBounds(const FTerrainGenerator& Gen, const FIntPoint& Column, int32 InChunkSize)
{
    const int32 OX = Column.X * InChunkSize - 1;
    const int32 OY = Column.Y * InChunkSize - 1;
    const int32 N = InChunkSize + 2;

    FColumnBounds Bounds;
    Bounds.MinH = TNumericLimits<float>::Max();
    Bounds.MaxH = TNumericLimits<float>::Lowest();
    for (int32 y = 0; y < N; ++y)
        for (int32 x = 0; x < N; ++x)
        {
            const float H = Gen.ComputeHeight((float)OX + x, (float)OY + y);
            Bounds.MinH = FMath::Min(Bounds.MinH, H);
            Bounds.MaxH = FMath::Max(Bounds.MaxH, H);
        }
    return Bounds;
}

int32 AChunckManager::GetNbChunkForLOD(int32 LOD) const
{
    switch (LOD) { case 1: return 1; case 2: return 4; case 3: return 8; default: return 1; }
}

uint8 AChunckManager::SampleGlobalVoxelMaterialNoLock(int32 GX, int32 GY, int32 GZ)
{
    if (!VoxelWorld) return 0;

    const FIntVector CC(FloorDivInt(GX, ChunkSize),
        FloorDivInt(GY, ChunkSize),
        FloorDivInt(GZ, ChunkSize));

    const FChunckDataStructure* D = VoxelWorld->Chuncks.Find(CC);
    if (!D || D->Voxels.Num() == 0) return 0;

    const int32 SubSize = ChunkSize >> D->LOD;
    const int32 lx = (GX - CC.X * ChunkSize) >> D->LOD;
    const int32 ly = (GY - CC.Y * ChunkSize) >> D->LOD;
    const int32 lz = (GZ - CC.Z * ChunkSize) >> D->LOD;

    const int32 idx = lx + ly * SubSize + lz * SubSize * SubSize;
    if (!D->Voxels.IsValidIndex(idx)) return 0;
    return D->Voxels[idx].Material.Id;
}

bool AChunckManager::BuildChunkPaddedVolumeNoLock(FIntVector Coord, int32 LOD, int32 SubSize,
                                                  TArray<FVoxelDataStructure>& OutVolume)
{
    if (!VoxelWorld) return false;

    const FChunckDataStructure* D = VoxelWorld->Chuncks.Find(Coord);
    if (!D) return false;

    const int32 Expected = SubSize * SubSize * SubSize;
    if (D->Voxels.Num() != Expected) return false;

    const int32 PS = SubSize + 2;
    OutVolume.Reset();
    OutVolume.SetNumZeroed(PS * PS * PS);   // memset, PAS 2M appels de constructeur

    // --- Interieur : X est contigu en memoire -> une seule Memcpy par ligne. ---
    for (int32 z = 0; z < SubSize; ++z)
        for (int32 y = 0; y < SubSize; ++y)
        {
            const int32 Src = y * SubSize + z * SubSize * SubSize;
            const int32 Dst = 1 + (y + 1) * PS + (z + 1) * PS * PS;
            FMemory::Memcpy(&OutVolume[Dst], &D->Voxels[Src],
                            SubSize * sizeof(FVoxelDataStructure));
        }

    // --- Les 6 faces de padding, chacune lue a SON PROPRE LOD. ---
    const int32 Step = 1 << LOD;
    //static const FIntVector Dirs[6] = { {-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1} };

    for (const FIntVector& Dir : Dirs)
    {
        const FChunckDataStructure* N = VoxelWorld->Chuncks.Find(Coord + Dir);
        if (!N || N->Voxels.Num() == 0) continue;               // absent -> traite comme de l'air

        const int32 NLOD = N->LOD;
        const int32 NSub = ChunkSize >> NLOD;
        if (N->Voxels.Num() != NSub * NSub * NSub) continue;    // incoherent -> air, jamais de lecture hasardeuse

        const int32 Axis = (Dir.X != 0) ? 0 : (Dir.Y != 0) ? 1 : 2;
        const int32 A1 = (Axis + 1) % 3;
        const int32 A2 = (Axis + 2) % 3;
        const int32 Sign = (Axis == 0) ? Dir.X : (Axis == 1) ? Dir.Y : Dir.Z;

        // Sur l'axe traverse : cellule -1 -> derniere couche du voisin, cellule SubSize -> premiere.
        // (ChunkSize - Step) est la coordonnee locale-monde ; >> NLOD la snappe sur la grille du voisin.
        const int32 NAxisIdx = (Sign < 0) ? ((ChunkSize - Step) >> NLOD) : 0;
        const int32 PAxisIdx = (Sign < 0) ? 0 : (PS - 1);

        for (int32 b = 0; b < SubSize; ++b)
            for (int32 a = 0; a < SubSize; ++a)
            {
                int32 NI[3], PadI[3];
                NI[Axis] = NAxisIdx;
                NI[A1]   = (a * Step) >> NLOD;   // notre grille -> monde -> grille du voisin
                NI[A2]   = (b * Step) >> NLOD;

                PadI[Axis] = PAxisIdx;
                PadI[A1]   = a + 1;
                PadI[A2]   = b + 1;

                const int32 SrcIdx = NI[0] + NI[1] * NSub + NI[2] * NSub * NSub;
                if (!N->Voxels.IsValidIndex(SrcIdx)) continue;

                OutVolume[PadI[0] + PadI[1] * PS + PadI[2] * PS * PS] = N->Voxels[SrcIdx];
            }
    }

    return true;
}

// Greedy mesh sur volume de dimensions arbitraires (données déjà downsamplées -> Step interne = 1)
void AChunckManager::GenerateGreedyMeshVolume(FChunckMeshData& OutMesh,
    const TArray<FVoxelDataStructure>& Pad, const TArray<uint8>& MaskVol, int32 SX, int32 SY, int32 SZ, float EVS, const FVector3f& OriginOffset)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_GenerateGreedyMeshVolume);

    using namespace RealtimeMesh;
    TRealtimeMeshBuilderLocal<uint32, FPackedNormal, FVector2DHalf, 1> Builder(OutMesh.Streams);
    Builder.EnableTangents();
    Builder.EnableTexCoords();
    Builder.EnableColors();
    int32 QuadCount = 0;
    const int32 PX = SX + 2, PY = SY + 2, PZ = SZ + 2;
    const int32 Dims[3] = { SX, SY, SZ };

    auto Solid = [&](int32 x, int32 y, int32 z) -> bool
        {
            const int32 ix = x + 1, iy = y + 1, iz = z + 1;
            if (ix < 0 || ix >= PX || iy < 0 || iy >= PY || iz < 0 || iz >= PZ) return false;
            const int32 idx = ix + iy * PX + iz * PX * PY;
            return Pad.IsValidIndex(idx) && Pad[idx].Material.Id > 0;
        };

    auto IsMasked = [&](int32 x, int32 y, int32 z) -> bool
        {
            const int32 ix = x + 1, iy = y + 1, iz = z + 1;
            if (ix < 0 || ix >= PX || iy < 0 || iy >= PY || iz < 0 || iz >= PZ) return false;
            const int32 idx = ix + iy * PX + iz * PX * PY;
            return MaskVol.IsValidIndex(idx) && MaskVol[idx] != 0;
        };

    auto MaterialAt = [&](int32 x, int32 y, int32 z) -> uint8
        {
            const int32 ix = x + 1, iy = y + 1, iz = z + 1;
            if (ix < 0 || ix >= PX || iy < 0 || iy >= PY || iz < 0 || iz >= PZ) return 0;
            const int32 idx = ix + iy * PX + iz * PX * PY;
            if (!Pad.IsValidIndex(idx)) return 0;
            return Pad[idx].Material.Id;
        };

    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        const int32 A1 = (Axis + 1) % 3;
        const int32 A2 = (Axis + 2) % 3;
        int32 t[3] = { 0, 0, 0 }; t[A1] = 1;
        const FVector3f T3f((float)t[0], (float)t[1], (float)t[2]);
        int32 q[3] = { 0,0,0 }; q[Axis] = 1;

        TArray<FMask> Mask;
        Mask.SetNum(Dims[A1] * Dims[A2]);

        int32 Iter[3] = { 0,0,0 };
        for (Iter[Axis] = -1; Iter[Axis] < Dims[Axis]; ++Iter[Axis])
        {
            int32 N = 0;
            for (Iter[A2] = 0; Iter[A2] < Dims[A2]; ++Iter[A2])
                for (Iter[A1] = 0; Iter[A1] < Dims[A1]; ++Iter[A1])
                {
                    const bool a = Solid(Iter[0], Iter[1], Iter[2]);
                    const bool b = Solid(Iter[0] + q[0], Iter[1] + q[1], Iter[2] + q[2]);
                    // Un côté masqué => aucune face : cette frontière est fictive.
                    if (IsMasked(Iter[0], Iter[1], Iter[2]) ||
                        IsMasked(Iter[0] + q[0], Iter[1] + q[1], Iter[2] + q[2]))
                        Mask[N++] = FMask{ 0, 0 };
                    else if (a == b) Mask[N++] = FMask{ 0, 0 };
                    else if (a)      Mask[N++] = FMask{ MaterialAt(Iter[0], Iter[1], Iter[2] ), 1};
                    else             Mask[N++] = FMask{ MaterialAt(Iter[0] + q[0], Iter[1] + q[1], Iter[2] + q[2]), -1};
                }

            N = 0;
            for (int32 j = 0; j < Dims[A2]; ++j)
            {
                for (int32 i = 0; i < Dims[A1]; )
                {
                    if (Mask[N].Normal != 0)
                    {
                        const FMask CurrentMask = Mask[N];
                        
                        int32 W = 1;
                        while (i + W < Dims[A1] && CompareMask(Mask[N + W], CurrentMask)) ++W;
                        int32 H = 1; bool Done = false;
                        for (; j + H < Dims[A2]; ++H)
                        {
                            for (int32 k = 0; k < W; ++k)
                                if (!CompareMask(Mask[N + k + H * Dims[A1]], CurrentMask)) { Done = true; break; }
                            if (Done) break;
                        }

                        int32 V1[3]; V1[Axis] = Iter[Axis] + 1; V1[A1] = i;     V1[A2] = j;
                        int32 V2[3] = { V1[0],V1[1],V1[2] }; V2[A1] += W;
                        int32 V3[3] = { V1[0],V1[1],V1[2] }; V3[A2] += H;
                        int32 V4[3] = { V2[0],V2[1],V2[2] }; V4[A2] += H;

                        const FVector Normal = FVector(q[0], q[1], q[2]) * (float)CurrentMask.Normal;
                        const FVector3f N3f(Normal);
                        int32 Idx[4];
                        Idx[0] = Builder.AddVertex(FVector3f(V1[0], V1[1], V1[2]) * EVS + OriginOffset)
                            .SetNormalAndTangent(N3f, T3f).SetColor(FColor(255, 255, 255, CurrentMask.Block)).SetTexCoord(FVector2f(0.f, 0.f));
                        Idx[1] = Builder.AddVertex(FVector3f(V2[0], V2[1], V2[2]) * EVS + OriginOffset)
                            .SetNormalAndTangent(N3f, T3f).SetColor(FColor(255, 255, 255, CurrentMask.Block)).SetTexCoord(FVector2f(W, 0.f));
                        Idx[2] = Builder.AddVertex(FVector3f(V3[0], V3[1], V3[2]) * EVS + OriginOffset)
                            .SetNormalAndTangent(N3f, T3f).SetColor(FColor(255, 255, 255, CurrentMask.Block)).SetTexCoord(FVector2f(0.f, H));
                        Idx[3] = Builder.AddVertex(FVector3f(V4[0], V4[1], V4[2]) * EVS + OriginOffset)
                            .SetNormalAndTangent(N3f, T3f).SetColor(FColor(255, 255, 255, CurrentMask.Block)).SetTexCoord(FVector2f(W, H));

                        int32 Dir = CurrentMask.Normal;   // ±1
                        Builder.AddTriangle(Idx[0], Idx[2 + Dir], Idx[2 - Dir]);
                        Builder.AddTriangle(Idx[3], Idx[1 - Dir], Idx[1 + Dir]);

                        for (int32 l = 0; l < H; ++l)
                            for (int32 k = 0; k < W; ++k)
                                Mask[N + k + l * Dims[A1]] = FMask{ 0, 0 };

                        i += W; N += W;
                        QuadCount++;
                    }
                    else { ++i; ++N; }
                }
            }
            OutMesh.bIsEmpty = (QuadCount == 0);
        }
    }
}
/*
EClusterDispatchResult AChunckManager::TryDispatchClusterMesh(FIntVector ClusterCoord, int32 LOD)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_TryDispatchClusterMesh);

    TArray<FVoxelDataStructure> Volume;
    TArray<uint8> MaskVol;
    int32 SX = 0, SY = 0, SZ = 0;
    const EClusterBuildResult Build = BuildClusterPaddedVolume(ClusterCoord, LOD, Volume, MaskVol, SX, SY, SZ);
    if (Build == EClusterBuildResult::NotReady)
    {
        return EClusterDispatchResult::NotReady;
    }

    // Ce traitement devient LE dernier etat connu de ce cluster, qu'on le meshe ou non.
    // Tout job plus ancien encore en vol sera jete a l'arrivee (garde de version du callback).
    TMap<FIntVector, int32>& VersionMap =
        (LOD == 1) ? ClusterMeshVersionTier1 :
        (LOD == 2) ? ClusterMeshVersionTier2 : ClusterMeshVersionTier3;
    const int32 MyVersion = ++VersionMap.FindOrAdd(ClusterCoord);

    // Rien a afficher : on fait TOUT DE SUITE ce que ferait le callback avec un mesh vide.
    // ApplyClusterVolumeMesh efface l'ancienne geometrie si besoin ET appelle NotifyFootprint.
    if (Build != EClusterBuildResult::Ready)
    {
        if (Build == EClusterBuildResult::AllSolid || Build == EClusterBuildResult::AllSolidFast)
        {
            ++PipeTotal.ClusterSolid;
        }
        else if (Build == EClusterBuildResult::AllAir)
        {
            ++PipeTotal.ClusterAir;
        }

        FChunckMeshData EmptyMesh;
        EmptyMesh.ChunckCoord = ClusterCoord;
        EmptyMesh.bIsEmpty = true;   // OBLIGATOIRE : ce membre n'a pas de valeur par defaut
        ApplyClusterVolumeMesh(ClusterCoord, LOD, MoveTemp(EmptyMesh));

        // Orphelin et plein prouve n'ont rien alloue : ils ne consomment pas le plafond des sautes.
        return (Build == EClusterBuildResult::Orphan || Build == EClusterBuildResult::AllSolidFast)
            ? EClusterDispatchResult::SkippedCheap
            : EClusterDispatchResult::Skipped;
    }

    const float EVS = VoxelSize * (float)(1 << LOD);
    CurrentClusterMeshJob++;
    TWeakObjectPtr<AChunckManager> WeakThis(this);

    Async(EAsyncExecution::ThreadPool,
        [WeakThis, Volume = MoveTemp(Volume), MaskVol = MoveTemp(MaskVol), SX, SY, SZ, EVS, ClusterCoord, LOD, MyVersion]() mutable
        {
            if (!WeakThis.IsValid()) return;
            FChunckMeshData Mesh;
            WeakThis->GenerateGreedyMeshVolume(Mesh, Volume, MaskVol, SX, SY, SZ, EVS);

            AsyncTask(ENamedThreads::GameThread,
                [WeakThis, Mesh = MoveTemp(Mesh), ClusterCoord, LOD, MyVersion]() mutable
                {
                    AChunckManager* M = WeakThis.Get();
                    if (!M) return;

                    // Garde d'obsolescence (GT-only : pas de lock necessaire).
                    TMap<FIntVector, int32>& VMap =
                        (LOD == 1) ? M->ClusterMeshVersionTier1 :
                        (LOD == 2) ? M->ClusterMeshVersionTier2 : M->ClusterMeshVersionTier3;

                    if (VMap.FindRef(ClusterCoord) == MyVersion)
                    {
                        if (Mesh.bIsEmpty)
                        {
                            ++M->PipeTotal.ClusterEmpty;   // ce que la detection n'a pas su eviter
                        }
                        M->ApplyClusterVolumeMesh(ClusterCoord, LOD, MoveTemp(Mesh));
                        ++M->PipeTotal.ClusterApplied;
                    }
                    else
                    {
                        ++M->PipeTotal.ClusterStale;
                    }
                    M->CurrentClusterMeshJob = FMath::Max(0, M->CurrentClusterMeshJob - 1);
                });
        });
    return EClusterDispatchResult::Dispatched;
}
*/

void AChunckManager::ApplyClusterVolumeMesh(FIntVector ClusterCoord, int32 LOD, FChunckMeshData&& MeshData)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ApplyClusterVolumeMesh);

    TMap<FIntVector, URealtimeMeshComponent*>* Pool =
        (LOD == 1) ? &ClusterPoolTier1 : (LOD == 2) ? &ClusterPoolTier2 : &ClusterPoolTier3;

    // Create et Update ne sont pas interchangeables, et l'existence du composant ne
    // dit PAS si un groupe existe : un mesh vide le retire sans detruire le composant.
    TSet<FIntVector>* HasGroup =
        (LOD == 1) ? &ClusterHasGroupTier1 : (LOD == 2) ? &ClusterHasGroupTier2 : &ClusterHasGroupTier3;

    const int32 NbChunk = GetNbChunkForLOD(LOD);
    const float ClusterWorldSize = (float)NbChunk * ChunkSize * VoxelSize;
    const FVector ClusterOrigin(
        ClusterCoord.X * ClusterWorldSize,
        ClusterCoord.Y * ClusterWorldSize,
        ClusterCoord.Z * (float)ChunkSize * VoxelSize);

    const FRealtimeMeshSectionGroupKey GroupKey =
        FRealtimeMeshSectionGroupKey::Create(FRealtimeMeshLODKey(0), FName("Cluster"));
    

    // Les chunks de cette empreinte qui attendaient leur nouvelle representation
    // viennent de l'obtenir. DOIT etre appele sur TOUS les chemins de sortie.
    auto NotifyFootprint = [&]()
        {
            if (ChunksAwaitingMesh.Num() == 0) return;
            TArray<FIntVector> Applied;
            for (int32 cx = 0; cx < NbChunk; ++cx)
                for (int32 cy = 0; cy < NbChunk; ++cy)
                {
                    const FIntVector C(ClusterCoord.X * NbChunk + cx,
                        ClusterCoord.Y * NbChunk + cy, ClusterCoord.Z);
                    if (ChunksAwaitingMesh.Contains(C)) Applied.Add(C);
                }
            // Copie avant iteration : NotifyDisplayApplied modifie ChunksAwaitingMesh.
            for (const FIntVector& C : Applied) NotifyDisplayApplied(C);
        };

    URealtimeMeshComponent* RMC = nullptr;
    if (URealtimeMeshComponent** Found = Pool->Find(ClusterCoord)) RMC = *Found;

    // Rien a afficher et rien d'existant : ne cree pas un composant pour du vide.
    if (MeshData.bIsEmpty && !RMC)
    {
        NotifyFootprint();
        return;
    }
    

    if (!RMC)
    {
        RMC = NewObject<URealtimeMeshComponent>(this);
        // Mobility AVANT RegisterComponent : un composant Static deja enregistre
        // ne peut plus etre deplace -> le cluster resterait plante a l'origine.
        RMC->SetMobility(EComponentMobility::Movable);
        RMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // clusters LOD>0 = visuel seul
        RMC->bVisibleInRayTracing = false;                          // evite les reconstructions de SBT
        RMC->SetCastShadow(false);                                  // evite les invalidations VSM
        RMC->RegisterComponent();
        RMC->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
        RMC->SetWorldLocation(ClusterOrigin);

        // Le composant fabrique et possede son mesh. Un NewObject<URealtimeMeshSimple>
        // produirait un objet orphelin que le composant ignorerait.
        if (URealtimeMeshSimple* NewMesh = RMC->InitializeRealtimeMesh<URealtimeMeshSimple>())
        {
            NewMesh->SetupMaterialSlot(0, TEXT("Cluster"), ClusterMaterial);
        }

        Pool->Add(ClusterCoord, RMC);
    }

    URealtimeMeshSimple* ClusterMesh = Cast<URealtimeMeshSimple>(RMC->GetRealtimeMesh());
    if (!ClusterMesh)
    {
        NotifyFootprint();
        return;
    }

    // Cluster devenu vide : on retire le groupe plutot que de pousser un StreamSet vide
    // (crash connu en RMC 5.3.2). Le composant reste dans le pool, pret a resservir.
    if (MeshData.bIsEmpty)
    {
        if (HasGroup->Contains(ClusterCoord))
        {
            ClusterMesh->RemoveSectionGroup(GroupKey);
            HasGroup->Remove(ClusterCoord);
        }
        NotifyFootprint();
        return;
    }

    if (!HasGroup->Contains(ClusterCoord))
    {
        ClusterMesh->CreateSectionGroup(GroupKey, MoveTemp(MeshData.Streams));
        HasGroup->Add(ClusterCoord);
    }
    else
    {
        ClusterMesh->UpdateSectionGroup(GroupKey, MoveTemp(MeshData.Streams));
    }

    NotifyFootprint();
}

void AChunckManager::TrySafeSpawn()
{
     TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_TrySafeSpawn);

    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!IsValid(Pawn)) return;   // pawn pas encore possede : le timer repassera

    // Placement unique au-dessus du sol procedural. Le garde-fou voxel du composant
    // de mouvement empeche ensuite toute chute a travers un sol pas encore meshe.
    float HalfHeightCm = 0.0f;
    if (const ACharacter* Character = Cast<ACharacter>(Pawn))
    {
        HalfHeightCm = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    }
    FVector Safe = Pawn->GetActorLocation();
    Safe.Z = GetProceduralGroundZ(Safe) + HalfHeightCm + 5.0f * VoxelSize;
    Pawn->SetActorLocation(Safe, false, nullptr, ETeleportType::TeleportPhysics);
    GetWorld()->GetTimerManager().ClearTimer(SafeSpawnTimer);
}
bool AChunckManager::CompareMask(const FMask& M1, const FMask& M2)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_CompareMask);

    return M1.Block == M2.Block && M1.Normal == M2.Normal;
}


bool AChunckManager::ShouldRebuildVisibility() const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ShouldRebuildVisibility);

    if(bNeedsInitialBuild || bPlayerChangedChunk || bStreamingSettingsDirty || bForceVisibilityRefresh)
    {
        return true;
    }

    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!IsValid(Pawn)) return false;

    return FVector::DistSquared(Pawn->GetActorLocation(), LastRebuildPosition)
        > LODRebuildDistance * LODRebuildDistance;
}

bool AChunckManager::ResolveVoxelWorldIfNeeded()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ResolveVoxelWorldIfNeeded);

    if (IsValid(VoxelWorld)) return true;
    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVoxelWorld::StaticClass(), Found);
    if (Found.Num() > 0) VoxelWorld = Cast<AVoxelWorld>(Found[0]);
    return IsValid(VoxelWorld);
}

void AChunckManager::RebuildDesiredChunkSet(TMap <FIntVector, int32>& OutChunksToKeep)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_RebuildDesiredChunkSet);
    OutChunksToKeep.Reset();
    const int32 HR = FMath::Max(0, HorizontalViewDistance);
    const int32 VR = FMath::Max(0, VerticalViewDistance);
    const int32 HRSq = HR * HR;

    const int32 ColumnsBefore = ColumnBoundsCache.Num();
    const double StartTime = FPlatformTime::Seconds();
    int32 CulledAir = 0;
    FScopeLock Lock(&VoxelWorld->ChunckMutex);

    for (const TPair<APawn*, FIntVector>& Src : LastPlayerChunks)
    {
        if (!IsValid(Src.Key)) continue;
        if (!IsValid(VoxelWorld)) return;
        const FVector PlayerPos = Src.Key->GetActorLocation();
        const FIntVector& Center = Src.Value;
        for (int32 dx = -HR; dx <= HR; ++dx)
            for (int32 dy = -HR; dy <= HR; ++dy)
            {
                if (dx * dx + dy * dy > HRSq) continue;          // disque horizontal
                FIntPoint Column(Center.X + dx, Center.Y + dy);
                FColumnBounds Bounds = GetColumnBounds(Column);
                for (int32 dz = -VR; dz <= VR; ++dz)
                {
                    FIntVector Coord = Center + FIntVector(dx, dy, dz);
                    if (IsChunkGuaranteedEmpty(Coord))
                    {
                        continue;
                    }
                    const bool bIsEdited = VoxelWorld->EditLayers.Contains(Coord);
                    if (!bIsEdited && (float)(Coord.Z * ChunkSize) >= Bounds.MaxH)
                    {
                        ++CulledAir;
                        continue;
                    }
                    const int32 LOD = GetLODForChunck(Coord, PlayerPos);
                    if (int32* Cur = OutChunksToKeep.Find(Coord))
                        *Cur = FMath::Min(*Cur, LOD);
                    else
                        OutChunksToKeep.Add(Coord, LOD);

                }
            }
    }
    static double LastLog = 0.0;
    const double Now = FPlatformTime::Seconds();
    if (Now - LastLog > 5.0)
    {
        LastLog = Now;
        UE_LOG(LogTemp, Display,
            TEXT("DESIRED | gardes=%d | air retires=%d (%.1f%%) | colonnes calculees=%d (cache=%d) | %.1f ms"),
            OutChunksToKeep.Num(), CulledAir,
            100.0 * CulledAir / FMath::Max(1, CulledAir + OutChunksToKeep.Num()),
            ColumnBoundsCache.Num() - ColumnsBefore, ColumnBoundsCache.Num(),
            (Now - StartTime) * 1000.0);

        UE_LOG(LogTemp, Display, TEXT("SHADOW | plein predit=%lld | erreurs LOD0=%lld LOD1=%lld LOD2=%lld LOD3=%lld"),
            ShadowPredicted, ShadowMiss[0], ShadowMiss[1], ShadowMiss[2], ShadowMiss[3]);
    }
}

void AChunckManager::BuildStreamingQueues(const TMap <FIntVector, int32>& DesiredChunks)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_BuildStreamingQueues);

    if (!IsValid(VoxelWorld)) return;

    // Snapshot des chunks existants UNE fois (un seul lock), pas par-coord.
    TMap <FIntVector, int32> Existing;
    {
        FScopeLock Lock(&VoxelWorld->ChunckMutex);
        Existing.Reserve(VoxelWorld->Chuncks.Num());
        for (const TPair<FIntVector, FChunckDataStructure>& P : VoxelWorld->Chuncks)
            Existing.Add(P.Key, P.Value.LOD);
    }

    FIntVector PlayerChunk = FIntVector::ZeroValue;
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        if (APawn* Pawn = PC->GetPawn())
            PlayerChunk = GetPlayerChunck(Pawn->GetActorLocation());

    TArray<FIntVector> ToSpawn;
    for (const TPair<FIntVector, int32>& P : DesiredChunks)
    {
        const FIntVector Coord = P.Key;
        const int32* CurrentLOD = Existing.Find(Coord);
        if (!CurrentLOD)
        {
            if (!PendingSpawnSet.Contains(Coord))
            {
                ToSpawn.Add(Coord);
            }
        }
        else if (*CurrentLOD != P.Value)
        {
            if (!PendingTransitionSet.Contains(Coord))
            {
                PendingTransitionQueue.Enqueue(Coord);
                PendingTransitionSet.Add(Coord);
            }
        }
        PendingUnloadSet.Remove(Coord);   // redevenu desire -> annule un unload en attente
    }

    // Distance au carre en unites de chunk : arithmetique entiere, pas de sqrt.
    ToSpawn.Sort([&PlayerChunk](const FIntVector& A, const FIntVector& B)
        {
            const FIntVector DA = A - PlayerChunk;
            const FIntVector DB = B - PlayerChunk;
            return (DA.X * DA.X + DA.Y * DA.Y + DA.Z * DA.Z)
                < (DB.X * DB.X + DB.Y * DB.Y + DB.Z * DB.Z);
        });

    for (const FIntVector& Coord : ToSpawn)
    {
        PendingSpawnQueue.Enqueue(Coord);
        PendingSpawnSet.Add(Coord);
    }

    // UNLOAD : Existing − Desired
    for (const TPair<FIntVector, int32> P : Existing)
    {
        FIntVector Coord = P.Key;
        if (!DesiredChunks.Contains(Coord))
        {
            if (!PendingUnloadSet.Contains(Coord))
            {
                PendingUnloadQueue.Enqueue(Coord);
                PendingUnloadSet.Add(Coord);
            }
            PendingSpawnSet.Remove(Coord);   // plus désiré → annule un spawn en attente
        }
    }
}

void AChunckManager::ProcessSpawnQueue()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ProcessSpawnQueue);

    if (!IsValid(VoxelWorld)) return;
    int32 Spawned = 0;
    APawn* Pawn = GetWorld()->GetFirstPlayerController()->GetPawn();
    if (!Pawn) return;

    FVector PlayerPos = Pawn->GetActorLocation();
    while (Spawned < MaxSpawnPerFrame)
    {
        FIntVector Coord;
        if (!PendingSpawnQueue.Dequeue(Coord)) break;
        if (!PendingSpawnSet.Contains(Coord)) continue;   // annulé
        PendingSpawnSet.Remove(Coord);
        const int32 LOD = GetLODForChunck(Coord, PlayerPos);
        SpawnChunk(Coord, LOD, 1);                          // GenId=1 pour un spawn neuf
        ++PipeTotal.Spawned;
        ++Spawned;
    }
}

void AChunckManager::ProcessGenerationQueue()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ProcessGenerationQueue);

    if (!IsValid(VoxelWorld)) return;
    const double Deadline = FPlatformTime::Seconds() + GenDispatchBudgetMs * 0.001;
    int32 Dispatched = 0;
    while (Dispatched < MaxGenPerFrame)
    {
        if ((Dispatched & 31) == 0 && FPlatformTime::Seconds() > Deadline)
        {
            break;
        }
        FIntVector Coord;
        if (!ChunckGenerationQueue.Dequeue(Coord)) break;

        int32 ChunkLOD = 0;
        int32 GenerationId = 0;
        bool bDispatch = false;
        {
            FScopeLock Lock(&VoxelWorld->ChunckMutex);
            if (FChunckDataStructure* Data = VoxelWorld->Chuncks.Find(Coord))
            {
                if (Data->Phase == EChunkLODPhase::GeneratingData)
                {
                    ChunkLOD = Data->PendingLOD;              // le LOD CIBLE
                    GenerationId = Data->PendingGenerationId;
                    bDispatch = true;
                }
                else if (!Data->bIsChunckGenerated)
                {
                    ChunkLOD = Data->LOD;
                    GenerationId = Data->GenerationId;
                    bDispatch = true;
                }
            }
        }
        if (!bDispatch) continue;                           // déchargé ou déjà généré
        FillChunck(EChunkVariant::Full, Coord, ChunkLOD, GenerationId);   // → ChunckGenerationJobQueue
        ++PipeTotal.GenDispatched;
        ++Dispatched;
    }
}

void AChunckManager::ProcessGenerationResults()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ProcessGenerationResults);

    if (!IsValid(VoxelWorld)) return;

    const double Deadline = FPlatformTime::Seconds() + 0.003;
    FChunkGenResult Result;
    while (ChunckGenerationResult.Dequeue(Result))
    {
        PendingClusterGenCount += 1;
        ++PipeTotal.GenReturned;

        bool bAccepted = false;
        bool bSelfDirty = false;
        bool bReadyToCommit = false;
        int32 ResultLOD = 0;
        TArray<FIntVector, TInlineAllocator<6>> ExistingNeighbors;
        if (const FColumnBounds* Bound = ColumnBoundsCache.Find(FIntPoint(Result.Coord.X, Result.Coord.Y)))
        {
            if (!VoxelWorld->EditLayers.Contains(Result.Coord)
                && IsChunkGuaranteedSolid(Result.Coord, *Bound))
            {
                ++ShadowPredicted;
                if (!Result.bIsAllSolid)
                {
                    const int32 L = FMath::Clamp(Result.LOD, 0, 3);
                    ++ShadowMiss[L];
                    if (ShadowMiss[L] <= 20)
                    {
                        UE_LOG(LogTemp, Error, TEXT("SHADOW | (%d,%d,%d) LOD%d predit PLEIN mais ne l'est pas"),
                            Result.Coord.X, Result.Coord.Y, Result.Coord.Z, Result.LOD);
                    }
                }
            }
        }
        {
            FScopeLock Lock(&VoxelWorld->ChunckMutex);
            
            FChunckDataStructure* Data = VoxelWorld->Chuncks.Find(Result.Coord);
            if (!Data) continue;
            

            if (Data->Phase == EChunkLODPhase::GeneratingData
                && Result.GenerationId == Data->PendingGenerationId)
            {
                Data->PendingVoxels = MoveTemp(Result.Voxels);
                Data->bPendingIsAllSolid = Result.bIsAllSolid;
                bReadyToCommit = true;
            }
            else if (Result.GenerationId == Data->GenerationId)
            {
                Data->Voxels = MoveTemp(Result.Voxels);
                Data->bIsAllSolid = Result.bIsAllSolid;
                Data->bIsChunckGenerated = true;
                ResultLOD = Result.LOD;

                if (Result.LOD == 0)                      // le bloc qui utilise Dirs
                {
                    bSelfDirty = !Result.bIsAllEmpty;
                    for (const FIntVector& Dir : Dirs)
                        if (const FChunckDataStructure* N = VoxelWorld->Chuncks.Find(Result.Coord + Dir))
                            if (N->LOD == 0)              // n'enfile que du LOD0 réel
                                ExistingNeighbors.Add(Result.Coord + Dir);
                }
                bAccepted = true;
            }
            else continue;
        }

        if (bReadyToCommit)
        {
            PendingCommitSet.Add(Result.Coord);
            TelemetryStamp(Result.Coord, &FChunkJobTelemetry::TCommited, FPlatformTime::Seconds());
        }

        if (bAccepted)                                    // bloc, PAS un continue
        {
            ++PipeTotal.GenAccepted;
            if (ResultLOD == 0)
            {
                TelemetryStamp(Result.Coord, &FChunkJobTelemetry::TResultConsumed, FPlatformTime::Seconds());
                if (bSelfDirty) DirtyChuncks.Add(Result.Coord);
                for (const FIntVector& N : ExistingNeighbors) DirtyChuncks.Add(N);
            }
            else
            {
                RequestClusterRebuild(GetClusterCoord(Result.Coord, ResultLOD), ResultLOD);
            }
        }

        if (FPlatformTime::Seconds() > Deadline) break;
    }
}

void AChunckManager::ProcessDirtyChunks()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ProcessDirtyChunks);

    if (!IsValid(VoxelWorld)) return;
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!IsValid(Pawn)) return;
    const FVector PlayerPos = Pawn->GetActorLocation();

    TArray<FIntVector> DirtyToProcess = DirtyChuncks.Array();
    DirtyToProcess.Sort([&](const FIntVector& A, const FIntVector& B)
        {
            return FVector::DistSquared(PlayerPos, FVector(A) * ChunkSize * VoxelSize)
                < FVector::DistSquared(PlayerPos, FVector(B) * ChunkSize * VoxelSize);
        });

    int32 RebuildCount = 0;
    for (const FIntVector& Coord : DirtyToProcess)
    {
        if (RebuildCount >= MaxRebuildPerFrame) break;
        if (!DirtyChuncks.Contains(Coord)) continue;

        const int32 DesiredLODLocal = GetLODForChunck(Coord, PlayerPos);
        AVoxelChunck* VoxelChunck = nullptr;
        bool bRemoveDirty = false, bReady = false;
        {
            FScopeLock Lock(&VoxelWorld->ChunckMutex);
            FChunckDataStructure* Data = VoxelWorld->Chuncks.Find(Coord);
            if (!Data)                          bRemoveDirty = true;
            else if (Data->LOD != 0)            bRemoveDirty = true;   // ← plus un chunk-acteur
            else if (!Data->bIsChunckGenerated) continue;              // pas prêt → reste dirty
            else
            {
                VoxelChunck = Data->VoxelChunck.Get();
                if (!IsValid(VoxelChunck)) bRemoveDirty = true;
                else                       bReady = true;
            }
        }
        if (bRemoveDirty) { DirtyChuncks.Remove(Coord); continue; }
        if (!bReady) continue;

        VoxelChunck->CurrentLOD = DesiredLODLocal;
        if (!VoxelChunck->bIsQueued)                          // flag porté par l'ACTEUR (cf. fix #2)
        {
            VoxelChunck->bIsQueued = true;
            PendingMeshToApply.Enqueue(Coord);                // file de FIntVector
            ++RebuildCount;
            DirtyChuncks.Remove(Coord);
        }


    }
}

void AChunckManager::ProcessMeshJobs()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ProcessMeshJobs);

    if (!IsValid(VoxelWorld)) return;
    while (CurrentMeshJob < MaxMeshJob)
    {
        FIntVector Coord;
        if (!PendingMeshToApply.Dequeue(Coord)) break;
        ++CurrentMeshJob;                 // réserve le slot ; GenerateAsyncGreedyMesh le rend sur TOUS ses chemins
        TelemetryStamp(Coord, &FChunkJobTelemetry::TMeshDispatched, FPlatformTime::Seconds());
        GenerateAsyncGreedyMesh(Coord);
    }
}

void AChunckManager::ProcessPendingClusters()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ProcessPendingClusters);

    if (!IsValid(VoxelWorld)) return;

    APawn* Pawn = GetWorld()->GetFirstPlayerController()->GetPawn();
    if (!Pawn)
    {
        return;
    }
    FVector PlayerPos = Pawn->GetActorLocation();
    const FIntVector PlayerChunk = GetPlayerChunck(PlayerPos);
    const bool bCenterMoved = (PlayerChunk != LastClusterSortCenter);
    LastClusterSortCenter = PlayerChunk;
    int32 Dispatched = 0;   // meshes reellement lances : consomment des places
    int32 Skipped = 0;      // traites sur place : ne consomment pas de place, mais du game thread
    const int32 EffectiveClusterBudget = MaxClusterMeshJob;
    LastClusterBudget = EffectiveClusterBudget;
    const double ClusterDeadline = FPlatformTime::Seconds() + ClusterBudgetMs * 0.001;

    bool bBudgetHitThisFrame = false;

    auto ProcessTier = [&](TSet<FIntVector>& Pending, FClusterTierOrder& Order, int32 LOD)
        {
            if (Pending.Num() == 0)
            {
                Order.Sorted.Reset();   // vide sans liberer la memoire : reutilisee au prochain remplissage
                return;
            }

            if (Order.bDirty || bCenterMoved)
            {
                TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ClusterSort);
                const double SortStart = FPlatformTime::Seconds();
                Order.Sorted = Pending.Array();
                Order.Sorted.Sort([&](const FIntVector& A, const FIntVector& B)
                    {
                        return FVector::DistSquared(PlayerPos, FVector(A) * ChunkSize * VoxelSize)
                            < FVector::DistSquared(PlayerPos, FVector(B) * ChunkSize * VoxelSize);
                    });
                Order.bDirty = false;
                PipeTotal.ClusterSortSeconds += FPlatformTime::Seconds() - SortStart;
                ++PipeTotal.ClusterSorts;

            }
            TMap<FIntVector, uint64>& LastMap =
                (LOD == 1) ? LastRequestFrameClusterTier1 :
                (LOD == 2) ? LastRequestFrameClusterTier2 : LastRequestFrameClusterTier3;

            TMap<FIntVector, uint64>& FirstMap =
                (LOD == 1) ? FirstRequestFrameClusterTier1 :
                (LOD == 2) ? FirstRequestFrameClusterTier2 : FirstRequestFrameClusterTier3;

            bool bAnyRemoved = false;
            for (const FIntVector& CC : Order.Sorted)
            {
                if (FPlatformTime::Seconds() > ClusterDeadline) 
                {
                    bBudgetHitThisFrame = true;
                    break; 
                }
                if (Dispatched >= MaxClusterDispatchPerFrame
                    || CurrentClusterMeshJob >= EffectiveClusterBudget
                    || Skipped >= MaxClusterSkipPerFrame
                    || FPlatformTime::Seconds() > ClusterDeadline)
                {
                    break;
                }
                ++PipeTotal.ClusterVisited;
                if (!Pending.Contains(CC))
                {
                    continue;   // perimee : sortie du set par un autre chemin
                }

                const uint64 Silent = GFrameCounter - LastMap[CC];
                const uint64 Waiting = GFrameCounter - FirstMap[CC];
                if (Silent < ClusterSettleFrames && Waiting < ClusterMaxWaitFrames)
                {
                    continue;
                }

                const EClusterDispatchResult R = TryDispatchClusterMesh(CC, LOD);
                if (R == EClusterDispatchResult::NotReady)
                {
                    ++PipeTotal.ClusterDispatchFail;
                    continue;
                }

                // Traite (lance OU saute) : sort du set dans les deux cas.
                FirstMap.Remove(CC);
                Pending.Remove(CC);   // l'entree du tableau devient perimee, on n'y touche pas ici
                bAnyRemoved = true;

                if (R == EClusterDispatchResult::Dispatched)
                {
                    ++PipeTotal.ClusterDispatched;
                    ++Dispatched;
                }
                else if (R == EClusterDispatchResult::Skipped)
                {
                    ++Skipped;   // seuls les sautes COUTEUX consomment ce plafond
                }
            }
            if (bAnyRemoved)
            {
                Order.Sorted.RemoveAll([&Pending](const FIntVector& C)
                    {
                        return !Pending.Contains(C);
                    });
            }
        };
    ProcessTier(PendingClusterTier1, ClusterOrderTier1, 1);
    ProcessTier(PendingClusterTier2, ClusterOrderTier2, 2);
    ProcessTier(PendingClusterTier3, ClusterOrderTier3, 3);
    if (bBudgetHitThisFrame) ++PipeTotal.ClusterBudgetHit;
}bool AChunckManager::UpdatePlayerChunkState()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_UpdatePlayerChunkState);

    UWorld* World = GetWorld();
    if (!World)
    {
        bPlayerChangedChunk = false;
        return false;
    }

    bool bAnyChanged = false;

    // Quels pawns existent encore cette frame (pour purger les partis)
    TSet<APawn*> SeenThisFrame;
    SeenThisFrame.Reserve(LastPlayerChunks.Num() + 1);

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC) continue;

        APawn* Pawn = PC->GetPawn();
        if (!Pawn) continue;

        SeenThisFrame.Add(Pawn);

        const FIntVector CurrentChunk = GetPlayerChunck(Pawn->GetActorLocation());

        if (FIntVector* Cached = LastPlayerChunks.Find(Pawn))
        {
            if (*Cached != CurrentChunk)   // a VRAIMENT franchi une frontière
            {
                *Cached = CurrentChunk;    // écrit À TRAVERS le pointeur → maj la map
                bAnyChanged = true;
            }
        }
        else
        {
            LastPlayerChunks.Add(Pawn, CurrentChunk);   // nouveau pawn
            bAnyChanged = true;
        }
    }

    // Purge des pawns disparus — APRÈS la boucle, quand SeenThisFrame est complet
    for (auto It = LastPlayerChunks.CreateIterator(); It; ++It)
    {
        if (!SeenThisFrame.Contains(It.Key()))
        {
            It.RemoveCurrent();
            bAnyChanged = true;
        }
    }

    bPlayerChangedChunk = bAnyChanged;
    return bAnyChanged;
}

void AChunckManager::ProcessTransitionQueue()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ProcessTransitionQueue);

    if (!IsValid(VoxelWorld)) return;
    int32 Done = 0;
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!IsValid(Pawn)) return;
    const FVector PlayerPos = Pawn->GetActorLocation();

    while (Done < MaxSpawnPerFrame)
    {
        // Garde mémoire : un chunk en transition détient DEUX tableaux de voxels.
        // Sans plafond, un franchissement de frontière peut en mettre des milliers
        // en vol simultanément.
        if (PendingCommitSet.Num() + ChunksAwaitingMesh.Num() >= MaxConcurrentLODTransitions)
            break;

        FIntVector Coord;
        if (!PendingTransitionQueue.Dequeue(Coord)) break;
        if (!PendingTransitionSet.Contains(Coord)) continue;
        PendingTransitionSet.Remove(Coord);

        const int32 NewLOD = GetLODForChunck(Coord, PlayerPos);

        bool bStarted = false;
        {
            FScopeLock Lock(&VoxelWorld->ChunckMutex);
            FChunckDataStructure* D = VoxelWorld->Chuncks.Find(Coord);
            if (!D) continue;
            if (D->Phase != EChunkLODPhase::Stable) continue;   // transition déjà en cours
            if (D->LOD == NewLOD)                   continue;   // rien à faire

            D->PendingLOD = NewLOD;
            D->PendingGenerationId = ++NextGenerationId;
            D->Phase = EChunkLODPhase::GeneratingData;
            D->PhaseEnteredTime = FPlatformTime::Seconds();
            bStarted = true;
            // AUCUN Remove, AUCUN Destroy, AUCUN blanchiment de cluster.
            // L'ancienne représentation continue de s'afficher, intacte.
        }

        if (!bStarted) continue;

        ChunckGenerationQueue.Enqueue(Coord);
        ++Done;
    }
}
void AChunckManager::ProcessUnloadQueue()
{
TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ProcessUnloadQueue);

if (!IsValid(VoxelWorld)) return;

int32 UnloadedThisFrame = 0;

TSet<FIntVector> ClustersTier1Touched;
TSet<FIntVector> ClustersTier2Touched;
TSet<FIntVector> ClustersTier3Touched;
while (UnloadedThisFrame < MaxUnloadPerFrame)
{
    FIntVector Coord;
    if (!PendingUnloadQueue.Dequeue(Coord)) break;
    if (!PendingUnloadSet.Contains(Coord)) continue;
    PendingUnloadSet.Remove(Coord);
    bool bDestroyed = false;
    int32 UnloadedLOD = 0;
    int32 PendingRelease = INDEX_NONE;
    FIntVector PendingReleaseCC = FIntVector::ZeroValue;
{
    FScopeLock Lock(&VoxelWorld->ChunckMutex);
    FChunckDataStructure* Data = VoxelWorld->Chuncks.Find(Coord);
    if (Data)
    {
        UnloadedLOD = Data->LOD;
                    // Le chunk part alors qu'il attendait sa nouvelle représentation :
                    // son ANCIENNE doit être libérée aussi, sinon elle reste affichée.
                    if (Data->Phase == EChunkLODPhase::AwaitingNewMesh)
                    {
                        PendingRelease = Data->ReleaseLOD;

                        PendingReleaseCC = Data->ReleaseClusterCoord;
                    }

                    if (Data->VoxelChunck.IsValid())
                    {
                        Data->VoxelChunck->bIsBeingDestroyed = true;
                        Data->VoxelChunck->Destroy();
                    }

                    VoxelWorld->Chuncks.Remove(Coord);
                    bDestroyed = true;
                }
            }

            if (!bDestroyed) continue;

            if (TObjectPtr<UInstancedStaticMeshComponent>* ISM = TreeISMPerChunk.Find(Coord))
            {
                ISM->Get()->DestroyComponent();
                TreeISMPerChunk.Remove(Coord);
            }


            // Purge des états de transition — sinon fuite permanente dans les deux sets
            // et logs watchdog en boucle sur un chunk qui n'existe plus.
            ChunksAwaitingMesh.Remove(Coord);
            PendingCommitSet.Remove(Coord);

            if (UnloadedLOD >= 1)
            {
                const FIntVector CC = GetClusterCoord(Coord, UnloadedLOD);
                if (UnloadedLOD == 1)      ClustersTier1Touched.Add(CC);
                else if (UnloadedLOD == 2) ClustersTier2Touched.Add(CC);
                else                       ClustersTier3Touched.Add(CC);
            }

            if (PendingRelease >= 1)
            {
                if (PendingRelease == 1)      ClustersTier1Touched.Add(PendingReleaseCC);
                else if (PendingRelease == 2) ClustersTier2Touched.Add(PendingReleaseCC);
                else                          ClustersTier3Touched.Add(PendingReleaseCC);
            }

            ++UnloadedThisFrame;
        }

        for (const FIntVector& CC : ClustersTier1Touched) RequestClusterRebuild(CC, 1);
        for (const FIntVector& CC : ClustersTier2Touched) RequestClusterRebuild(CC, 2);
        for (const FIntVector& CC : ClustersTier3Touched) RequestClusterRebuild(CC, 3);
    }

bool AChunckManager::IsChunkGuaranteedEmpty(const FIntVector& Coord) const
{
    // Bas du chunk en voxels-monde. Marge d'un chunk pour absorber tout dépassement de bruit/octaves.
    const int32 ChunkMinZVoxel = Coord.Z * ChunkSize;
    const int32 MaxSurfaceVoxel = FMath::CeilToInt(TerrainGenerator.GetMaxPossibleHeight()) + ChunkSize;
    return Coord.Z * ChunkSize >= MaxSurfaceVoxel;   // tout le chunk est au-dessus de toute surface possible
}

bool AChunckManager::IsChunkGuaranteedSolid(const FIntVector& Coord, const FColumnBounds& Bounds) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_IsChunkGuaranteedSolid);
    float TopLevel = Coord.Z * ChunkSize + ChunkSize - 1;
    if (TopLevel + 1 >= Bounds.MinH)
    {
        return false;
    }
    float Half = (ChunkSize - 1) * 0.5;
    FVector Center = FVector(Coord * ChunkSize) + Half;
    float Radius = (Half + 1) * FMath::Sqrt(3.0f);
    return TerrainGenerator.CaveNoiseAt(Center.X, Center.Y, Center.Z) + CaveLipschitz * Radius <= TerrainGenerator.GetCaveThreshold();
}

void AChunckManager::ProcessLODCommits()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ProcessLODCommits);

    if (!IsValid(VoxelWorld) || PendingCommitSet.Num() == 0) return;

    for (const FIntVector& Coord : PendingCommitSet.Array())
    {
        PendingCommitSet.Remove(Coord);

        int32 OldLOD = INDEX_NONE, NewLOD = INDEX_NONE;
        bool bNeedActor = false;
        {
            FScopeLock Lock(&VoxelWorld->ChunckMutex);
            FChunckDataStructure* ChunkDataStructure = VoxelWorld->Chuncks.Find(Coord);
            if (!ChunkDataStructure || ChunkDataStructure->Phase != EChunkLODPhase::GeneratingData) continue;

            OldLOD = ChunkDataStructure->LOD;
            NewLOD = ChunkDataStructure->PendingLOD;

            // Bascule atomique de la donnée
            ChunkDataStructure->Voxels = MoveTemp(ChunkDataStructure->PendingVoxels);
            ChunkDataStructure->bIsAllSolid = ChunkDataStructure->bPendingIsAllSolid;
            ChunkDataStructure->PendingVoxels.Empty();
            ChunkDataStructure->LOD = NewLOD;
            ChunkDataStructure->GenerationId = ChunkDataStructure->PendingGenerationId;
            ChunkDataStructure->bIsChunckGenerated = true;

            // Ce qu'il faudra libérer QUAND la nouvelle représentation sera affichée
            ChunkDataStructure->ReleaseLOD = OldLOD;
            ChunkDataStructure->ReleaseClusterCoord = (OldLOD >= 1)
                ? GetClusterCoord(Coord, OldLOD) : FIntVector::ZeroValue;

            ChunkDataStructure->Phase = EChunkLODPhase::AwaitingNewMesh;
            ChunkDataStructure->PhaseEnteredTime = FPlatformTime::Seconds();

            bNeedActor = (NewLOD == 0 && !ChunkDataStructure->VoxelChunck.IsValid());
        }

        ChunksAwaitingMesh.Add(Coord);

        if (bNeedActor)   // SpawnActor hors lock
        {
            AVoxelChunck* A = GetWorld()->SpawnActor<AVoxelChunck>(
                VoxelChunckClass, FVector(Coord) * ChunkSize * VoxelSize, FRotator::ZeroRotator);
            if (IsValid(A))
            {
                A->SetChunckManager(this);
                A->Coord = Coord;
                FScopeLock Lock(&VoxelWorld->ChunckMutex);
                if (FChunckDataStructure* D = VoxelWorld->Chuncks.Find(Coord))
                    D->VoxelChunck = A;
            }
        }

        // Construire la NOUVELLE représentation. L'ancienne reste affichée.
        if (NewLOD == 0)
        {
            DirtyChuncks.Add(Coord);
            TArray<FIntVector> NeighboorArray;
            FScopeLock Lock(&VoxelWorld->ChunckMutex);
            for (FIntVector Dir : Dirs)
            {
                if (FChunckDataStructure* D = VoxelWorld->Chuncks.Find(Coord + Dir))
                {
                    if (D->LOD == 0)
                    {
                        NeighboorArray.Add(Coord + Dir);
                    }
                }
            }
            for (FIntVector NeighboorCoord : NeighboorArray)
            {
                DirtyChuncks.Add(NeighboorCoord);
            }
        }
        else
        {
            RequestClusterRebuild(GetClusterCoord(Coord, NewLOD), NewLOD);
        }
    }
}

void AChunckManager::RequestClusterRebuild(FIntVector CC, int32 Tier)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_RequestClusterRebuild);
    int32 Count = ++ChunkAlreadyBuilt.FindOrAdd(CC);
    /*if (Count > 1)
    {
        NathanDebug(TEXT("Cluster (%d,%d,%d) rebati %d fois"), CC.X, CC.Y, CC.Z, Count);
    }
    */
    if (Tier < 1) return;
    TMap<FIntVector, int32>& VMap =
        (Tier == 1) ? ClusterMeshVersionTier1 :
        (Tier == 2) ? ClusterMeshVersionTier2 : ClusterMeshVersionTier3;
    ++VMap.FindOrAdd(CC);        // les jobs en vol deviennent périmés — ça reste nécessaire

    bool bIsAlreadyInSet = false;
    TSet<FIntVector>& PendingSet =
        (Tier == 1) ? PendingClusterTier1 :
        (Tier == 2) ? PendingClusterTier2 : PendingClusterTier3;
    FClusterTierOrder& Order =
        (Tier == 1) ? ClusterOrderTier1 :
        (Tier == 2) ? ClusterOrderTier2 : ClusterOrderTier3;
    PendingSet.Add(CC, &bIsAlreadyInSet);
    if (!bIsAlreadyInSet)
    {
        Order.bDirty = true;   // seul cas ou l'ordre devient faux
    }
    
    TMap<FIntVector, uint64>& LastMap =
        (Tier == 1) ? LastRequestFrameClusterTier1 :
        (Tier == 2) ? LastRequestFrameClusterTier2 : LastRequestFrameClusterTier3;

    TMap<FIntVector, uint64>& FirstMap =
        (Tier == 1) ? FirstRequestFrameClusterTier1 :
        (Tier == 2) ? FirstRequestFrameClusterTier2 : FirstRequestFrameClusterTier3;

    LastMap.Add(CC, GFrameCounter);

    if (!FirstMap.Contains(CC))
    {
        FirstMap.Add(CC, GFrameCounter);
    }

}

void AChunckManager::NotifyDisplayApplied(FIntVector Coord)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_NotifyDisplayApplied);

    if (!IsValid(VoxelWorld)) return;

    int32 ReleaseLOD = INDEX_NONE;
    FIntVector ReleaseCC = FIntVector::ZeroValue;
    AVoxelChunck* ActorToKill = nullptr;
    {
        FScopeLock Lock(&VoxelWorld->ChunckMutex);
        FChunckDataStructure* D = VoxelWorld->Chuncks.Find(Coord);
        if (!D || D->Phase != EChunkLODPhase::AwaitingNewMesh)
        {
            ChunksAwaitingMesh.Remove(Coord);
            return;
        }

        ReleaseLOD = D->ReleaseLOD;
        ReleaseCC = D->ReleaseClusterCoord;


        // L'ancienne représentation était un acteur LOD0, et le chunk n'est plus
        // en LOD0 : l'acteur n'affiche plus rien d'utile.
        if (ReleaseLOD == 0 && D->LOD != 0)
        {
            ActorToKill = D->VoxelChunck.Get();
            D->VoxelChunck = nullptr;
        }

        D->ReleaseLOD = INDEX_NONE;
        D->ReleaseClusterCoord = FIntVector::ZeroValue;
        D->Phase = EChunkLODPhase::Stable;
    }
    ChunksAwaitingMesh.Remove(Coord);

    // Deux décisions INDÉPENDANTES, jamais un if/else :
    // ReleaseLOD == 0 -> acteur à détruire ; ReleaseLOD >= 1 -> cluster à rebâtir.
    if (ActorToKill && IsValid(ActorToKill))
    {
        ActorToKill->bIsBeingDestroyed = true;
        ActorToKill->Destroy();
    }

    if (ReleaseLOD >= 1)
    {
        RequestClusterRebuild(ReleaseCC, ReleaseLOD);
    }
}

void AChunckManager::ProcessLODWatchdog()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ProcessLODWatchdog);

    if (!IsValid(VoxelWorld) || ChunksAwaitingMesh.Num() == 0) return;
    const double Now = FPlatformTime::Seconds();

    TArray<FIntVector> TimedOut;
    {
        FScopeLock Lock(&VoxelWorld->ChunckMutex);
        for (const FIntVector& Coord : ChunksAwaitingMesh)
        {
            const FChunckDataStructure* D = VoxelWorld->Chuncks.Find(Coord);
            if (!D || D->Phase != EChunkLODPhase::AwaitingNewMesh
                || (Now - D->PhaseEnteredTime) > LODSwapWatchdogSeconds)

                TimedOut.Add(Coord);
        }
    }

    for (const FIntVector& Coord : TimedOut)
    {
        NotifyDisplayApplied(Coord);
        
    }
}

void AChunckManager::NukeClusters()
{
    const FRealtimeMeshSectionGroupKey GroupKey =
        FRealtimeMeshSectionGroupKey::Create(FRealtimeMeshLODKey(0), FName("Cluster"));

    int32 Cleared = 0;
    for (auto* Pool : { &ClusterPoolTier1, &ClusterPoolTier2, &ClusterPoolTier3 })
        for (auto& P : *Pool)
            if (P.Value)
                if (URealtimeMeshSimple* M = Cast<URealtimeMeshSimple>(P.Value->GetRealtimeMesh()))
                {
                    M->RemoveSectionGroup(GroupKey);
                    ++Cleared;
                }

    // Sans ca, un Apply ulterieur ferait un Update sur un groupe supprime.
    ClusterHasGroupTier1.Empty();
    ClusterHasGroupTier2.Empty();
    ClusterHasGroupTier3.Empty();
}

void AChunckManager::CarveSphereAt(const FVector& WorldCenter, float RadiusMeters)
{
    if (!IsValid(VoxelWorld))
    {
        return;
    }
    int32 EditCount = 0;

    const double StartTime = FPlatformTime::Seconds();
    FIntVector CenterVoxel(FMath::FloorToInt(WorldCenter.X / VoxelSize), FMath::FloorToInt(WorldCenter.Y / VoxelSize), FMath::FloorToInt(WorldCenter.Z / VoxelSize));
    FVoxelBrushOp Op;
    Op.CenterVoxel = CenterVoxel;
    const int32 R = FMath::CeilToInt(RadiusMeters * 100 / VoxelSize);
    Op.RadiusVoxels = R;
    Op.Seq = NextBrushSeq++;
    const FIntVector BoxMin(CenterVoxel.X - R, CenterVoxel.Y - R, CenterVoxel.Z - R);
    const FIntVector BoxMax(CenterVoxel.X + R, CenterVoxel.Y + R, CenterVoxel.Z + R);

    const FIntVector ChunkMin(FloorDivInt(BoxMin.X, ChunkSize),
        FloorDivInt(BoxMin.Y, ChunkSize),
        FloorDivInt(BoxMin.Z, ChunkSize));

    const FIntVector ChunkMax(FloorDivInt(BoxMax.X, ChunkSize),
        FloorDivInt(BoxMax.Y, ChunkSize),
        FloorDivInt(BoxMax.Z, ChunkSize));

    int32 ChunksInRange = 0;
    int32 ChunksFound = 0;
    
        for (int32 cz = ChunkMin.Z; cz <= ChunkMax.Z; ++cz)
            for (int32 cy = ChunkMin.Y; cy <= ChunkMax.Y; ++cy)
                for (int32 cx = ChunkMin.X; cx <= ChunkMax.X; ++cx)
                {
                    ++ChunksInRange;
                    const FIntVector CC(cx, cy, cz);
                    {
                        FScopeLock Lock(&VoxelWorld->ChunckMutex);
                        const FChunckDataStructure* D = VoxelWorld->Chuncks.Find(CC);
                        if (!D) continue;

                    }
                    ++ChunksFound;
                }
    
    int32 CellsTested = 0;
    int32 CellsModified = 0;

    // Rayon au carre : comparer des carres evite un sqrt par cellule.
    const int64 R2 = (int64)R * (int64)R;

    // Chunks reellement modifies, avec leur LOD (le routage dirty en depend).
    TMap<FIntVector, int32> TouchedChunks;

    {
        FScopeLock Lock(&VoxelWorld->ChunckMutex);

        for (int32 cz = ChunkMin.Z; cz <= ChunkMax.Z; ++cz)
            for (int32 cy = ChunkMin.Y; cy <= ChunkMax.Y; ++cy)
                for (int32 cx = ChunkMin.X; cx <= ChunkMax.X; ++cx)
                {
                    ++ChunksInRange;

                    const FIntVector CC(cx, cy, cz);
                    
                    if (IsChunkGuaranteedEmpty(CC))
                    {
                        continue;
                    }
                    
                    FChunkEditLayer& Layer = VoxelWorld->EditLayers.FindOrAdd(CC);
                    Layer.BrushOps.Add(Op);
                    ++Layer.Revision;
                    FChunckDataStructure* ChunkDataStructure = VoxelWorld->Chuncks.Find(CC);   // non-const : on ecrit
                    if (!ChunkDataStructure || ChunkDataStructure->Voxels.Num() == 0) continue;
                    if (ApplyBrushOp(ChunkDataStructure->Voxels.Edit(), CC, ChunkDataStructure->LOD, ChunkSize, Op))
                    {
                        TouchedChunks.Add(CC, ChunkDataStructure->LOD);
                        ChunkDataStructure->bIsAllSolid = false;
                    }
                    ++ChunksFound;

                    
                }
    }   // <-- VERROU RELACHE ICI. Le dispatch qui suit le reprendrait.

    // --- Dispatch des reconstructions, HORS verrou ---
    // Le mesher lit une coque de padding d'un voxel chez ses 6 voisins
    // (cf. BuildChunkPaddedVolumeNoLock). Modifier une face invalide donc
    // aussi le mesh du voisin de ce cote.
    DispatchChunkRebuilds(TouchedChunks);
}

FIntVector AChunckManager::WorldToVoxelGlobal(FVector World) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_WorldToVoxelGlobal);

    return FIntVector(FMath::FloorToInt(World.X / VoxelSize),
        FMath::FloorToInt(World.Y / VoxelSize),
        FMath::FloorToInt(World.Z / VoxelSize));
}

FIntVector AChunckManager::VoxelGlobalToChunk(FIntVector VoxelLocation) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_VoxelGlobalToChunk);

    return FIntVector(FloorDivInt(VoxelLocation.X, ChunkSize),
        FloorDivInt(VoxelLocation.Y, ChunkSize),
        FloorDivInt(VoxelLocation.Z, ChunkSize));
}


int32 AChunckManager::VoxelGlobalToLocalIndex0(FIntVector G, FIntVector CC) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_VoxelGlobalToLocalIndex0);

    int32 lx = G.X - CC.X * ChunkSize;
    int32 ly = G.Y - CC.Y * ChunkSize;
    int32 lz = G.Z - CC.Z * ChunkSize;

    if (lx < 0 || lx >= ChunkSize || ly < 0  || ly >= ChunkSize || lz < 0 || lz >= ChunkSize)
    {
        UE_LOG(LogTemp, Error, TEXT("AChunckManager::VoxelGlobalToLocalIndex0 --> lx >= 0 && lx < ChunkSize || ly >= 0 && ly < ChunkSize || lz >= 0 && lz < ChunkSize"));
    }

    return lx + ly * ChunkSize + lz * ChunkSize * ChunkSize;
}


bool AChunckManager::SetVoxelAtNoLock(const FIntVector& VoxelGlobal,
    const FVoxelDataStructure& NewValue,
    FIntVector& OutChunkCoord,
    int32& OutChunkLOD)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_SetVoxelAtNoLock);

    OutChunkCoord = VoxelGlobalToChunk(VoxelGlobal);
    OutChunkLOD = INDEX_NONE;
    const FIntVector CC = OutChunkCoord;
    FChunckDataStructure* D = VoxelWorld->Chuncks.Find(CC);
    if (!D || D->Voxels.Num() == 0) return false;
    if (NewValue.Material.Id == 0) D->bIsAllSolid = false;

    const int32 LOD = D->LOD;

    if (!IsValid(VoxelWorld)) return false;

    const int32      Idx0 = VoxelGlobalToLocalIndex0(VoxelGlobal, CC);

    // --- 1) Couche d'edition : la verite persistante ---------------------

    const float Height = TerrainGenerator.ComputeHeight((float)VoxelGlobal.X, (float)VoxelGlobal.Y);
    const float Province = TerrainGenerator.ComputeProvince((float)VoxelGlobal.X, (float)VoxelGlobal.Y);
    const uint8 BaseId = TerrainGenerator.MaterialIdAt(Province, Height, VoxelGlobal.X, VoxelGlobal.Y, VoxelGlobal.Z, LOD);
    if (NewValue.Material.Id == BaseId)
    {
        // Retour a l'etat d'origine : on SUPPRIME l'exception au lieu d'en
        // stocker une. C'est ce qui empeche la couche de gonfler a l'infini
        // quand un joueur creuse puis rebouche.
        if (FChunkEditLayer* Layer = VoxelWorld->EditLayers.Find(CC))
        {
            Layer->Edits.Remove(Idx0);
            ++Layer->Revision;
            if (Layer->Edits.Num() == 0)
            {
                VoxelWorld->EditLayers.Remove(CC);
            }
        }
    }
    else
    {
        FChunkEditLayer& Layer = VoxelWorld->EditLayers.FindOrAdd(CC);
        Layer.Edits.Add(Idx0, NewValue);   // Add ecrase si la cle existe deja FAUDRA AJOUTER DES MODIFICATIONS AUX EXTREMITES DES SPEHERES/RAYONS POUR PASSER LE SABLE EN VERRE...
        ++Layer.Revision;
    }

    // --- 2) Chunk charge : retour immediat a l'ecran ---------------------
    
    const int32 SubSize = ChunkSize >> LOD;
    if (D->Voxels.Num() != SubSize * SubSize * SubSize) return false;

    const int32 lx = VoxelGlobal.X - CC.X * ChunkSize;
    const int32 ly = VoxelGlobal.Y - CC.Y * ChunkSize;
    const int32 lz = VoxelGlobal.Z - CC.Z * ChunkSize;

    // A LOD2, seuls les voxels d'indice multiple de 4 existent dans le tableau.
    // Les autres n'ont AUCUNE representation a ce LOD : l'edition est stockee,
    // elle apparaitra quand le chunk repassera a une resolution assez fine.
    const int32 Step = 1 << LOD;
    if ((lx % Step) != 0 || (ly % Step) != 0 || (lz % Step) != 0) return false;

    const int32 Index = (lx >> LOD)
        + (ly >> LOD) * SubSize
        + (lz >> LOD) * SubSize * SubSize;
    if (!D->Voxels.IsValidIndex(Index)) return false;

    if (D->Voxels[Index].Material.Id == NewValue.Material.Id) return false;  // deja bon

    D->Voxels.Edit()[Index] = NewValue;
    if (NewValue.Material.Id == 0)
    {
        D->bIsAllSolid = false;
    }
    OutChunkLOD = LOD;
    return true;
}

bool AChunckManager::LaunchDesiredRebuild()
{
    if (!IsValid(VoxelWorld) || !DesiredBuilder.IsValid() || !DesiredResultQueue.IsValid()) return false;

    FDesiredRequest Req;
    for (const TPair<APawn*, FIntVector>& Src : LastPlayerChunks)
    {
        if (!IsValid(Src.Key)) continue;
        FDesiredPlayer P;
        P.Pos = Src.Key->GetActorLocation();   // lu ICI, sur le game thread
        P.Center = Src.Value;
        Req.Players.Add(P);
    }
    if (Req.Players.Num() == 0) return false;

    Req.HR = HorizontalViewDistance;
    Req.VR = VerticalViewDistance;
    Req.ChunkSize = ChunkSize;
    Req.VoxelSize = VoxelSize;
    Req.LODDistances = LODDistances;
    Req.MaxLOD = MaxLOD;
    Req.MaxSurfaceVoxel = FMath::CeilToInt(TerrainGenerator.GetMaxPossibleHeight()) + ChunkSize;
    Req.Config = TerrainConfig;
    Req.SortCenter = Req.Players[0].Center;
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        if (APawn* Pawn = PC->GetPawn())
            Req.SortCenter = GetPlayerChunck(Pawn->GetActorLocation());
    {
        FScopeLock Lock(&VoxelWorld->ChunckMutex);
        Req.Edited.Reserve(VoxelWorld->EditLayers.Num());
        for (const auto& E : VoxelWorld->EditLayers) Req.Edited.Add(E.Key);
    }

    bDesiredInFlight = true;
    Async(EAsyncExecution::ThreadPool,
        [Builder = DesiredBuilder, Queue = DesiredResultQueue, Req = MoveTemp(Req)]() mutable
        {
            FDesiredResult Res;
            Builder->Compute(Req, Res);
            Queue->Enqueue(MoveTemp(Res));
        });
    return true;
}

// GAME THREAD. Au plus un resultat en vol.
void AChunckManager::ProcessDesiredResults()
{
    if (!DesiredResultQueue.IsValid()) return;

    FDesiredResult Res;
    if (!DesiredResultQueue->Dequeue(Res)) return;
    bDesiredInFlight = false;
    if (!Res.bValid) return;

    for (const TPair<FIntPoint, FColumnBounds>& C : Res.NewColumns)
    {
        ColumnBoundsCache.Add(C.Key, C.Value);   // garde le mode ombre SHADOW vivant
    }

    const double ApplyStart = FPlatformTime::Seconds();
    ApplyDesiredSet(Res);
    const double ApplyMs = (FPlatformTime::Seconds() - ApplyStart) * 1000.0;

    const double Now = FPlatformTime::Seconds();
    if (Now - LastDesiredLogTime > 5.0)
    {
        LastDesiredLogTime = Now;
        UE_LOG(LogTemp, Display,
            TEXT("DESIRED | gardes=%d | air retires=%d | colonnes nouvelles=%d (cache=%d) | calcul worker %.1f ms | application GT %.1f ms"),
            Res.Desired.Num(), Res.CulledAir, Res.NewColumns.Num(), Res.ColumnCacheSize, Res.ComputeMs, ApplyMs);
        UE_LOG(LogTemp, Display, TEXT("SHADOW | plein predit=%lld | erreurs LOD0=%lld LOD1=%lld LOD2=%lld LOD3=%lld"),
            ShadowPredicted, ShadowMiss[0], ShadowMiss[1], ShadowMiss[2], ShadowMiss[3]);
    }
}

// GAME THREAD. Memes regles que l'ancien BuildStreamingQueues, sans copier les 150 000 chunks existants.
void AChunckManager::ApplyDesiredSet(const FDesiredResult& Res)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ApplyDesiredSet);
    if (!IsValid(VoxelWorld)) return;

    TArray<FIntVector> NoLongerDesired;
    {
        FScopeLock Lock(&VoxelWorld->ChunckMutex);

        // 1) Desire : deja trie par distance -> spawn dans l'ordre.
        for (const FIntVector& Coord : Res.SortedCoords)
        {
            const int32 WantLOD = Res.Desired.FindChecked(Coord);
            if (const FChunckDataStructure* D = VoxelWorld->Chuncks.Find(Coord))
            {
                if (D->LOD != WantLOD && !PendingTransitionSet.Contains(Coord))
                {
                    PendingTransitionQueue.Enqueue(Coord);
                    PendingTransitionSet.Add(Coord);
                }
            }
            else if (!PendingSpawnSet.Contains(Coord))
            {
                PendingSpawnQueue.Enqueue(Coord);
                PendingSpawnSet.Add(Coord);
            }
            PendingUnloadSet.Remove(Coord);   // redevenu desire -> annule un unload en attente
        }

        // 2) Existant - desire.
        for (const TPair<FIntVector, FChunckDataStructure>& P : VoxelWorld->Chuncks)
        {
            if (!Res.Desired.Contains(P.Key)) NoLongerDesired.Add(P.Key);
        }
    }

    for (const FIntVector& Coord : NoLongerDesired)
    {
        if (!PendingUnloadSet.Contains(Coord))
        {
            PendingUnloadQueue.Enqueue(Coord);
            PendingUnloadSet.Add(Coord);
        }
        PendingSpawnSet.Remove(Coord);
    }
}

void AChunckManager::SetVoxelAt(const FIntVector& VoxelGlobal, const FVoxelDataStructure& NewValue)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_SetVoxelAt);

    if (!IsValid(VoxelWorld)) return;

    FIntVector CC;
    int32      LOD = INDEX_NONE;
    bool       bTouched = false;

    {
        FScopeLock Lock(&VoxelWorld->ChunckMutex);
        bTouched = SetVoxelAtNoLock(VoxelGlobal, NewValue, CC, LOD);
    }   // <-- verrou relache AVANT le dispatch : RegisterDirtyChunk le reprendrait.

    if (bTouched)
    {
        TMap<FIntVector, int32> Touched;
        Touched.Add(CC, LOD);
        //NathanDebug(TEXT("Chunk touché par setVoxel : X : %d, Y : %d, Z : %d au LOD %d"), CC.X, CC.Y, CC.Z, LOD);
        DispatchChunkRebuilds(Touched);
    }
}

void AChunckManager::DispatchChunkRebuilds(const TMap<FIntVector, int32>& TouchedChunks)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_DispatchChunkRebuilds);

    if (!IsValid(VoxelWorld) || TouchedChunks.Num() == 0) return;

    // Le mesher lit une coque de padding d'un voxel chez ses 6 voisins.
    // Modifier une face invalide donc aussi le mesh du voisin de ce cote.
    //static const FIntVector Dirs[6] =
    //{ {-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1} };

    TMap<FIntVector, int32> ToRebuild = TouchedChunks;

    {
        FScopeLock Lock(&VoxelWorld->ChunckMutex);
        for (const TPair<FIntVector, int32>& P : TouchedChunks)
        {
            //NathanDebug(TEXT("TouchedChunks : X : %d, Y : %d, Z : %d au LOD %d"), P.Key.X, P.Key.Y, P.Key.Z, P.Value);
            for (const FIntVector& Dir : Dirs)
            {
                const FIntVector NC = P.Key + Dir;
                if (ToRebuild.Contains(NC)) continue;

                // Le LOD du voisin peut differer : on le lit, on ne le suppose pas.
                if (const FChunckDataStructure* N = VoxelWorld->Chuncks.Find(NC))
                {
                    ToRebuild.Add(NC, N->LOD);
                }
            }
        }
    }

    for (const TPair<FIntVector, int32>& P : ToRebuild)
    {

        if (P.Value == 0) RegisterDirtyChunk(P.Key);
        else              RequestClusterRebuild(GetClusterCoord(P.Key, P.Value), P.Value);
    }
}

void AChunckManager::SetVoxel(int32 gx, int32 gy, int32 gz, int32 MaterialId)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_SetVoxel);

    FVoxelDataStructure V;
    V.Material.Id = (uint8)MaterialId;
    SetVoxelAt(FIntVector(gx, gy, gz), V);

}

void AChunckManager::TestCoords()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_TestCoords);

    const FIntVector Samples[] = {
        {   0,   0,   0 }, {  127, 127, 127 }, {  128, 128, 128 },
        {  -1,  -1,  -1 }, {   -5,  -5,  -5 }, { -128,-128,-128 },
        {-129,-129,-129 }, { -300, 450,-777 }
    };

    for (const FIntVector& G : Samples)
    {
        const FIntVector CC = VoxelGlobalToChunk(G);
        const int32      Idx0 = VoxelGlobalToLocalIndex0(G, CC);
        const int32 lx = G.X - CC.X * ChunkSize;
        const int32 ly = G.Y - CC.Y * ChunkSize;
        const int32 lz = G.Z - CC.Z * ChunkSize;

    }
}

bool AChunckManager::ApplyBrushOp(TArray<FVoxelDataStructure>& Voxels,
    FIntVector ChunkCoord, int32 LOD, int32 ChunkSize,
    const FVoxelBrushOp& Op)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ApplyBrushOp);

    bool bModified = false;
    int32 Step = 1 << LOD; 
    int32 SubSize = ChunkSize >> LOD;
    if (Voxels.Num() != (SubSize * SubSize * SubSize))
    {
        return false;
    }
    int32 OriginX = ChunkCoord.X * ChunkSize;
    int32 OriginY = ChunkCoord.Y * ChunkSize;
    int32 OriginZ = ChunkCoord.Z * ChunkSize;
    FIntVector Center = Op.CenterVoxel;
    FIntVector BoxMin(Center.X - Op.RadiusVoxels, Center.Y - Op.RadiusVoxels, Center.Z - Op.RadiusVoxels);
    FIntVector BoxMax(Center.X + Op.RadiusVoxels, Center.Y + Op.RadiusVoxels, Center.Z + Op.RadiusVoxels);
    int32 IxMin = FMath::Max(BoxMin.X - OriginX, 0) >> LOD;
    int32 IxMax = FMath::Min(BoxMax.X - OriginX, ChunkSize - 1) >> LOD;
    int32 IyMin = FMath::Max(BoxMin.Y - OriginY, 0) >> LOD;
    int32 IyMax = FMath::Min(BoxMax.Y - OriginY, ChunkSize - 1) >> LOD;
    int32 IzMin = FMath::Max(BoxMin.Z - OriginZ, 0) >> LOD;
    int32 IzMax = FMath::Min(BoxMax.Z - OriginZ, ChunkSize - 1) >> LOD;
    if (IxMin > IxMax || IyMin > IyMax || IzMin > IzMax)
    {
        return false;
    }

    for (int32 z = IzMin; z <= IzMax; ++z)
    {
        for (int32 y = IyMin; y <= IyMax; ++y)
        {
            for (int32 x = IxMin; x <= IxMax; ++x)
            {
                int64 wx = OriginX + (x << LOD);
                int64 wy = OriginY + (y << LOD);
                int64 wz = OriginZ + (z << LOD);
                int64 dx = wx - Center.X;
                int64 dy = wy - Center.Y;
                int64 dz = wz - Center.Z;
                if (dx * dx + dy * dy + dz * dz > int64(Op.RadiusVoxels) * int64(Op.RadiusVoxels))
                {
                    continue;
                }
                int32 Index = x + y * SubSize + z * SubSize * SubSize;
                if (!Voxels.IsValidIndex(Index)) continue;
                if (Voxels[Index].Material.Id == Op.MaterialId)
                {
                    continue;
                }
                Voxels[Index].Material.Id = Op.MaterialId;
                bModified = true;
            }
        }
    }
    return bModified;

}

void AChunckManager::TelemetryStamp(const FIntVector& Coord,
    double FChunkJobTelemetry::* Field,
    double Value)
{
    if (!bJobTelemetry) return;
    FScopeLock Lock(&TelemetryMutex);

    // Garde-fou : sans purge, la map gonfle indéfiniment en 40x10.
    if (JobTelemetryMap.Num() > 50000)
    {
        UE_LOG(LogTemp, Error, TEXT("TELEMETRY : purge (%d entrees)"), JobTelemetryMap.Num());
        JobTelemetryMap.Empty();
    }

    JobTelemetryMap.FindOrAdd(Coord).*Field = Value;
}

void AChunckManager::TelemetryReportAndClear(const FIntVector& Coord, int32 LOD)
{
    FChunkJobTelemetry T;
    {
        FScopeLock Lock(&TelemetryMutex);
        const FChunkJobTelemetry* Found = JobTelemetryMap.Find(Coord);
        if (!Found) return;
        T = *Found;
        JobTelemetryMap.Remove(Coord);   // entree consommee : pas de fuite
    }

    // Chaine incomplete (chunk arrive par un chemin non instrumente) : rien a dire.
    if (T.TEnqueue <= 0.0 || T.TApplied <= 0.0) return;

    const double Total = T.TApplied - T.TEnqueue;

    // Seuil : seuls les cas pathologiques nous interessent. Loguer chaque chunk
    // saturerait l'I/O et fausserait la mesure elle-meme.
    if (Total < TelemetryAlertThresholdSeconds) return;
    /*
    UE_LOG(LogTemp, Warning,
        TEXT("CHUNK (%d,%d,%d) : file=%.0fms gen=%.0fms attente-mesh=%.0fms mesh=%.0fms apply=%.0fms | TOTAL=%.0fms"),
        Coord.X, Coord.Y, Coord.Z,
        (T.TDequeue - T.TEnqueue) * 1000.0,
        (T.TGenerated - T.TDequeue) * 1000.0,
        (T.TMeshDispatched - T.TGenerated) * 1000.0,
        (T.TMeshed - T.TMeshDispatched) * 1000.0,
        (T.TApplied - T.TMeshed) * 1000.0,
        Total * 1000.0)
        */
}

void AChunckManager::MeshTrees(float Radius)
{
    UWorld* World = GetWorld();
    if (!World) return;

    APlayerController* PC = World->GetFirstPlayerController();
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!IsValid(Pawn))
    {
        UE_LOG(LogTemp, Error, TEXT("TreeDebugScatter : pas de pawn"));
        return;
    }
    TObjectPtr<UInstancedStaticMeshComponent> TreeISM;

    if (!TreeISM)
    {
        TreeISM = NewObject<UInstancedStaticMeshComponent>(this);
        TreeISM->SetMobility(EComponentMobility::Movable);
        TreeISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        TreeISM->SetStaticMesh(TreeMesh);
        TreeISM->RegisterComponent();
        TreeISM->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
    }
    TreeISM->ClearInstances();

    const FVector P = Pawn->GetActorLocation();

    FTreeScatterParams SParams;
    SParams.WorldSeed = TreeWorldSeed;
    SParams.CellSize = TreeCellSize;
    SParams.JitterRatio = TreeJitterRatio;
    SParams.MinDensity = TreeMinDensity;
    SParams.ForestWaveLength = TreeForestWeveLength;
    SParams.GlobalDensityScale = TreeGlobalDensityScale;
    SParams.VoxelSize = VoxelSize;


    FTreeScatterer Scatterer;
    Scatterer.SetParams(SParams);

    const float R = FMath::Max(1.0f, Radius) * 100.0f;   // m -> unites

    const FBox Bounds(FVector(P.X - R, P.Y - R, 0.0),
        FVector(P.X + R, P.Y + R, 0.0));
        
    TArray<FVector> Positions;
    Scatterer.EnumerateInBounds(Bounds, Positions, TerrainGenerator);

    const FTransform CompToWorld = TreeISM->GetComponentTransform();
    TArray<FTransform> Transforms;

    Transforms.Reserve(Positions.Num());
    const uint32 Seed = (uint32)TreeWorldSeed;
    for (const FVector& Pos : Positions)
    {
        const int32 cx = FMath::FloorToInt(Pos.X / TreeCellSize);
        const int32 cy = FMath::FloorToInt(Pos.Y / TreeCellSize);

        const uint32 H4 = HashMix(HashMix(HashMix(HashCell2D(cx, cy, Seed))));
        const uint32 H5 = HashMix(H4);

        const float Scale = HashToRange(H4, TreeScaleMin, TreeScaleMax);
        const float Yaw = HashToFloat01(H5) * 360.0f;

        const FTransform WorldTransform(FRotator(0.0f, Yaw, 0.0f), Pos, FVector(Scale));
        Transforms.Add(WorldTransform.GetRelativeTransform(CompToWorld));
    }

    if (Transforms.Num() > 0)
    {
        TreeISM->AddInstances(Transforms, /*bReturnIndices*/ false,
            /*bWorldSpace*/ false, /*bUpdateNavigation*/ false);
    }

    //UE_LOG(LogTemp, Warning, TEXT("MESH TREES R=%.0fm | %d arbres | mesh=%s"), Radius, Transforms.Num(), *TreeMesh->GetName());
}
void AChunckManager::TreeDebugScatter(float RadiusMeters)
{
    UWorld* World = GetWorld();
    if (!World) return;

    APlayerController* PC = World->GetFirstPlayerController();
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!IsValid(Pawn))
    {
        UE_LOG(LogTemp, Error, TEXT("TreeDebugScatter : pas de pawn"));
        return;
    }

    const FVector P = Pawn->GetActorLocation();
    const float   R = FMath::Max(1.0f, RadiusMeters) * 100.0f;   // m -> unites

    FTreeScatterParams SParams;
    SParams.WorldSeed = TreeWorldSeed;
    SParams.CellSize = TreeCellSize;
    SParams.JitterRatio = TreeJitterRatio;
    SParams.MinDensity = TreeMinDensity;
    SParams.ForestWaveLength = TreeForestWeveLength;
    SParams.GlobalDensityScale = TreeGlobalDensityScale;
    SParams.VoxelSize = VoxelSize;


    FTreeScatterer Scatterer;
    Scatterer.SetParams(SParams);

    const FBox Bounds(FVector(P.X - R, P.Y - R, 0.0),
        FVector(P.X + R, P.Y + R, 0.0));

    TArray<FVector> Positions;
    Scatterer.EnumerateInBounds(Bounds, Positions, TerrainGenerator);

    // ETAPE 2 : Z fixe, a hauteur du joueur. Rien a voir avec le sol :
    // c'est la repartition XY qu'on valide ici, rien d'autre.
    const double DebugZ = P.Z;

    // Cercle de reference : montre l'emprise reelle du carre demande.
    DrawDebugCircle(World, FVector(P.X, P.Y, P.Z), R, 64,
        FColor::Yellow, false, 30.0f, 0, 10.0f,
        FVector(1, 0, 0), FVector(0, 1, 0), false);

    for (const FVector& Pos : Positions)
    {
        DrawDebugSphere(World, FVector(Pos.X, Pos.Y, Pos.Z),
            100.0f, 8, FColor::Green, false, 30.0f, 0, 5.0f);
    }

    const int32 CellsPerAxis = FMath::FloorToInt((2.0f * R) / TreeCellSize) + 1;
    const int32 CellsTotal = CellsPerAxis * CellsPerAxis;
    UE_LOG(LogTemp, Warning,TEXT("TREE SCATTER R=%.0fm | cell=%.0f jitter=%.2f wave=%.0f scale=%.2f | %d/%d retenus (%.0f%%) | joueur (%.0f, %.0f) densite ici=%.2f"),
        RadiusMeters, TreeCellSize, TreeJitterRatio,
        TreeForestWeveLength, TreeGlobalDensityScale,
        Positions.Num(), CellsTotal,
        CellsTotal > 0 ? 100.0f * Positions.Num() / CellsTotal : 0.0f,
        P.X, P.Y,
        Scatterer.ComputeForestDensity((float)P.X, (float)P.Y));
}

void AChunckManager::MeasureCaveSlope(int32 Samples)
{
    FRandomStream Rng(12345);
    float MaxAxisDelta = 0.0f;

    for (int32 i = 0; i < FMath::Max(1, Samples); ++i)
    {
        const float X = Rng.FRandRange(-500000.f, 500000.f);
        const float Y = Rng.FRandRange(-500000.f, 500000.f);
        const float Z = Rng.FRandRange(-5000.f, 5000.f);

        const float N0 = TerrainGenerator.CaveNoiseAt(X, Y, Z);
        MaxAxisDelta = FMath::Max(MaxAxisDelta, FMath::Abs(TerrainGenerator.CaveNoiseAt(X + 1.f, Y, Z) - N0));
        MaxAxisDelta = FMath::Max(MaxAxisDelta, FMath::Abs(TerrainGenerator.CaveNoiseAt(X, Y + 1.f, Z) - N0));
        MaxAxisDelta = FMath::Max(MaxAxisDelta, FMath::Abs(TerrainGenerator.CaveNoiseAt(X, Y, Z + 1.f) - N0));
    }

    const float Suggested = 2.0f * FMath::Sqrt(3.0f) * MaxAxisDelta;
    UE_LOG(LogTemp, Display,
        TEXT("CAVE SLOPE | %d echantillons | variation max par voxel et par axe=%.6f | CaveLipschitz suggere=%.6f"),
        Samples, MaxAxisDelta, Suggested);
}



void AChunckManager::PopulateChunkTree(FIntVector Coord)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_PopulateChunkTree);

    UWorld* World = GetWorld();
    if (!World) return;

    APlayerController* PC = World->GetFirstPlayerController();
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!IsValid(Pawn))
    {
        UE_LOG(LogTemp, Error, TEXT("TreeDebugScatter : pas de pawn"));
        return;
    }
    
    UInstancedStaticMeshComponent* TreeISM = nullptr;

    if (TObjectPtr<UInstancedStaticMeshComponent>* Found = TreeISMPerChunk.Find(Coord))
    {
        TreeISM = Found->Get();
    }
    if (!TreeISM)
    {
        TreeISM = NewObject<UInstancedStaticMeshComponent>(this);
        if (!TreeISM)
        {
            UE_LOG(LogTemp, Error, TEXT("TreeDebugScatter : pas de pawn"));

        }
        TreeISM->SetMobility(EComponentMobility::Movable);
        TreeISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        TreeISM->SetStaticMesh(TreeMesh);
        TreeISM->RegisterComponent();
        TreeISM->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

        TreeISMPerChunk.Add(Coord, TreeISM);
    }
    TreeISM->ClearInstances();

    const FVector P = Pawn->GetActorLocation();

    FTreeScatterParams SParams;
    SParams.WorldSeed = TreeWorldSeed;
    SParams.CellSize = TreeCellSize;
    SParams.JitterRatio = TreeJitterRatio;
    SParams.MinDensity = TreeMinDensity;
    SParams.ForestWaveLength = TreeForestWeveLength;
    SParams.GlobalDensityScale = TreeGlobalDensityScale;
    SParams.VoxelSize = VoxelSize;


    FTreeScatterer Scatterer;
    Scatterer.SetParams(SParams);
    
    const float R = FMath::Max(1.0f, 12.5) * 100.0f;   // m -> unites

    const FBox Bounds(FVector(P.X - R, P.Y - R, 0.0),
        FVector(P.X + R, P.Y + R, 0.0));

    TArray<FVector> Positions;
    Scatterer.EnumerateInBounds(Bounds, Positions, TerrainGenerator);

    const FTransform CompToWorld = TreeISM->GetComponentTransform();
    TArray<FTransform> Transforms;

    Transforms.Reserve(Positions.Num());
    const uint32 Seed = (uint32)TreeWorldSeed;
    for (const FVector& Pos : Positions)
    {
        const int32 cx = FMath::FloorToInt(Pos.X / TreeCellSize);
        const int32 cy = FMath::FloorToInt(Pos.Y / TreeCellSize);

        const uint32 H4 = HashMix(HashMix(HashMix(HashCell2D(cx, cy, Seed))));
        const uint32 H5 = HashMix(H4);

        const float Scale = HashToRange(H4, TreeScaleMin, TreeScaleMax);
        const float Yaw = HashToFloat01(H5) * 360.0f;

        const FTransform WorldTransform(FRotator(0.0f, Yaw, 0.0f), Pos, FVector(Scale));
        Transforms.Add(WorldTransform.GetRelativeTransform(CompToWorld));
    }

    if (Transforms.Num() > 0)
    {
        if (!ClusterPoolTreeTier0.Find(Coord))
        {
        }
        else
        {
            ClusterPoolTreeTier0.Add(Coord, TreeISM);
            UE_LOG(LogTemp, Warning, TEXT("Entrées dans clusterPoolTreeTier0 : %d"), ClusterPoolTreeTier0.Num());
        }
        TreeISM->AddInstances(Transforms, /*bReturnIndices*/ false,
            /*bWorldSpace*/ false, /*bUpdateNavigation*/ false);
    }
}

void AChunckManager::GotoPeak()
{
    APawn* Pawn = GetWorld()->GetFirstPlayerController()->GetPawn();
    if (!Pawn) return;
    GetWorld()->GetTimerManager().ClearTimer(SafeSpawnTimer);   // sinon il te ramène

    const FVector Target(60137.0 * VoxelSize, 34293.0 * VoxelSize,
        (TerrainGenerator.ComputeHeight(60137.f, 34293.f) + 200.f) * VoxelSize);
    Pawn->SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
    UE_LOG(LogTemp, Warning, TEXT("GOTOPEAK : H=%.0f vx"), TerrainGenerator.ComputeHeight(60137.f, 34293.f));
}

bool AChunckManager::IsSolidWorld(const FIntVector& VoxelGlobal, uint8& OutMaterialId)
{
    OutMaterialId = 0;

    // 1) Base procedurale. Ne depend PAS de ce qui est charge : la verite ne doit
    //    jamais dependre de la representation (sinon chunk absent = air = on tombe).
    const float Province = TerrainGenerator.ComputeProvince((float)VoxelGlobal.X, (float)VoxelGlobal.Y);
    const float Height = TerrainGenerator.ComputeHeight((float)VoxelGlobal.X, (float)VoxelGlobal.Y);
    uint8 MaterialId = TerrainGenerator.MaterialIdAt(Province, Height, VoxelGlobal.X, VoxelGlobal.Y, VoxelGlobal.Z, 0);

    if (IsValid(VoxelWorld))
    {
        const FIntVector ChunkCoord = VoxelGlobalToChunk(VoxelGlobal);
        FScopeLock Lock(&VoxelWorld->EditMutex);

        if (const FChunkEditLayer* Layer = VoxelWorld->EditLayers.Find(ChunkCoord))
        {
            // 2) Pinceaux : MEME ordre que le worker (Seq croissant, le dernier gagne).
            //    Pour un seul voxel, inutile de trier : on garde l'op de plus grand Seq qui le contient.
            //    MEME test entier que ApplyBrushOp : dx2+dy2+dz2 <= R2.
            int32 BestSeq = TNumericLimits<int32>::Lowest();
            bool bInsideAnyBrush = false;
            for (const FVoxelBrushOp& Op : Layer->BrushOps)
            {
                const int64 dx = (int64)VoxelGlobal.X - Op.CenterVoxel.X;
                const int64 dy = (int64)VoxelGlobal.Y - Op.CenterVoxel.Y;
                const int64 dz = (int64)VoxelGlobal.Z - Op.CenterVoxel.Z;
                const int64 R2 = (int64)Op.RadiusVoxels * (int64)Op.RadiusVoxels;
                if (dx * dx + dy * dy + dz * dz <= R2 && Op.Seq >= BestSeq)
                {
                    BestSeq = Op.Seq;
                    MaterialId = Op.MaterialId;
                    bInsideAnyBrush = true;
                }
            }

            // 3) Editions ponctuelles : priorite maximale, comme dans le worker.
            const int32 Idx0 = VoxelGlobalToLocalIndex0(VoxelGlobal, ChunkCoord);
            if (const FVoxelDataStructure* Edit = Layer->Edits.Find(Idx0))
            {
                MaterialId = Edit->Material.Id;
            }
        }
    }

    OutMaterialId = MaterialId;
    return MaterialId != 0;
}

float AChunckManager::GetProceduralGroundZ(const FVector& Position)
{
    int32 GX = FMath::FloorToInt(Position.X / VoxelSize);
    int32 GY = FMath::FloorToInt(Position.Y / VoxelSize);
    float H = TerrainGenerator.ComputeHeight((float)GX, (float)GY);
    return FMath::CeilToFloat(H) * VoxelSize;
}

bool AChunckManager::VoxelRaycast(const FVector& OriginCm, const FVector& DirIn, float RangeCm, FVoxelRayHit& OutHit)
{
    const FVector Dir = DirIn.GetSafeNormal();
    if (Dir.IsNearlyZero() || RangeCm <= 0.f)
    {
        OutHit.ExitReason = EVoxelRayHitExit::OutOfRange;
        return false;
    }
    const float InvVoxel = 1.0f / (float)VoxelSize;
    const FVector O = OriginCm * InvVoxel;
    const float   MaxT = RangeCm * InvVoxel;

    int32 X = FMath::FloorToInt(O.X);
    int32 Y = FMath::FloorToInt(O.Y);
    int32 Z = FMath::FloorToInt(O.Z);

    const int32 StepX = (Dir.X >= 0.f) ? 1 : -1;
    const int32 StepY = (Dir.Y >= 0.f) ? 1 : -1;
    const int32 StepZ = (Dir.Z >= 0.f) ? 1 : -1;


    const float BIG = TNumericLimits<float>::Max();
    float tDeltaX, tDeltaY, tDeltaZ;
    float tMaxX, tMaxY, tMaxZ;

    auto InitAxis = [](float DirComp, float OComp, int32 Cell,
        float& OutDelta, float& OutMax)
        {
            if (FMath::Abs(DirComp) < 1e-8f)
            {
                // Direction parallele a cet axe : aucune frontiere ne sera jamais
                // franchie. On met l'infini pratique, jamais retenu comme minimum.
                OutDelta = TNumericLimits<float>::Max();
                OutMax = TNumericLimits<float>::Max();
                return;
            }
            OutDelta = FMath::Abs(1.0f / DirComp);
            // Frontiere visee : Cell+1 si on monte, Cell si on descend.
            const float Boundary = (DirComp > 0.f) ? (float)(Cell + 1) : (float)Cell;
            OutMax = (Boundary - OComp) / DirComp;   // toujours >= 0, les signes s'annulent
        };

    InitAxis(Dir.X, O.X, X, tDeltaX, tMaxX);
    InitAxis(Dir.Y, O.Y, Y, tDeltaY, tMaxY);
    InitAxis(Dir.Z, O.Z, Z, tDeltaZ, tMaxZ);

    // --- Voxel de depart : le rayon nait-il dans la roche ? -----------------
    uint8 MatId = 0;
    if (IsSolidWorld(FIntVector(X, Y, Z), MatId))
    {
        OutHit.bHit = true;
        OutHit.VoxelCoord = FIntVector(X, Y, Z);
        OutHit.ImpactPointCm = OriginCm;
        OutHit.Normal = -Dir;          // pas d'axe franchi : normale de secours
        OutHit.DistanceCm = 0.f;
        OutHit.MaterialId = MatId;
        OutHit.ResolvedLOD = 0;
        OutHit.StepsTaken = 0;
        OutHit.ExitReason = EVoxelRayHitExit::StartedInsideSolid;
        return true;
    }

    // --- Marche -------------------------------------------------------------
    const int32 NeededSteps = FMath::CeilToInt(MaxT * 1.7320508f) + 8;
    const int32 MaxSteps = FMath::Min(NeededSteps, AbsoluteStepCeiling);
    int32 Axis = -1;          // 0=X, 1=Y, 2=Z : dernier axe franchi
    float t = 0.f;

    for (int32 Step = 1; Step <= MaxSteps; ++Step)
    {
        // Le plus petit des trois tMax designe la frontiere la plus proche.
        if (tMaxX < tMaxY)
        {
            if (tMaxX < tMaxZ) { X += StepX; t = tMaxX; tMaxX += tDeltaX; Axis = 0; }
            else { Z += StepZ; t = tMaxZ; tMaxZ += tDeltaZ; Axis = 2; }
        }
        else
        {
            if (tMaxY < tMaxZ) { Y += StepY; t = tMaxY; tMaxY += tDeltaY; Axis = 1; }
            else { Z += StepZ; t = tMaxZ; tMaxZ += tDeltaZ; Axis = 2; }
        }

        if (t > MaxT)
        {
            OutHit.StepsTaken = Step;
            OutHit.ExitReason = EVoxelRayHitExit::OutOfRange;
            return false;
        }

        MatId = 0;
        if (IsSolidWorld(FIntVector(X, Y, Z), MatId))
        {
            OutHit.bHit = true;
            OutHit.VoxelCoord = FIntVector(X, Y, Z);
            OutHit.DistanceCm = t * VoxelSize;
            OutHit.ImpactPointCm = OriginCm + Dir * OutHit.DistanceCm;

            // La normale est l'oppose du sens de parcours sur l'axe franchi.
            OutHit.Normal = (Axis == 0) ? FVector(-StepX, 0, 0)
                : (Axis == 1) ? FVector(0, -StepY, 0)
                : FVector(0, 0, -StepZ);

            OutHit.MaterialId = MatId;
            OutHit.ResolvedLOD = 0;
            OutHit.StepsTaken = Step;
            OutHit.ExitReason = EVoxelRayHitExit::Hit;
            return true;
        }
    }

    OutHit.StepsTaken = MaxSteps;
    OutHit.ExitReason = EVoxelRayHitExit::StepBudgetExhausted;
    return false;
}

FColumnBounds AChunckManager::GetColumnBounds(const FIntPoint& ColumnPoint)
{
    if (FColumnBounds* Found = ColumnBoundsCache.Find(ColumnPoint)) return *Found;
    const FColumnBounds Bounds = ComputeColumnBounds(TerrainGenerator, ColumnPoint, ChunkSize);
    ColumnBoundsCache.Add(ColumnPoint, Bounds);
    return Bounds;
}


// Preuve qu'un cluster ne peut emettre aucune face, a partir des drapeaux de chunks.
// Doit etre appelee SOUS ChunckMutex (suffixe NoLock).
bool AChunckManager::AreClusterChunksAllSolidNoLock(const FIntVector& ChunkOrigin, int32 NbChunk, int32 LOD) const
{
    bool bAnyIncluded = false;
    for (int32 dz = -1; dz <= 1; ++dz)
        for (int32 cy = -1; cy <= NbChunk; ++cy)
            for (int32 cx = -1; cx <= NbChunk; ++cx)
            {
                const bool bInFootprint = (dz == 0 && cx >= 0 && cx < NbChunk && cy >= 0 && cy < NbChunk);
                const FIntVector CC(ChunkOrigin.X + cx, ChunkOrigin.Y + cy, ChunkOrigin.Z + dz);
                const FChunckDataStructure* D = VoxelWorld->Chuncks.Find(CC);

                if (bInFootprint)
                {
                    if (!D || D->LOD != LOD) continue;   //masque : aucune face possible contre lui
                    if (!D->bIsAllSolid) return false;
                    bAnyIncluded = true;
                }
                else
                {
                    // Anneau lu par le padding : absent ou non genere = air -> pas de preuve.
                    if (!D || !D->bIsChunckGenerated || !D->bIsAllSolid) return false;
                }
            }
    return bAnyIncluded;
}

// GAME THREAD, sous ChunckMutex. Decide, et photographie le voisinage SANS copier un seul voxel.
EClusterBuildResult AChunckManager::PrepareClusterJobNoLock(FIntVector ClusterCoord, int32 LOD, FClusterJob& OutJob)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_PrepareClusterJob);

    const int32 NbChunk = GetNbChunkForLOD(LOD);
    const FIntVector ChunkOrigin(ClusterCoord.X * NbChunk, ClusterCoord.Y * NbChunk, ClusterCoord.Z);

    // 1) Disponibilite + comptage (empreinte seule : sortie rapide si pas pret).
    int32 PresentSameLOD = 0;
    for (int32 cx = 0; cx < NbChunk; ++cx)
        for (int32 cy = 0; cy < NbChunk; ++cy)
        {
            const FChunckDataStructure* D =
                VoxelWorld->Chuncks.Find(FIntVector(ChunkOrigin.X + cx, ChunkOrigin.Y + cy, ChunkOrigin.Z));
            if (!D || D->LOD != LOD) continue;
            if (!D->bIsChunckGenerated) return EClusterBuildResult::NotReady;
            ++PresentSameLOD;
        }

    // 2) Orphelin : rien de ce tier dans l'empreinte.
    if (PresentSameLOD == 0)
    {
        ++PipeTotal.ClusterOrphan;
        return EClusterBuildResult::Orphan;
    }

    // 3) Preuve rapide (code inchange, deja valide en mode ombre).
    bool bFastSaysSolid = false;
    if (AreClusterChunksAllSolidNoLock(ChunkOrigin, NbChunk, LOD))
    {
        ++PipeTotal.ClusterSolidFast;
        if (!bVerifyFastSolid)
        {
            return EClusterBuildResult::AllSolidFast;
        }
        bFastSaysSolid = true;
    }

    // 4) Photographie du voisinage : (NbChunk+2)^2 x 3 cases, copies de POINTEURS.
    const int32 NW = NbChunk + 2;
    OutJob.ClusterCoord = ClusterCoord;
    OutJob.ChunkOrigin = ChunkOrigin;
    OutJob.LOD = LOD;
    OutJob.NbChunks = NbChunk;
    OutJob.ChunkSize = ChunkSize;
    OutJob.bFastSaysSolid = bFastSaysSolid;
    OutJob.Slots.SetNum(NW * NW * 3);

    for (int32 dz = -1; dz <= 1; ++dz)
        for (int32 cy = -1; cy <= NbChunk; ++cy)
            for (int32 cx = -1; cx <= NbChunk; ++cx)
            {
                const FChunckDataStructure* D = VoxelWorld->Chuncks.Find(
                    FIntVector(ChunkOrigin.X + cx, ChunkOrigin.Y + cy, ChunkOrigin.Z + dz));
                if (!D) continue;                                   // absent = air

                FClusterSlot& S = OutJob.Slots[(cx + 1) + (cy + 1) * NW + (dz + 1) * NW * NW];
                S.bIsPresent = true;
                S.LOD = D->LOD;

                const bool bInFootprint = (dz == 0 && cx >= 0 && cx < NbChunk && cy >= 0 && cy < NbChunk);
                if (bInFootprint && D->LOD != LOD) continue;        // masque : rien a lire
                S.Voxels = D->Voxels;                               // compteur +1, AUCUN voxel copie
            }

    return EClusterBuildResult::Ready;   // "a construire sur un worker"
}

// N'IMPORTE QUEL THREAD. Statique : le compilateur interdit tout acces au manager.
// Meme logique que l'ancien BuildClusterPaddedVolume, etapes 2 a 5.
EClusterBuildResult AChunckManager::BuildClusterVolumeFromSnapshot(const FClusterJob& Job,
    TArray<FVoxelDataStructure>& OutVolume, TArray<uint8>& OutMask, int32& OutSX, int32& OutSY, int32& OutSZ)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_BuildClusterFromSnapshot);

    const int32 LOD = Job.LOD;
    const int32 NbChunk = Job.NbChunks;
    const int32 CS = Job.ChunkSize;
    const int32 StepCluster = 1 << LOD;
    const int32 SubSize = CS >> LOD;
    const int32 NW = NbChunk + 2;

    auto SlotAt = [&](int32 cx, int32 cy, int32 dz) -> const FClusterSlot&
        {
            return Job.Slots[(cx + 1) + (cy + 1) * NW + (dz + 1) * NW * NW];
        };

    // 2) Dimensions et allocation.
    OutSX = NbChunk * SubSize;
    OutSY = NbChunk * SubSize;
    OutSZ = SubSize;
    const int32 PX = OutSX + 2, PY = OutSY + 2, PZ = OutSZ + 2;
    OutVolume.Reset();
    OutVolume.SetNumZeroed(PX * PY * PZ);
    OutMask.Reset();
    OutMask.SetNumZeroed(PX * PY * PZ);

    bool bSawAir = false;
    bool bSawSolid = false;

    // 3) Interieur : un chunk d'un autre LOD (ou absent) est MASQUE.
    for (int32 cx = 0; cx < NbChunk; ++cx)
        for (int32 cy = 0; cy < NbChunk; ++cy)
        {
            const FClusterSlot& S = SlotAt(cx, cy, 0);
            const int32 baseDX = cx * SubSize;
            const int32 baseDY = cy * SubSize;

            if (!S.bIsPresent || S.LOD != LOD)
            {
                for (int32 lz = 0; lz < SubSize; ++lz)
                    for (int32 ly = 0; ly < SubSize; ++ly)
                        for (int32 lx = 0; lx < SubSize; ++lx)
                            OutMask[(baseDX + lx + 1) + (baseDY + ly + 1) * PX + (lz + 1) * PX * PY] = 1;
                continue;
            }

            const int32 SubSizeD = CS >> S.LOD;
            for (int32 lz = 0; lz < SubSize; ++lz)
                for (int32 ly = 0; ly < SubSize; ++ly)
                    for (int32 lx = 0; lx < SubSize; ++lx)
                    {
                        const int32 vidx = ((lx * StepCluster) >> S.LOD)
                            + ((ly * StepCluster) >> S.LOD) * SubSizeD
                            + ((lz * StepCluster) >> S.LOD) * SubSizeD * SubSizeD;

                        if (!S.Voxels.IsValidIndex(vidx)) { bSawAir = true; continue; }
                        const uint8 MaterialId = S.Voxels[vidx].Material.Id;
                        if (MaterialId == 0) { bSawAir = true; continue; }

                        bSawSolid = true;
                        OutVolume[(baseDX + lx + 1) + (baseDY + ly + 1) * PX + (lz + 1) * PX * PY].Material.Id = MaterialId;
                    }
        }

    // 4) Padding : chaque voisin lu a SON LOD, depuis la photographie.
    const int32 BaseGX = Job.ChunkOrigin.X * CS;
    const int32 BaseGY = Job.ChunkOrigin.Y * CS;
    const int32 BaseGZ = Job.ChunkOrigin.Z * CS;

    auto SamplePadding = [&](int32 GX, int32 GY, int32 GZ) -> uint8
        {
            const int32 CX = FloorDivInt(GX, CS);
            const int32 CY = FloorDivInt(GY, CS);
            const int32 CZ = FloorDivInt(GZ, CS);
            const int32 ix = CX - Job.ChunkOrigin.X + 1;
            const int32 iy = CY - Job.ChunkOrigin.Y + 1;
            const int32 iz = CZ - Job.ChunkOrigin.Z + 1;

            // Prouve impossible (bornes du padding). Si ca arrive, on le VOIT au lieu de lire le monde.
            if (!ensureMsgf(ix >= 0 && ix < NW && iy >= 0 && iy < NW && iz >= 0 && iz <= 2,
                TEXT("Padding hors de la photographie")))
            {
                return 0;
            }

            const FClusterSlot& ClusterSlot = Job.Slots[ix + iy * NW + iz * NW * NW];
            if (!ClusterSlot.bIsPresent || ClusterSlot.Voxels.Num() == 0) return 0;              // absent = air

            const int32 SubSizeD = CS >> ClusterSlot.LOD;
            const int32 lx = (GX - CX * CS) >> ClusterSlot.LOD;
            const int32 ly = (GY - CY * CS) >> ClusterSlot.LOD;
            const int32 lz = (GZ - CZ * CS) >> ClusterSlot.LOD;
            const int32 idx = lx + ly * SubSizeD + lz * SubSizeD * SubSizeD;
            return ClusterSlot.Voxels.IsValidIndex(idx) ? ClusterSlot.Voxels[idx].Material.Id : 0;   // index invalide = air
        };

    for (int32 pz = 0; pz < PZ; ++pz)
        for (int32 py = 0; py < PY; ++py)
            for (int32 px = 0; px < PX; ++px)
            {
                if (px != 0 && px != PX - 1 && py != 0 && py != PY - 1 && pz != 0 && pz != PZ - 1)
                    continue;
                const uint8 Id = SamplePadding(BaseGX + (px - 1) * StepCluster,
                    BaseGY + (py - 1) * StepCluster,
                    BaseGZ + (pz - 1) * StepCluster);
                if (Id != 0)
                {
                    OutVolume[px + py * PX + pz * PX * PY].Material.Id = Id;
                    bSawSolid = true;
                }
                else
                {
                    bSawAir = true;
                }
            }

    // 5) Classification.
    return !bSawAir ? EClusterBuildResult::AllSolid
        : !bSawSolid ? EClusterBuildResult::AllAir
        : EClusterBuildResult::Ready;
}

// GAME THREAD. Decide et lance ; le travail lourd part sur le pool.
EClusterDispatchResult AChunckManager::TryDispatchClusterMesh(FIntVector ClusterCoord, int32 LOD)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_TryDispatchClusterMesh);

    FClusterJob Job;
    EClusterBuildResult Pre;
    {
        FScopeLock Lock(&VoxelWorld->ChunckMutex);
        Pre = PrepareClusterJobNoLock(ClusterCoord, LOD, Job);
    }
    if (Pre == EClusterBuildResult::NotReady)
    {
        return EClusterDispatchResult::NotReady;
    }

    // Ce traitement devient LE dernier etat connu du cluster. Tout job plus ancien sera jete.
    TMap<FIntVector, int32>& VersionMap =
        (LOD == 1) ? ClusterMeshVersionTier1 :
        (LOD == 2) ? ClusterMeshVersionTier2 : ClusterMeshVersionTier3;
    const int32 MyVersion = ++VersionMap.FindOrAdd(ClusterCoord);

    // Decide sans rien construire : on applique tout de suite (inchange).
    if (Pre == EClusterBuildResult::Orphan || Pre == EClusterBuildResult::AllSolidFast)
    {
        if (Pre == EClusterBuildResult::AllSolidFast)
        {
            ++PipeTotal.ClusterSolid;
        }
        FChunckMeshData EmptyMesh;
        EmptyMesh.ChunckCoord = ClusterCoord;
        EmptyMesh.bIsEmpty = true;
        ApplyClusterVolumeMesh(ClusterCoord, LOD, MoveTemp(EmptyMesh));
        return EClusterDispatchResult::SkippedCheap;
    }

    // A construire : le worker fera volume + classification + mesh.
    Job.Version = MyVersion;
    Job.EVS = VoxelSize * (float)(1 << LOD);
    ++CurrentClusterMeshJob;   // rendu par ProcessClusterResults, et nulle part ailleurs

    Async(EAsyncExecution::ThreadPool,
        [Queue = ClusterResultQueue, Job = MoveTemp(Job)]() mutable
        {
            FClusterJobResult R;
            R.ClusterCoord = Job.ClusterCoord;
            R.LOD = Job.LOD;
            R.Version = Job.Version;
            R.bFastSaysSolid = Job.bFastSaysSolid;
            R.Mesh.ChunckCoord = Job.ClusterCoord;
            R.Mesh.bIsEmpty = true;

            TArray<FVoxelDataStructure> Volume;
            TArray<uint8> Mask;
            int32 SX = 0, SY = 0, SZ = 0;
            R.Build = AChunckManager::BuildClusterVolumeFromSnapshot(Job, Volume, Mask, SX, SY, SZ);

            // Rend les tableaux partages AU PLUS TOT : moins de copies-sur-ecriture inutiles.
            Job.Slots.Empty();

            if (R.Build == EClusterBuildResult::Ready)
            {
                AChunckManager::GenerateGreedyMeshVolume(R.Mesh, Volume, Mask, SX, SY, SZ, Job.EVS);
            }

            Queue->Enqueue(MoveTemp(R));   // la file vit tant qu'un job la tient : arret de PIE sans risque
        });

    return EClusterDispatchResult::Dispatched;
}

// GAME THREAD. Applique les resultats dans un budget de temps.
void AChunckManager::ProcessClusterResults()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ProcessClusterResults);

    if (!ClusterResultQueue.IsValid()) return;

    const double Deadline = FPlatformTime::Seconds() + ClusterApplyBudgetMs * 0.001;
    FClusterJobResult R;

    // Le temps est teste AVANT de retirer un resultat : on ne retire jamais un resultat qu'on n'appliquera pas.
    while (FPlatformTime::Seconds() < Deadline && ClusterResultQueue->Dequeue(R))
    {
        CurrentClusterMeshJob = FMath::Max(0, CurrentClusterMeshJob - 1);

        // Mode ombre de la preuve rapide : elle ne doit JAMAIS contredire la construction complete.
        if (R.bFastSaysSolid && R.Build != EClusterBuildResult::AllSolid)
        {
            ++PipeTotal.FastSolidMiss;
            if (PipeTotal.FastSolidMiss <= 20)
            {
                UE_LOG(LogTemp, Error, TEXT("FASTSOLID | cluster (%d,%d,%d) LOD%d : drapeaux = plein, volume = %s"),
                    R.ClusterCoord.X, R.ClusterCoord.Y, R.ClusterCoord.Z, R.LOD,
                    R.Build == EClusterBuildResult::AllAir ? TEXT("air") : TEXT("a mesher"));
            }
        }

        if (R.Build == EClusterBuildResult::AllSolid)    ++PipeTotal.ClusterSolid;
        else if (R.Build == EClusterBuildResult::AllAir) ++PipeTotal.ClusterAir;

        TMap<FIntVector, int32>& VMap =
            (R.LOD == 1) ? ClusterMeshVersionTier1 :
            (R.LOD == 2) ? ClusterMeshVersionTier2 : ClusterMeshVersionTier3;

        if (VMap.FindRef(R.ClusterCoord) != R.Version)
        {
            ++PipeTotal.ClusterStale;    // un job plus recent existe : celui-ci est perime
            continue;
        }

        if (R.Build == EClusterBuildResult::Ready && R.Mesh.bIsEmpty)
        {
            ++PipeTotal.ClusterEmpty;
        }
        ApplyClusterVolumeMesh(R.ClusterCoord, R.LOD, MoveTemp(R.Mesh));
        ++PipeTotal.ClusterApplied;
    }
}

// N'IMPORTE QUEL THREAD. Meme code que l'ancien BuildChunkPaddedVolumeNoLock, depuis une photographie.
void AChunckManager::BuildChunkPaddedVolumeFromSnapshot(const FChunkPadSnapshot& Snap, int32 LOD, int32 SubSize,
    int32 InChunkSize, TArray<FVoxelDataStructure>& OutVolume)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_BuildChunkPaddedFromSnapshot);

    const int32 PS = SubSize + 2;
    OutVolume.Reset();
    OutVolume.SetNumZeroed(PS * PS * PS);

    // Interieur : X contigu -> une Memcpy par ligne. Taille du centre verifiee par le game thread.
    for (int32 z = 0; z < SubSize; ++z)
        for (int32 y = 0; y < SubSize; ++y)
        {
            const int32 Src = y * SubSize + z * SubSize * SubSize;
            const int32 Dst = 1 + (y + 1) * PS + (z + 1) * PS * PS;
            FMemory::Memcpy(&OutVolume[Dst], &Snap.Center[Src], SubSize * sizeof(FVoxelDataStructure));
        }

    // Les 6 faces de padding, chacune lue a SON LOD.
    const int32 Step = 1 << LOD;
    for (int32 i = 0; i < 6; ++i)
    {
        const FIntVector& Dir = Dirs[i];
        const FVoxelBuffer& N = Snap.Neighbors[i];
        if (N.Num() == 0) continue;                                     // absent -> air

        const int32 NLOD = Snap.NeighborLOD[i];
        const int32 NSub = InChunkSize >> NLOD;
        if (N.Num() != NSub * NSub * NSub) continue;                    // incoherent -> air

        const int32 Axis = (Dir.X != 0) ? 0 : (Dir.Y != 0) ? 1 : 2;
        const int32 A1 = (Axis + 1) % 3;
        const int32 A2 = (Axis + 2) % 3;
        const int32 Sign = (Axis == 0) ? Dir.X : (Axis == 1) ? Dir.Y : Dir.Z;
        const int32 NAxisIdx = (Sign < 0) ? ((InChunkSize - Step) >> NLOD) : 0;
        const int32 PAxisIdx = (Sign < 0) ? 0 : (PS - 1);

        for (int32 b = 0; b < SubSize; ++b)
            for (int32 a = 0; a < SubSize; ++a)
            {
                int32 NI[3], PadI[3];
                NI[Axis] = NAxisIdx;
                NI[A1] = (a * Step) >> NLOD;
                NI[A2] = (b * Step) >> NLOD;
                PadI[Axis] = PAxisIdx;
                PadI[A1] = a + 1;
                PadI[A2] = b + 1;

                const int32 SrcIdx = NI[0] + NI[1] * NSub + NI[2] * NSub * NSub;
                if (!N.IsValidIndex(SrcIdx)) continue;
                OutVolume[PadI[0] + PadI[1] * PS + PadI[2] * PS * PS] = N[SrcIdx];
            }
    }
}