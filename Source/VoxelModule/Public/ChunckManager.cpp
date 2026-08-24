
#include "ChunckManager.h"
#include "FChunckDataStructure.h"
#include "GameFramework/GameModeBase.h"  
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformMisc.h"
#include "RealtimeMeshComponent.h"
#include "RealtimeMeshSimple.h"
#include "Interface/Core/RealtimeMeshBuilder.h"
#include "FChunkGenResult.h"
#include "ChunckGenWorker.h"
#include "DebugMacro.h"
#include "VoxelWorld.h"

static FORCEINLINE int32 FloorDivInt(int32 A, int32 B)
{
    const int32 Q = A / B;
    return (A % B != 0 && ((A < 0) != (B < 0))) ? Q - 1 : Q;
}
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
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;
    VoxelWorld = nullptr;
    VoxelChunckClass = AVoxelChunck::StaticClass();
    bNeedUpdate = false;
    MaxMeshJob = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
    CurrentMeshJob = 0;
    ChunkSize = 128;
    VoxelSize = 10.0f;
}

// Called when the game starts or when spawned
void AChunckManager::BeginPlay()
{
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

    NumThreads = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
    MaxClusterMeshJob = FMath::Max(2, MaxMeshJob / 4);   // budget réservé aux clusters
    MaxMeshJob = FMath::Max(2, MaxMeshJob - MaxClusterMeshJob);
    MaxGenPerFrame = FMath::Clamp(NumThreads * 8, 64, 100000);


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
    Cfg.Global.CaveFrequency = 0.038;
    Cfg.Global.CaveThreshold = 0.42;    

    FTerrainLayer Base;
    Base.Type = ETerrainLayerType::Fractal;
    Base.Wavelength = 2500.0f;
    Base.Amplitude = 600.0f;
    Base.Octaves = 5;
    Base.SeedOffset = 100;
    Base.Weight = 1;
    Cfg.Layers.Add(Base);
    FTerrainLayer Ridged;
    Ridged.Type = ETerrainLayerType::Ridged;
    Ridged.Amplitude = 800;
    Ridged.Gain = 0.6;
    Ridged.Octaves = 5;
    Ridged.SeedOffset = 200;
    Ridged.Weight = 0.001;
    Ridged.Wavelength = 25000;
    Cfg.Layers.Add(Ridged);

    FTerrainLayer Plaine;
    Plaine.Type = ETerrainLayerType::Fractal;
    Plaine.Amplitude = 5;
    Plaine.Gain = 0.6;
    Plaine.Octaves = 2;
    Plaine.SeedOffset = 205;
    Plaine.Weight = 0.01;
    Ridged.Wavelength = 25000;
    Cfg.Layers.Add(Plaine);

    TerrainGenerator.SetConfig(TerrainConfig);


    UE_LOG(LogTemp, Warning, TEXT("=== TERRAIN : Wavelength=%.1f  Amplitude=%.1f  BaseHeight=%d ==="),
        SurfaceWavelength, SurfaceAmplitude, BaseHeight);

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
        UE_LOG(LogTemp, Warning, TEXT("    x=%6.0f vx  bruit=%+.3f  hauteur=%5d vx  (%.1f m)"),
            VX, N,
            BaseHeight + FMath::FloorToInt(N * SurfaceAmplitude),
            (BaseHeight + N * SurfaceAmplitude) * VoxelSize * 0.01f);
    }

    FTimerHandle Timer;
    GetWorld()->GetTimerManager().SetTimer(Timer, [this]()
        {
            APawn* Pawn = GetWorld()->GetFirstPlayerController()->GetPawn();
            if (!Pawn) return;

            FVector Pos = Pawn->GetActorLocation();
            Pos.Z = (BaseHeight + SurfaceAmplitude) * VoxelSize + 300.0f; // 🔥 hauteur safe

            Pawn->SetActorLocation(Pos, false, nullptr, ETeleportType::TeleportPhysics);

        }, 0.5f, false);
}

void AChunckManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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
    Super::Tick(DeltaTime);
    ResolveVoxelWorldIfNeeded();
    UpdatePlayerChunkState();
    TMap<FIntVector, int32> OutChunksToKeep;
    if (ShouldRebuildVisibility())
    {
        RebuildDesiredChunkSet(OutChunksToKeep);
        BuildStreamingQueues(OutChunksToKeep);
        MarkVisibilityClean();
    }
    ProcessTransitionQueue();      // marque les transitions, ne détruit RIEN
    ProcessSpawnQueue();
    ProcessGenerationQueue();
    ProcessGenerationResults();
    ProcessLODCommits();           // ← NOUVEAU : bascule les données prêtes
    ProcessDirtyChunks();
    ProcessMeshJobs();
    ProcessPendingClusters();
    ProcessLODWatchdog();          // ← NOUVEAU : filet anti-superposition permanente
    ProcessUnloadQueue();

    /*
    UE_LOG(LogTemp, Warning, TEXT("GEN RESULTS=%d DIRTY=%d PENDING_MESH=%d T1=%d T2=%d T3=%d"),
        PendingClusterGenCount,
        DirtyChuncks.Num(),
        PendingMeshClusterCount,
        PendingClusterTier1.Num(),
        PendingClusterTier2.Num(),
        PendingClusterTier3.Num());
    UE_LOG(LogTemp, Warning,
        TEXT("CurrentMeshJob=%d/%d"),
        CurrentMeshJob,
        MaxMeshJob);
        */

}

void AChunckManager::MarkVisibilityClean()
{
    bNeedsInitialBuild = false;

    bPlayerChangedChunk = false;

    bStreamingSettingsDirty = false;

    bForceVisibilityRefresh = false;
}

int32 AChunckManager::GetLODForChunck(const FIntVector& Coord, const FVector& PlayerPos) const
{
    FVector ChunckCenter = FVector(Coord) * ChunkSize * VoxelSize
        + FVector(ChunkSize * VoxelSize * 0.5f);

    float Dist = FVector::Dist(PlayerPos, ChunckCenter);

    for (int32 i = 0; i < LODDistances.Num(); i++)
    {
        if (Dist < LODDistances[i])
        {
            //UE_LOG(LogTemp, Warning, TEXT("AChunckManager::GetLODForChunck --> ChunkCoord : X %d Y %d, Z %d, LOD : %d"), Coord.X, Coord.Y, Coord.Z, FMath::Clamp(i, 0, MaxLOD));
            return FMath::Clamp(i, 0, MaxLOD);
        }
    }
    //UE_LOG(LogTemp, Warning, TEXT("AChunckManager::GetLODForChunck --> ChunkCoord : X %d Y %d, Z %d, LOD : %d"), Coord.X, Coord.Y, Coord.Z, MaxLOD);
    return MaxLOD;
}



void AChunckManager::GetAllPlayerChunks(TSet<FIntVector>& GlobalChunksToKeep)
{
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
    float ChunkWorldSize = VoxelSize * ChunkSize;

    return FIntVector(
        FMath::FloorToInt(PlayerPos.X / ChunkWorldSize),
        FMath::FloorToInt(PlayerPos.Y / ChunkWorldSize),
        FMath::FloorToInt(PlayerPos.Z / ChunkWorldSize)
    );
}



void AChunckManager::RegisterDirtyChunk(FIntVector Coord)
{

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

void AChunckManager::GenerateTerrain(FChunckDataStructure& Data, FIntVector Coord)
{
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

float AChunckManager::GetNoise(float WorldX, float WorldY)
{
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
    SurfaceNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    SurfaceNoise.SetFrequency(1);
    CaveNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    CaveNoise.SetFrequency(CaveFrequency);
}



void AChunckManager::FillChunck(EChunkVariant Variant, FIntVector Coord, int32 LOD, int32 GenerationId)
{
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
    int32 NbChunk = GetNbChunkForLOD(LOD);


    FIntVector ClusterCoord(
        FMath::FloorToInt((float)Coord.X / NbChunk),
        FMath::FloorToInt((float)Coord.Y / NbChunk),
        Coord.Z
    );

    return ClusterCoord;
}

void AChunckManager::SpawnChunk(FIntVector Coord, int32 LOD, int32 GenerationId)
{
    if (!VoxelWorld || !VoxelChunckClass || !IsValid(VoxelWorld))
        return;

    { // garde : déjà présent → no-op (spawn uniquement sur le GT)
        FScopeLock Lock(&VoxelWorld->ChunckMutex);
        if (VoxelWorld->Chuncks.Contains(Coord)) return;
    }

    FVector Location = FVector(Coord) * ChunkSize * VoxelSize;
    FChunckDataStructure NewData;
    int32 SubSize = ChunkSize >> LOD;
    NewData.Voxels.SetNum(SubSize * SubSize * SubSize);

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

void AChunckManager::GenerateAsyncGreedyMesh(FIntVector Coord)
{
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
    TArray<FVoxelDataStructure> Padded;

    {
        FScopeLock Lock(&VoxelWorld->ChunckMutex);

        FChunckDataStructure* ChunkData = VoxelWorld->Chuncks.Find(Coord);
        if (!ChunkData) return;

        // Ce chemin ne sert QUE le LOD0. Les LOD>0 passent par TryDispatchClusterMesh.
        // (ProcessDirtyChunks filtre deja, cette garde rend l'invariant explicite.)
        if (ChunkData->LOD != 0) return;
        if (!ChunkData->bIsChunckGenerated) return;

        VoxelChunk = ChunkData->VoxelChunck.Get();
        
        
        if (!IsValid(VoxelChunk) || VoxelChunk->bIsBeingDestroyed)
        {
            VoxelChunk = nullptr;   // acteur mort : rien a debloquer
            return;
        }

        CapturedGenerationId = ChunkData->GenerationId;

        if (!BuildChunkPaddedVolumeNoLock(Coord, 0, ChunkSize, Padded))
        {
            UE_LOG(LogTemp, Error,
                   TEXT("GenerateAsyncGreedyMesh (%d,%d,%d) : volume padde non construit"),
                   Coord.X, Coord.Y, Coord.Z);
            return;
        }
        
    }

    TWeakObjectPtr<AVoxelChunck>   WeakChunk(VoxelChunk);
    TWeakObjectPtr<AChunckManager> WeakManager(this);
    const int32 SX = ChunkSize;
    const float EVS = VoxelSize;   // LOD0 -> Step = 1

    bDispatched = true;   // a partir d'ici, c'est la tache async qui rend le slot

    Async(EAsyncExecution::ThreadPool,
        [WeakManager, WeakChunk, Padded = MoveTemp(Padded), SX, EVS, Coord, CapturedGenerationId]() mutable
        {
            AChunckManager* Mgr = WeakManager.Get();
            if (!Mgr) return;

            FChunckMeshData MeshData;
            MeshData.ChunckCoord = Coord;

            // Mask vide : IsMasked() renvoie false partout (IsValidIndex echoue).
            // Aucune surcouche necessaire, c'est le comportement voulu pour un chunk seul.
            static const TArray<uint8> EmptyMask;
            Mgr->GenerateGreedyMeshVolume(MeshData, Padded, EmptyMask, SX, SX, SX, EVS);
            Mgr->TelemetryStamp(Coord, &FChunkJobTelemetry::TMeshed, FPlatformTime::Seconds());

            AsyncTask(ENamedThreads::GameThread,
                [WeakManager, WeakChunk, MeshData = MoveTemp(MeshData), Coord, CapturedGenerationId]() mutable
                {
                    AChunckManager* Manager = WeakManager.Get();

                    {
                        if (Manager) Manager->CurrentMeshJob = FMath::Max(0, Manager->CurrentMeshJob - 1);
                        if (WeakChunk.IsValid()) WeakChunk->bIsQueued = false;
                    };

                    if (!Manager || !IsValid(Manager->VoxelWorld)) return;

                    // Obsolescence : GenerationId a bouge, OU le chunk est passe en cluster
                    // pendant le vol (appliquer un mesh d'acteur le superposerait au cluster).
                    bool bStale = true;
                    {
                        FScopeLock Lock(&Manager->VoxelWorld->ChunckMutex);
                        if (const FChunckDataStructure* Data = Manager->VoxelWorld->Chuncks.Find(Coord))
                            bStale = (Data->GenerationId != CapturedGenerationId) || (Data->LOD != 0);
                    }
                    if (bStale) return;

                    if (WeakChunk.IsValid() && !WeakChunk->bIsBeingDestroyed)
                    {
                        // A l'INTERIEUR de la garde : la notification ne vaut que si
                        // le mesh a reellement ete applique.
                        WeakChunk->ApplyMesh(MoveTemp(MeshData));
                        Manager->TelemetryStamp(Coord, &FChunkJobTelemetry::TApplied, FPlatformTime::Seconds());
                        Manager->TelemetryReportAndClear(Coord, 0);
                        Manager->NotifyDisplayApplied(Coord);
                        Manager->NotifyDisplayApplied(Coord);
                    }
                });
        });
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
    static const FIntVector Dirs[6] = { {-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1} };

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

// Construit le volume sous-échantillonné + padding. Renvoie false si le cluster n'est pas prêt.
bool AChunckManager::BuildClusterPaddedVolume(FIntVector ClusterCoord, int32 LOD,
    TArray<FVoxelDataStructure>& OutVolume, TArray<uint8>& OutMask, int32& OutSX, int32& OutSY, int32& OutSZ)
{
    if (!VoxelWorld) return false;

    const int32 NbChunk = GetNbChunkForLOD(LOD);
    const int32 StepCluster = 1 << LOD;          // pas d'échantillonnage du cluster (espace-MONDE)
    const int32 SubSize = ChunkSize >> LOD;      // cellules/axe/chunk, à la résolution du cluster
    const FIntVector ChunkOrigin(ClusterCoord.X * NbChunk, ClusterCoord.Y * NbChunk, ClusterCoord.Z);

    FScopeLock Lock(&VoxelWorld->ChunckMutex);

    // 1) Readiness : un chunk PRÉSENT mais pas encore généré => on attend, il arrive.
    //    Un chunk ABSENT n'est PAS attendu : hors du disque de vision (empreinte carrée
    //    vs disque), culled par IsChunkGuaranteedEmpty, ou déchargé. Il ne viendra jamais.
    //    Exiger sa présence bloquait le cluster À VIE (pending qui monte, dispatches=0).
    for (int32 cx = 0; cx < NbChunk; ++cx)
        for (int32 cy = 0; cy < NbChunk; ++cy)
        {
            const FIntVector CC(ChunkOrigin.X + cx, ChunkOrigin.Y + cy, ChunkOrigin.Z);
            const FChunckDataStructure* D = VoxelWorld->Chuncks.Find(CC);
            if (D && D->LOD == LOD && !D->bIsChunckGenerated)
                return false;
        }

    // 2) Dimensions du volume, à la résolution du cluster (+2 pour le padding)
    OutSX = NbChunk * SubSize;
    OutSY = NbChunk * SubSize;
    OutSZ = SubSize;
    const int32 PX = OutSX + 2, PY = OutSY + 2, PZ = OutSZ + 2;
    OutVolume.Reset();
    OutVolume.SetNumZeroed(PX * PY * PZ);
    OutMask.Reset();
    OutMask.SetNumZeroed(PX * PY * PZ);


    // 3) Intérieur, chunk par chunk. Un chunk d'un autre LOD (ou absent) est EXCLU :
    //    sa géométrie est rendue ailleurs (acteur LOD0 ou cluster d'un autre tier).
    int32 IncludedChunks = 0;
    for (int32 cx = 0; cx < NbChunk; ++cx)
        for (int32 cy = 0; cy < NbChunk; ++cy)
        {
            const FIntVector CC(ChunkOrigin.X + cx, ChunkOrigin.Y + cy, ChunkOrigin.Z);
            const FChunckDataStructure* D = VoxelWorld->Chuncks.Find(CC);
            const int32 baseDX = cx * SubSize;
            const int32 baseDY = cy * SubSize;
            if (!D || D->LOD != LOD)
            {
                for (int32 lz = 0; lz < SubSize; ++lz)
                    for (int32 ly = 0; ly < SubSize; ++ly)
                        for (int32 lx = 0; lx < SubSize; ++lx)
                            OutMask[(baseDX + lx + 1) + (baseDY + ly + 1) * PX + (lz + 1) * PX * PY] = 1;
                continue;
            }
            ++IncludedChunks;

            const int32 SubSizeD = ChunkSize >> D->LOD;   // taille RÉELLE stockée de CE chunk

            for (int32 lz = 0; lz < SubSize; ++lz)
                for (int32 ly = 0; ly < SubSize; ++ly)
                    for (int32 lx = 0; lx < SubSize; ++lx)
                    {
                        // décalage-monde de la cellule dans le chunk, sur la grille du cluster
                        const int32 ox = lx * StepCluster;   // 0..ChunkSize-1
                        const int32 oy = ly * StepCluster;
                        const int32 oz = lz * StepCluster;
                        // index dans le tableau du chunk, snappé à SA grille (>> D->LOD)
                        const int32 vidx = (ox >> D->LOD)
                            + (oy >> D->LOD) * SubSizeD
                            + (oz >> D->LOD) * SubSizeD * SubSizeD;
                        if (!D->Voxels.IsValidIndex(vidx))
                            continue;
                        uint8 MaterialId = D->Voxels[vidx].Material.Id;
                        if (MaterialId == 0) continue;

                        const int32 px = baseDX + lx + 1;
                        const int32 py = baseDY + ly + 1;
                        const int32 pz = lz + 1;
                        OutVolume[(baseDX + lx + 1) + (baseDY + ly + 1) * PX + (lz + 1) * PX * PY].Material.Id = MaterialId;
                    }
        }

    // 3bis) Plus AUCUN chunk de ce tier dans l'empreinte : ce cluster n'a rien à afficher.
    //       Volume vide -> mesh vide -> SetStaticMesh(nullptr) côté apply.
    //       Sans ce court-circuit, l'étape 4 remplirait quand même la coque de padding et
    //       le mesher produirait un PLAN DE FRONTIÈRE PLEIN (fausse surface superposée).
    if (LOD == 1 && IncludedChunks > 0 && IncludedChunks < NbChunk * NbChunk)
    {
        //UE_LOG(LogTemp, Warning, TEXT("BUILD T1 (%d,%d,%d) : %d/%d chunks inclus"),
            //ClusterCoord.X, ClusterCoord.Y, ClusterCoord.Z, IncludedChunks, NbChunk * NbChunk);
    }

    if (IncludedChunks == 0)
        return true;

    // 4) Bords (padding) : échantillonnage global aux frontières -> raccord sans seam.
    //    SampleGlobalVoxelSolidNoLock lit chaque voisin à SON propre LOD.
    const int32 BaseGX = ChunkOrigin.X * ChunkSize;   // géométrie-monde : PAS divisé par Step
    const int32 BaseGY = ChunkOrigin.Y * ChunkSize;
    const int32 BaseGZ = ChunkOrigin.Z * ChunkSize;

    for (int32 pz = 0; pz < PZ; ++pz)
        for (int32 py = 0; py < PY; ++py)
            for (int32 px = 0; px < PX; ++px)
            {
                if (px != 0 && px != PX - 1 && py != 0 && py != PY - 1 && pz != 0 && pz != PZ - 1)
                    continue;
                const int32 GX = BaseGX + (px - 1) * StepCluster;
                const int32 GY = BaseGY + (py - 1) * StepCluster;
                const int32 GZ = BaseGZ + (pz - 1) * StepCluster;
                uint8 Id = SampleGlobalVoxelMaterialNoLock(GX, GY, GZ);
                if (Id != 0)
                    OutVolume[px + py * PX + pz * PX * PY].Material.Id = Id;
            }

    return true;
}

// Greedy mesh sur volume de dimensions arbitraires (données déjà downsamplées -> Step interne = 1)
void AChunckManager::GenerateGreedyMeshVolume(FChunckMeshData& OutMesh,
    const TArray<FVoxelDataStructure>& Pad, const TArray<uint8>& MaskVol, int32 SX, int32 SY, int32 SZ, float EVS, const FVector3f& OriginOffset)
{
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

bool AChunckManager::TryDispatchClusterMesh(FIntVector ClusterCoord, int32 LOD)
{
    TArray<FVoxelDataStructure> Volume;
    TArray<uint8> MaskVol;
    int32 SX = 0, SY = 0, SZ = 0;
    if (!BuildClusterPaddedVolume(ClusterCoord, LOD, Volume, MaskVol, SX, SY, SZ))
        return false;

    // Ce job devient LE dernier snapshot connu de ce cluster.
    // Tout job plus ancien encore en vol sera jeté à l'arrivée.
    TMap<FIntVector, int32>& VersionMap =
        (LOD == 1) ? ClusterMeshVersionTier1 :
        (LOD == 2) ? ClusterMeshVersionTier2 : ClusterMeshVersionTier3;
    const int32 MyVersion = ++VersionMap.FindOrAdd(ClusterCoord);

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

                    // Garde d'obsolescence (GT-only : pas de lock nécessaire).
                    TMap<FIntVector, int32>& VMap =
                        (LOD == 1) ? M->ClusterMeshVersionTier1 :
                        (LOD == 2) ? M->ClusterMeshVersionTier2 : M->ClusterMeshVersionTier3;

                    if (VMap.FindRef(ClusterCoord) == MyVersion)
                    {
                        M->ApplyClusterVolumeMesh(ClusterCoord, LOD, MoveTemp(Mesh));
                    }
                    else
                    {
                        // Log temporaire : chaque tir prouve que la course existait.
                        //UE_LOG(LogTemp, Warning, TEXT("Cluster (%d,%d,%d) T%d : mesh périmé jeté (v%d, courant v%d)"),
                            //ClusterCoord.X, ClusterCoord.Y, ClusterCoord.Z, LOD,
                            //MyVersion, VMap.FindRef(ClusterCoord));
                    }
                    M->CurrentClusterMeshJob = FMath::Max(0, M->CurrentClusterMeshJob - 1);
                });
        });
    return true;
}

void AChunckManager::ApplyClusterVolumeMesh(FIntVector ClusterCoord, int32 LOD, FChunckMeshData&& MeshData)
{
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
        if (URealtimeMeshSimple* NewMesh = RMC->InitializeRealtimeMesh<URealtimeMeshSimple>())
        {
            NewMesh->SetupMaterialSlot(0, TEXT("Voxel"), VoxelMaterial);
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
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!IsValid(Pawn) || !IsValid(VoxelWorld)) return;

    const FVector P = Pawn->GetActorLocation();
    const float SafeZ = (BaseHeight + SurfaceAmplitude) * VoxelSize + 500.f; // au-dessus du pic max

    // Chunks LOD0 pouvant contenir la surface sous le joueur (vallée -> pic).
    const FIntVector C = GetPlayerChunck(P);
    const int32 ZLow = FMath::FloorToInt((BaseHeight - SurfaceAmplitude) / (float)ChunkSize);
    const int32 ZHigh = FMath::FloorToInt((BaseHeight + SurfaceAmplitude) / (float)ChunkSize);

    bool bGroundReady = false;
    {
        FScopeLock Lock(&VoxelWorld->ChunckMutex);
        for (int32 z = ZLow; z <= ZHigh && !bGroundReady; ++z)
        {
            const FChunckDataStructure* D = VoxelWorld->Chuncks.Find(FIntVector(C.X, C.Y, z));
            // LOD0 généré + acteur présent = collision en place sous le joueur.
            if (D && D->LOD == 0 && D->bIsChunckGenerated && D->VoxelChunck.IsValid())
                bGroundReady = true;
        }
    }

    if (!bGroundReady)
    {
        // Sol pas prêt : on maintient le pawn en l'air, ni dans le vide ni dans
        // de la matière en cours d'apparition.
        if (P.Z < SafeZ - 50.f)
        {
            FVector Hold = P; Hold.Z = SafeZ;
            Pawn->SetActorLocation(Hold, false, nullptr, ETeleportType::TeleportPhysics);
        }
        return;   // on repolle au prochain tick du timer
    }

    // Sol prêt : placement final au-dessus du pic. Gravité + collision font le reste.
    FVector Safe = P; Safe.Z = SafeZ;
    Pawn->SetActorLocation(Safe, false, nullptr, ETeleportType::TeleportPhysics);
    GetWorld()->GetTimerManager().ClearTimer(SafeSpawnTimer);
}
bool AChunckManager::CompareMask(const FMask& M1, const FMask& M2) const
{
    return M1.Block == M2.Block && M1.Normal == M2.Normal;
}


bool AChunckManager::ShouldRebuildVisibility() const
{
    return bNeedsInitialBuild || bPlayerChangedChunk
        || bStreamingSettingsDirty || bForceVisibilityRefresh;
}

bool AChunckManager::ResolveVoxelWorldIfNeeded()
{
    if (IsValid(VoxelWorld)) return true;
    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVoxelWorld::StaticClass(), Found);
    if (Found.Num() > 0) VoxelWorld = Cast<AVoxelWorld>(Found[0]);
    return IsValid(VoxelWorld);
}

void AChunckManager::RebuildDesiredChunkSet(TMap <FIntVector, int32>& OutChunksToKeep)
{
    OutChunksToKeep.Reset();
    const int32 HR = FMath::Max(0, HorizontalViewDistance);
    const int32 VR = FMath::Max(0, VerticalViewDistance);
    const int32 HRSq = HR * HR;

    for (const TPair<APawn*, FIntVector>& Src : LastPlayerChunks)
    {
        if (!IsValid(Src.Key)) continue;
        const FVector PlayerPos = Src.Key->GetActorLocation();
        const FIntVector& Center = Src.Value;
        for (int32 dx = -HR; dx <= HR; ++dx)
            for (int32 dy = -HR; dy <= HR; ++dy)
            {
                if (dx * dx + dy * dy > HRSq) continue;          // disque horizontal
                for (int32 dz = -VR; dz <= VR; ++dz)
                {
                    FIntVector Coord = Center + FIntVector(dx, dy, dz);
                    const int32 LOD = GetLODForChunck(Coord, PlayerPos);
                    if (int32* Cur = OutChunksToKeep.Find(Coord))
                        *Cur = FMath::Min(*Cur, LOD);
                    else
                        OutChunksToKeep.Add(Coord, LOD);

                }
            }
    }
}

void AChunckManager::BuildStreamingQueues(const TMap <FIntVector, int32>& DesiredChunks)
{
    if (!IsValid(VoxelWorld)) return;

    // Snapshot des chunks existants UNE fois (un seul lock), pas par-coord.
    TMap <FIntVector, int32> Existing;
    {
        FScopeLock Lock(&VoxelWorld->ChunckMutex);
        Existing.Reserve(VoxelWorld->Chuncks.Num());
        for (const TPair<FIntVector, FChunckDataStructure>& P : VoxelWorld->Chuncks)
            Existing.Add(P.Key, P.Value.LOD);
    }

    // SPAWN : Desired − Existing
    for (const TPair<FIntVector, int32> P : DesiredChunks)
    {


        FIntVector Coord = P.Key;
        if (IsChunkGuaranteedEmpty(Coord)) continue;
        int32 LOD = P.Value;
        const int32* CurrentLOD = Existing.Find(Coord);
        if (!CurrentLOD)
        {
            if (!PendingSpawnSet.Contains(Coord))
            {
                PendingSpawnQueue.Enqueue(Coord);
                PendingSpawnSet.Add(Coord);
            }
        }
        else if (*CurrentLOD != LOD)
        {
            if (!PendingTransitionSet.Contains(Coord)) // ← borne la file
            {
                PendingTransitionQueue.Enqueue(Coord);
                PendingTransitionSet.Add(Coord);
            }
        }

        PendingUnloadSet.Remove(Coord);   // redevenu désiré → annule un unload en attente
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
        ++Spawned;
    }
}

void AChunckManager::ProcessGenerationQueue()
{
    if (!IsValid(VoxelWorld)) return;
    int32 Dispatched = 0;
    while (Dispatched < MaxGenPerFrame)
    {
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
        ++Dispatched;
    }
}

void AChunckManager::ProcessGenerationResults()
{
    if (!IsValid(VoxelWorld)) return;
    static const FIntVector Dirs[6] =
    { {-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1} };

    const double Deadline = FPlatformTime::Seconds() + 0.003;
    FChunkGenResult Result;
    while (ChunckGenerationResult.Dequeue(Result))
    {
        PendingClusterGenCount += 1;

        bool bAccepted = false;
        bool bSelfDirty = false;
        bool bReadyToCommit = false;                      // ← manquait
        int32 ResultLOD = 0;
        TArray<FIntVector, TInlineAllocator<6>> ExistingNeighbors;

        {
            FScopeLock Lock(&VoxelWorld->ChunckMutex);    // ← manquait

            FChunckDataStructure* Data = VoxelWorld->Chuncks.Find(Result.Coord);
            if (!Data) continue;

            if (Data->Phase == EChunkLODPhase::GeneratingData
                && Result.GenerationId == Data->PendingGenerationId)
            {
                Data->PendingVoxels = MoveTemp(Result.Voxels);
                bReadyToCommit = true;
            }
            else if (Result.GenerationId == Data->GenerationId)
            {
                Data->Voxels = MoveTemp(Result.Voxels);
                Data->bIsChunckGenerated = true;
                ResultLOD = Result.LOD;

                if (Result.LOD == 0)                      // ← le bloc qui utilise Dirs
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
        }
        DirtyChuncks.Remove(Coord);
    }
}

void AChunckManager::ProcessMeshJobs()
{
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
    if (!IsValid(VoxelWorld)) return;
    int32 Dispatched = 0;
    const int32 EffectiveClusterBudget = MaxClusterMeshJob + FMath::Max(0, MaxMeshJob - CurrentMeshJob);
    auto ProcessTier = [&](TSet<FIntVector>& Pending, int32 LOD)
        {
            if (Pending.Num() == 0) return;
            for (auto It = Pending.CreateIterator(); It; ++It)
            {
                if (Dispatched >= MaxClusterDispatchPerFrame || CurrentClusterMeshJob >= EffectiveClusterBudget) return;
                if (TryDispatchClusterMesh(*It, LOD)) { It.RemoveCurrent(); ++Dispatched; }
            }
        };
    ProcessTier(PendingClusterTier1, 1);
    ProcessTier(PendingClusterTier2, 2);
    ProcessTier(PendingClusterTier3, 3);

    // DIAGNOSTIC temporaire — une ligne par seconde.
    static double LastDiag = 0.0;
    const double Now = FPlatformTime::Seconds();
    if (Now - LastDiag > 10.0)
    {
        LastDiag = Now;
       // UE_LOG(LogTemp, Warning,
           // TEXT("CLUSTERS pending T1=%d T2=%d T3=%d | dispatches=%d | jobs chunk=%d/%d cluster=%d/%d"),
            //PendingClusterTier1.Num(), PendingClusterTier2.Num(), PendingClusterTier3.Num(),
            //Dispatched, CurrentMeshJob, MaxMeshJob, CurrentClusterMeshJob, EffectiveClusterBudget);
    }
}
bool AChunckManager::UpdatePlayerChunkState()
{
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
    if (!IsValid(VoxelWorld)) return;

    const int32 MaxUnloadPerFrame = 10;
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

void AChunckManager::ProcessLODCommits()
{
    if (!IsValid(VoxelWorld) || PendingCommitSet.Num() == 0) return;

    for (const FIntVector& Coord : PendingCommitSet.Array())
    {
        PendingCommitSet.Remove(Coord);

        int32 OldLOD = INDEX_NONE, NewLOD = INDEX_NONE;
        bool bNeedActor = false;
        {
            FScopeLock Lock(&VoxelWorld->ChunckMutex);
            FChunckDataStructure* D = VoxelWorld->Chuncks.Find(Coord);
            if (!D || D->Phase != EChunkLODPhase::GeneratingData) continue;

            OldLOD = D->LOD;
            NewLOD = D->PendingLOD;

            // Bascule atomique de la donnée
            D->Voxels = MoveTemp(D->PendingVoxels);
            D->PendingVoxels.Empty();
            D->LOD = NewLOD;
            D->GenerationId = D->PendingGenerationId;
            D->bIsChunckGenerated = true;

            // Ce qu'il faudra libérer QUAND la nouvelle représentation sera affichée
            D->ReleaseLOD = OldLOD;
            D->ReleaseClusterCoord = (OldLOD >= 1)
                ? GetClusterCoord(Coord, OldLOD) : FIntVector::ZeroValue;

            D->Phase = EChunkLODPhase::AwaitingNewMesh;
            D->PhaseEnteredTime = FPlatformTime::Seconds();

            bNeedActor = (NewLOD == 0 && !D->VoxelChunck.IsValid());
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
        if (NewLOD == 0) DirtyChuncks.Add(Coord);
        else             RequestClusterRebuild(GetClusterCoord(Coord, NewLOD), NewLOD);
    }
}

void AChunckManager::RequestClusterRebuild(FIntVector CC, int32 Tier)
{
    if (Tier < 1) return;
    TMap<FIntVector, int32>& VMap =
        (Tier == 1) ? ClusterMeshVersionTier1 :
        (Tier == 2) ? ClusterMeshVersionTier2 : ClusterMeshVersionTier3;
    ++VMap.FindOrAdd(CC);        // les jobs en vol deviennent périmés — ça reste nécessaire

    if (Tier == 1)      PendingClusterTier1.Add(CC);
    else if (Tier == 2) PendingClusterTier2.Add(CC);
    else                PendingClusterTier3.Add(CC);
}

void AChunckManager::NotifyDisplayApplied(FIntVector Coord)
{
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
        //UE_LOG(LogTemp, Warning, TEXT("LOD WATCHDOG (%d,%d,%d) : libération forcée après %.1fs"),
            //Coord.X, Coord.Y, Coord.Z, LODSwapWatchdogSeconds);
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

    //UE_LOG(LogTemp, Warning, TEXT("NukeClusters : %d composants vides (manager %s)"), Cleared, *GetName());
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
                    FChunckDataStructure* D = VoxelWorld->Chuncks.Find(CC);   // non-const : on ecrit
                    if (!D || D->Voxels.Num() == 0) continue;
                    if (ApplyBrushOp(D->Voxels, CC, D->LOD, ChunkSize, Op))
                    {
                        TouchedChunks.Add(CC, D->LOD);
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

FIntVector AChunckManager::WorldToVoxelGlobal(FVector World)
{
    return FIntVector(FMath::FloorToInt(World.X / VoxelSize),
        FMath::FloorToInt(World.Y / VoxelSize),
        FMath::FloorToInt(World.Z / VoxelSize));
}

FIntVector AChunckManager::VoxelGlobalToChunk(FIntVector VoxelLocation)
{
    return FIntVector(FloorDivInt(VoxelLocation.X, ChunkSize),
        FloorDivInt(VoxelLocation.Y, ChunkSize),
        FloorDivInt(VoxelLocation.Z, ChunkSize));
}


int32 AChunckManager::VoxelGlobalToLocalIndex0(FIntVector G, FIntVector CC)
{
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
    OutChunkCoord = VoxelGlobalToChunk(VoxelGlobal);
    OutChunkLOD = INDEX_NONE;

    if (!IsValid(VoxelWorld)) return false;

    const FIntVector CC = OutChunkCoord;
    const int32      Idx0 = VoxelGlobalToLocalIndex0(VoxelGlobal, CC);

    // --- 1) Couche d'edition : la verite persistante ---------------------

    const uint8 BaseId = TerrainGenerator.MaterialIdAt(TerrainGenerator.ComputeHeight((float)VoxelGlobal.X, (float)VoxelGlobal.Y), VoxelGlobal.X, VoxelGlobal.Y, VoxelGlobal.Z);

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
    FChunckDataStructure* D = VoxelWorld->Chuncks.Find(CC);
    if (!D || D->Voxels.Num() == 0) return false;

    const int32 LOD = D->LOD;
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

    D->Voxels[Index] = NewValue;
    OutChunkLOD = LOD;
    return true;
}

void AChunckManager::SetVoxelAt(const FIntVector& VoxelGlobal, const FVoxelDataStructure& NewValue)
{
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
    if (!IsValid(VoxelWorld) || TouchedChunks.Num() == 0) return;

    // Le mesher lit une coque de padding d'un voxel chez ses 6 voisins.
    // Modifier une face invalide donc aussi le mesh du voisin de ce cote.
    static const FIntVector Dirs[6] =
    { {-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1} };

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
    FVoxelDataStructure V;
    V.Material.Id = (uint8)MaterialId;
    SetVoxelAt(FIntVector(gx, gy, gz), V);

    //UE_LOG(LogTemp, Warning, TEXT("SetVoxel (%d,%d,%d) = %d"), gx, gy, gz, MaterialId);
}

void AChunckManager::TestCoords()
{
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

        //UE_LOG(LogTemp, Warning,
            //TEXT("G=(%5d,%5d,%5d) -> CC=(%3d,%3d,%3d) local=(%3d,%3d,%3d) idx0=%d"),
            //G.X, G.Y, G.Z, CC.X, CC.Y, CC.Z, lx, ly, lz, Idx0);
    }
}

bool AChunckManager::ApplyBrushOp(TArray<FVoxelDataStructure>& Voxels,
    FIntVector ChunkCoord, int32 LOD, int32 ChunkSize,
    const FVoxelBrushOp& Op)
{
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
}