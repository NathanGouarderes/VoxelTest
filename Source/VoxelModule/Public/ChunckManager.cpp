
#include "ChunckManager.h"
#include "FChunckDataStructure.h"
#include "GameFramework/GameModeBase.h"  
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformMisc.h"
#include "FChunkGenResult.h"
#include "ChunckGenWorker.h"
#include "VoxelWorld.h"

#include "StaticMeshAttributes.h"

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
    UProceduralMeshComponent* ClusterProceduralMeshComponent = NewObject<UProceduralMeshComponent>(this);
    ClusterProceduralMeshComponent->RegisterComponent();
    ClusterProceduralMeshComponent->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
    //ClusterProceduralMeshComponent->SetWorldLocation(ClusterWorldPos);

    NumThreads = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
    MaxClusterMeshJob = FMath::Max(2, MaxMeshJob / 4);   // budget réservé aux clusters
    MaxMeshJob = FMath::Max(2, MaxMeshJob - MaxClusterMeshJob);
    MaxGenPerFrame = FMath::Clamp(NumThreads * 8, 64, 100000);


    InitNoise();

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
            Pos.Z = (BaseHeight + SurfaceAmplitude) * VoxelSize + 500.0f; // 🔥 hauteur safe

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

    //if (!ChunckData) return;
    int32 TotalSize = ChunkSize * ChunkSize * ChunkSize;

    ChunckGenerationJobQueue.Enqueue(FChunkGenJob(Coord, Variant, this, LOD, GenerationId));
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


void AChunckManager::ApplyMeshToCluster(const TArray<FChunckMeshData>& ChunkMeshData, TMap<FIntVector, UProceduralMeshComponent*>& ClusterPool, FIntVector ClusterCoord, int32 LOD)
{
    int32 NbChunk = 0;

    switch (LOD)
    {
    case 1:
        NbChunk = 4;
        break;
    case 2:
        NbChunk = 16;
        break;
    case 3:
        NbChunk = 32;
        break;
    default:
        NbChunk = 4;
        break;
    }
    float ClusterSize = NbChunk * ChunkSize * VoxelSize;

    FIntVector ClusterOrigine = FIntVector(
        ClusterCoord.X * (int32)ClusterSize,
        ClusterCoord.Y * (int32)ClusterSize,
        ClusterCoord.Z * (ChunkSize * (int32)VoxelSize)
    );
    //UE_LOG(LogTemp, Warning, TEXT("ClusterOrigine : (%d,%d,%d)"),
     //   ClusterOrigine.X, ClusterOrigine.Y, ClusterOrigine.Z);



    TArray<FVector> MergedVertices;
    TArray<int32> MergedTriangles;
    TArray<FVector> MergedNormals;
    TArray<FVector2D> MergedUVs;
    TArray<FProcMeshTangent> MergedTangents;


    for (const FChunckMeshData& ChunkMesh : ChunkMeshData)
    {
        FIntVector OffsetWorld = FIntVector(
            ChunkMesh.ChunckCoord.X * (VoxelSize * ChunkSize),
            ChunkMesh.ChunckCoord.Y * (VoxelSize * ChunkSize),
            ChunkMesh.ChunckCoord.Z * (VoxelSize * ChunkSize)
        );
        FIntVector OffsetRelatif = OffsetWorld - ClusterOrigine;

        int32 VertexOffset = MergedVertices.Num();

        for (const FVector& Vertex : ChunkMesh.Vertices)
        {
            MergedVertices.Add(Vertex + FVector(OffsetRelatif));
        }

        for (const int32& Triangle : ChunkMesh.Triangles)
        {
            MergedTriangles.Add(Triangle + VertexOffset);
        }
        for (const FVector& Normal : ChunkMesh.Normals)
        {
            MergedNormals.Add(Normal);
        }
        for (const FVector2D& Uv : ChunkMesh.UVs)
        {
            MergedUVs.Add(Uv);
        }
        for (const FProcMeshTangent& Tangent : ChunkMesh.Tangents)
        {
            MergedTangents.Add(Tangent);
        }
    }
    UProceduralMeshComponent* PMC = ClusterPool.FindOrAdd(ClusterCoord);
    if (!PMC)
    {
        PMC = NewObject<UProceduralMeshComponent>(this);
        PMC->RegisterComponent();
        PMC->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
        PMC->SetWorldLocation(FVector(ClusterOrigine));
        ClusterPool[ClusterCoord] = PMC;
    }
    PMC->CreateMeshSection(
        0,
        MergedVertices,
        MergedTriangles,
        MergedNormals,
        MergedUVs,
        TArray<FColor>(),
        MergedTangents,
        false
    );

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

/*
int32 AChunckManager::CalculChunkLODBeforeSpawn(FIntVector Coord)
{
    APawn* Pawn = GetWorld()->GetFirstPlayerController()->GetPawn();
    if (!Pawn)
    {
        return 0;
    }
    FVector PlayerPos = Pawn->GetActorLocation();
    FChunckDataStructure* ChunkData = VoxelWorld->Chuncks.Find(Coord);
    FVector ChunkCenter = FVector(Coord) * ChunkSize * VoxelSize + FVector(ChunkSize * VoxelSize * 0.5f);
    float Dist = FVector::Dist(PlayerPos, ChunkCenter);
    int32 ChunkLOD = 0;
    for (int i = 0; i < LODDistances.Num(); i++)
    {
        if (Dist < LODDistances[i])
        {
            ChunkLOD = FMath::Clamp(i, 0, MaxLOD);
            break;
        }
        if (ChunkLOD > LODDistances[i])
        {
            ChunkLOD = MaxLOD;
        }
    }
    return ChunkLOD;
}
*/

/*void AChunckManager::UpdateVisibleChunks(const TSet<FIntVector>& ChunksToKeep)
{
    if (!VoxelWorld)
    {
        UE_LOG(LogTemp, Error, TEXT("VoxelWorld NULL"));
        return;
    }

    int32 SpawnCount = 0;
    TArray<FIntVector> SortedChunks = ChunksToKeep.Array();

    APawn* Pawn = GetWorld()->GetFirstPlayerController()->GetPawn();
    if (!Pawn) return;

    FVector PlayerPos = Pawn->GetActorLocation();

    SortedChunks.Sort([&](const FIntVector& A, const FIntVector& B)
        {
            FIntVector DeltaA = A - GetPlayerChunck(PlayerPos);
            FIntVector DeltaB = B - GetPlayerChunck(PlayerPos);
            int32 RingA = FMath::Max(FMath::Abs(DeltaA.X), FMath::Abs(DeltaA.Y));
            int32 RingB = FMath::Max(FMath::Abs(DeltaB.X), FMath::Abs(DeltaB.Y));
            if (RingA != RingB) return RingA < RingB;
            return FMath::Abs(DeltaA.Z) < FMath::Abs(DeltaB.Z);
        });

    for (const FIntVector& Coord : SortedChunks)
    {
        int32 ChunkLOD = GetLODForChunck(Coord, PlayerPos);

        if (VoxelWorld->Chuncks.Contains(Coord))
        {
            // Chunk existant — vérifier si transition LOD nécessaire
            FChunckDataStructure* ChunkData = VoxelWorld->Chuncks.Find(Coord);
            if (ChunkData->LOD != ChunkLOD)
            {
                // Détruire l'ancien acteur LOD0 si présent
                if (ChunkData->VoxelChunck.IsValid())
                {
                    ChunkData->VoxelChunck->bIsBeingDestroyed = true;
                    ChunkData->VoxelChunck->Destroy();
                }

                // Retirer de la salle d'attente LOD
                PendingLODMesh.Remove(Coord);

                // Incrémenter le GenerationId pour invalider les async en transit
                int32 NewGenerationId = ChunkData->GenerationId + 1;

                // Retirer AVANT SpawnChunk pour éviter le double spawn
                VoxelWorld->Chuncks.Remove(Coord);

                SpawnChunk(Coord, ChunkLOD, NewGenerationId);
            }
            // else : LOD correct, rien à faire
        }
        else
        {
            // Chunk nouveau — spawn normal
            if (SpawnCount >= MaxSpawnPerFrame)
                break;

            SpawnChunk(Coord, ChunkLOD, 1);
            SpawnCount++;
        }
    }

    // UNLOAD — chunks hors de la view distance
    TArray<FIntVector> ToRemove;
    for (auto& Pair : VoxelWorld->Chuncks)
    {
        if (!ChunksToKeep.Contains(Pair.Key))
            ToRemove.Add(Pair.Key);
    }

    for (const FIntVector& Coord : ToRemove)
    {
        FScopeLock Lock(&VoxelWorld->ChunckMutex);
        FChunckDataStructure* Data = VoxelWorld->Chuncks.Find(Coord);
        if (!Data) continue;

        if (Data->VoxelChunck.IsValid())
        {
            Data->VoxelChunck->bIsBeingDestroyed = true;
            Data->VoxelChunck->Destroy();
        }
        VoxelWorld->Chuncks.Remove(Coord);
    }
}*/

void AChunckManager::GenerateAsyncGreedyMesh(FIntVector Coord)
{
    if (!VoxelWorld || !IsValid(this))
    {
        CurrentMeshJob = FMath::Max(0, CurrentMeshJob - 1);
        return;
    }

    FScopeLock Lock(&VoxelWorld->ChunckMutex);

    FChunckDataStructure* ChunkData = VoxelWorld->Chuncks.Find(Coord);
    if (!ChunkData || ChunkData->Voxels.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("AChunckManager::GenerateAsyncGreedyMesh(FIntVector Coord) --> !ChunkData || ChunkData->Voxels.Num() == 0"));
        CurrentMeshJob = FMath::Max(0, CurrentMeshJob - 1);
        return;
    }

    if (ChunkData->bIsChunckGenerated == false)
    {
        UE_LOG(LogTemp, Error, TEXT("AChunckManager::GenerateAsyncGreedyMesh(FIntVector Coord) --> ChunkData->bIsChunckGenerated == false"));
        CurrentMeshJob = FMath::Max(0, CurrentMeshJob - 1);
        //PendingMeshToApply.Enqueue(ChunkData->Coord);
        return;
    }

    int32 CapturedGenerationId = ChunkData->GenerationId;
    int32 LOD = ChunkData->LOD;
    const int32 SubSize = ChunkSize >> LOD;
    AVoxelChunck* VoxelChunk = (ChunkData->LOD == 0) ? ChunkData->VoxelChunck.Get() : nullptr;
    if (ChunkData->LOD == 0 && (!IsValid(VoxelChunk) || VoxelChunk->bIsBeingDestroyed))
    {
        //UE_LOG(LogTemp, Error, TEXT("AChunckManager::GenerateAsyncGreedyMesh(FIntVector Coord) --> ChunkData->LOD == 0 && (!IsValid(VoxelChunk) || VoxelChunk->bIsBeingDestroyed)"));
        CurrentMeshJob--;
        return;

    }

    if (ChunkData->Voxels.Num() != SubSize * SubSize * SubSize)
    {
        UE_LOG(LogTemp, Error, TEXT("GenerateAsyncGreedyMesh --> (%d,%d,%d) LOD %d : Voxels.Num()=%d, attendu %d"),
            Coord.X, Coord.Y, Coord.Z, LOD, ChunkData->Voxels.Num(), SubSize * SubSize * SubSize);
        CurrentMeshJob = FMath::Max(0, CurrentMeshJob - 1);
        if (VoxelChunk) VoxelChunk->bIsQueued = false;
        return;
    }

    // On capture les données nécessaires
    const int32 PaddedSize = SubSize + 2;
    TArray<FVoxelDataStructure> PaddedVoxels;
    PaddedVoxels.SetNum(PaddedSize * PaddedSize * PaddedSize);

    // Remplissage du padded (copie du chunk + voisins)
    for (int z = 0; z < SubSize; ++z)
        for (int y = 0; y < SubSize; ++y)
            for (int x = 0; x < SubSize; ++x)
            {
                int srcIdx = x + y * SubSize + z * SubSize * SubSize;
                int dstIdx = (x + 1) + (y + 1) * PaddedSize + (z + 1) * PaddedSize * PaddedSize;
                PaddedVoxels[dstIdx] = ChunkData->Voxels[srcIdx];
            }

    // Copie des voisins (bords)
    const FIntVector Neighbors[6] = { {-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1} };
    for (const FIntVector& Dir : Neighbors)
    {
        const FChunckDataStructure* Neighbor = VoxelWorld->Chuncks.Find(Coord + Dir);
        if (!Neighbor || Neighbor->Voxels.Num() == 0)
            continue;

        for (int a = 0; a < SubSize; ++a)
        {
            for (int b = 0; b < SubSize; ++b)
            {
                int nx = (Dir.X == -1) ? (SubSize - 1) : (Dir.X == 1) ? 0 : a;
                int ny = (Dir.Y == -1) ? (SubSize - 1) : (Dir.Y == 1) ? 0 : (Dir.X != 0) ? a : b;
                int nz = (Dir.Z == -1) ? (SubSize - 1) : (Dir.Z == 1) ? 0 : (Dir.X != 0) ? b : (Dir.Y != 0) ? b : 0;

                int px = (Dir.X == -1) ? 0 : (Dir.X == 1) ? PaddedSize - 1 : a + 1;
                int py = (Dir.Y == -1) ? 0 : (Dir.Y == 1) ? PaddedSize - 1 : (Dir.X != 0) ? a + 1 : b + 1;
                int pz = (Dir.Z == -1) ? 0 : (Dir.Z == 1) ? PaddedSize - 1 : (Dir.X != 0) ? b + 1 : (Dir.Y != 0) ? b + 1 : 1;

                int srcIdx = nx + ny * SubSize + nz * SubSize * SubSize;
                int dstIdx = px + py * PaddedSize + pz * PaddedSize * PaddedSize;

                if (Neighbor->Voxels.IsValidIndex(srcIdx))
                    PaddedVoxels[dstIdx] = Neighbor->Voxels[srcIdx];
            }
        }
    }
    int32 CapturedLOD = LOD;
    TWeakObjectPtr<AVoxelChunck> WeakChunk(VoxelChunk);
    TWeakObjectPtr<AChunckManager> WeakManager(this);

    Async(EAsyncExecution::ThreadPool,
        [WeakManager, WeakChunk, PaddedVoxels = MoveTemp(PaddedVoxels), CapturedLOD, Coord, CapturedGenerationId, this]() mutable
        {
            if (!WeakManager.IsValid())
                return;

            FChunckMeshData MeshData;
            WeakManager->GenerateGreedyMesh(MeshData, PaddedVoxels, Coord, CapturedLOD);

            AsyncTask(ENamedThreads::GameThread,
                [WeakManager, WeakChunk, MeshData = MoveTemp(MeshData), CapturedLOD, Coord, CapturedGenerationId]() mutable
                {
                    AChunckManager* Manager = WeakManager.Get();
                    ON_SCOPE_EXIT
                    {
                        if (Manager) Manager->CurrentMeshJob = FMath::Max(0, Manager->CurrentMeshJob - 1);
                        if (WeakChunk.IsValid()) WeakChunk->bIsQueued = false;
                    };

                    if (!Manager || !Manager->VoxelWorld) return;

                    bool bStale = true;
                    {
                        FScopeLock Lock(&Manager->VoxelWorld->ChunckMutex);
                        if (FChunckDataStructure* Data = Manager->VoxelWorld->Chuncks.Find(Coord))
                            bStale = (Data->GenerationId != CapturedGenerationId);
                    }
                    if (bStale) return;

                    if (CapturedLOD == 0)
                    {
                        if (WeakChunk.IsValid() && !WeakChunk->bIsBeingDestroyed)
                        {
                            WeakChunk->ApplyMesh(MeshData);
                            // À l'INTÉRIEUR de la garde : la notification ne vaut que
                            // si le mesh a réellement été appliqué.
                            Manager->NotifyDisplayApplied(Coord);
                        }
                    }
                    else
                    {
                        FClusterGenResult R;
                        R.LOD = CapturedLOD;
                        R.MeshData = MeshData;
                        R.MeshData.ChunckCoord = Coord;
                        Manager->ClusterGenerationResult.Enqueue(R);
                    }
                });
        });
}

int32 AChunckManager::GetNbChunkForLOD(int32 LOD) const
{
    switch (LOD) { case 1: return 1; case 2: return 4; case 3: return 8; default: return 1; }
}

bool AChunckManager::SampleGlobalVoxelSolidNoLock(int32 GX, int32 GY, int32 GZ)
{
    const FIntVector CC(
        FMath::FloorToInt((float)GX / ChunkSize),
        FMath::FloorToInt((float)GY / ChunkSize),
        FMath::FloorToInt((float)GZ / ChunkSize));
    const FChunckDataStructure* D = VoxelWorld->Chuncks.Find(CC);
    if (!D || D->Voxels.Num() == 0) return false;

    const int32 SubSize = ChunkSize >> D->LOD;
    const int32 lx = (GX - CC.X * ChunkSize) >> D->LOD;
    const int32 ly = (GY - CC.Y * ChunkSize) >> D->LOD;
    const int32 lz = (GZ - CC.Z * ChunkSize) >> D->LOD;
    const int32 idx = lx + ly * SubSize + lz * SubSize * SubSize;
    if (!D->Voxels.IsValidIndex(idx)) return false;
    return D->Voxels[idx].Material.Id > 0;
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
                        if (!D->Voxels.IsValidIndex(vidx) || D->Voxels[vidx].Material.Id == 0)
                            continue;

                        const int32 px = baseDX + lx + 1;
                        const int32 py = baseDY + ly + 1;
                        const int32 pz = lz + 1;
                        OutVolume[(baseDX + lx + 1) + (baseDY + ly + 1) * PX + (lz + 1) * PX * PY].Material.Id = 1;
                    }
        }

    // 3bis) Plus AUCUN chunk de ce tier dans l'empreinte : ce cluster n'a rien à afficher.
    //       Volume vide -> mesh vide -> SetStaticMesh(nullptr) côté apply.
    //       Sans ce court-circuit, l'étape 4 remplirait quand même la coque de padding et
    //       le mesher produirait un PLAN DE FRONTIÈRE PLEIN (fausse surface superposée).
    if (LOD == 1 && IncludedChunks > 0 && IncludedChunks < NbChunk * NbChunk)
    {
        UE_LOG(LogTemp, Warning, TEXT("BUILD T1 (%d,%d,%d) : %d/%d chunks inclus"),
            ClusterCoord.X, ClusterCoord.Y, ClusterCoord.Z, IncludedChunks, NbChunk * NbChunk);
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
                if (SampleGlobalVoxelSolidNoLock(GX, GY, GZ))
                    OutVolume[px + py * PX + pz * PX * PY].Material.Id = 1;
            }

    return true;
}

// Greedy mesh sur volume de dimensions arbitraires (données déjà downsamplées -> Step interne = 1)
void AChunckManager::GenerateGreedyMeshVolume(FChunckMeshData& OutMesh,
    const TArray<FVoxelDataStructure>& Pad, const TArray<uint8>& MaskVol, int32 SX, int32 SY, int32 SZ, float EVS)
{
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

    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        const int32 A1 = (Axis + 1) % 3;
        const int32 A2 = (Axis + 2) % 3;
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
                    else if (a)      Mask[N++] = FMask{ 1, 1 };
                    else             Mask[N++] = FMask{ 1, -1 };
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
                        const int32 S = OutMesh.Vertices.Num();
                        OutMesh.Vertices.Add(FVector(V1[0], V1[1], V1[2]) * EVS);
                        OutMesh.Vertices.Add(FVector(V2[0], V2[1], V2[2]) * EVS);
                        OutMesh.Vertices.Add(FVector(V3[0], V3[1], V3[2]) * EVS);
                        OutMesh.Vertices.Add(FVector(V4[0], V4[1], V4[2]) * EVS);

                        OutMesh.Triangles.Add(S + 0);
                        OutMesh.Triangles.Add(S + 2 + CurrentMask.Normal);
                        OutMesh.Triangles.Add(S + 2 - CurrentMask.Normal);
                        OutMesh.Triangles.Add(S + 3);
                        OutMesh.Triangles.Add(S + 1 - CurrentMask.Normal);
                        OutMesh.Triangles.Add(S + 1 + CurrentMask.Normal);

                        OutMesh.Normals.Add(Normal); OutMesh.Normals.Add(Normal);
                        OutMesh.Normals.Add(Normal); OutMesh.Normals.Add(Normal);

                        OutMesh.UVs.Add(FVector2D(0, 0)); OutMesh.UVs.Add(FVector2D(W, 0));
                        OutMesh.UVs.Add(FVector2D(0, H)); OutMesh.UVs.Add(FVector2D(W, H));

                        for (int32 l = 0; l < H; ++l)
                            for (int32 k = 0; k < W; ++k)
                                Mask[N + k + l * Dims[A1]] = FMask{ 0, 0 };

                        i += W; N += W;
                    }
                    else { ++i; ++N; }
                }
            }
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
                        M->ApplyClusterVolumeMesh(ClusterCoord, LOD, Mesh);
                    }
                    else
                    {
                        // Log temporaire : chaque tir prouve que la course existait.
                        UE_LOG(LogTemp, Warning, TEXT("Cluster (%d,%d,%d) T%d : mesh périmé jeté (v%d, courant v%d)"),
                            ClusterCoord.X, ClusterCoord.Y, ClusterCoord.Z, LOD,
                            MyVersion, VMap.FindRef(ClusterCoord));
                    }
                    M->CurrentClusterMeshJob = FMath::Max(0, M->CurrentClusterMeshJob - 1);
                });
        });
    return true;
}

void AChunckManager::ApplyClusterVolumeMesh(FIntVector ClusterCoord, int32 LOD, const FChunckMeshData& Mesh)
{
    TMap<FIntVector, UStaticMeshComponent*>* Pool =
        (LOD == 1) ? &ClusterPoolTier1 : (LOD == 2) ? &ClusterPoolTier2 : &ClusterPoolTier3;

    const int32 NbChunk = GetNbChunkForLOD(LOD);
    const float ClusterWorldSize = (float)NbChunk * ChunkSize * VoxelSize;
    const FVector ClusterOrigin(
        ClusterCoord.X * ClusterWorldSize,
        ClusterCoord.Y * ClusterWorldSize,
        ClusterCoord.Z * (float)ChunkSize * VoxelSize);

    // RÉUTILISATION du composant existant. Sans ce Find, chaque apply empile
    // un composant orphelin toujours rendu -> mesh fantôme permanent.
    UStaticMeshComponent* SMC = nullptr;
    if (UStaticMeshComponent** Found = Pool->Find(ClusterCoord)) SMC = *Found;
    if (!SMC)
    {
        SMC = NewObject<UStaticMeshComponent>(this);
        // Mobility AVANT RegisterComponent : un composant Static déjà enregistré
        // ne peut plus être déplacé -> le cluster resterait planté à l'origine.
        SMC->SetMobility(EComponentMobility::Movable);
        SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // clusters LOD>0 = visuel seul
        SMC->bVisibleInRayTracing = false;                          // évite les reconstructions de SBT
        SMC->SetCastShadow(false);                                  // évite les invalidations VSM
        SMC->RegisterComponent();
        SMC->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
        SMC->SetWorldLocation(ClusterOrigin);
        Pool->Add(ClusterCoord, SMC);
    }

    // Les chunks de cette empreinte qui attendaient leur nouvelle représentation
    // viennent de l'obtenir : leur ancienne peut être libérée. DOIT être appelé sur
    // TOUS les chemins de sortie, mesh vide inclus.
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
            // Copie avant itération : NotifyDisplayApplied modifie ChunksAwaitingMesh.
            for (const FIntVector& C : Applied) NotifyDisplayApplied(C);
        };

    // Cluster vide -> on détache le mesh (équivalent SMC du ClearMeshSection).
    if (Mesh.Vertices.Num() == 0 || Mesh.Triangles.Num() < 3)
    {
        SMC->SetStaticMesh(nullptr);
        NotifyFootprint();
        return;
    }

    // --- Conversion FChunckMeshData -> FMeshDescription ---
    FMeshDescription MeshDescription;
    FStaticMeshAttributes Attributes(MeshDescription);
    Attributes.Register();

    TVertexAttributesRef<FVector3f>          Positions = Attributes.GetVertexPositions();
    TVertexInstanceAttributesRef<FVector3f>  Normals = Attributes.GetVertexInstanceNormals();
    TVertexInstanceAttributesRef<FVector2f>  UVs = Attributes.GetVertexInstanceUVs();
    UVs.SetNumChannels(1);

    const int32 NumVerts = Mesh.Vertices.Num();
    MeshDescription.ReserveNewVertices(NumVerts);
    MeshDescription.ReserveNewVertexInstances(NumVerts);
    MeshDescription.ReserveNewPolygons(Mesh.Triangles.Num() / 3);
    MeshDescription.ReserveNewEdges(Mesh.Triangles.Num());

    const FPolygonGroupID PolyGroup = MeshDescription.CreatePolygonGroup();

    TArray<FVertexInstanceID> InstanceIDs;
    InstanceIDs.Reserve(NumVerts);
    for (int32 i = 0; i < NumVerts; ++i)
    {
        const FVertexID VID = MeshDescription.CreateVertex();
        Positions[VID] = FVector3f(Mesh.Vertices[i]);

        const FVertexInstanceID VIID = MeshDescription.CreateVertexInstance(VID);
        // CreateQuad produit des normales de longueur VoxelSize -> normaliser.
        if (Mesh.Normals.IsValidIndex(i))
            Normals[VIID] = FVector3f(Mesh.Normals[i].GetSafeNormal());
        if (Mesh.UVs.IsValidIndex(i))
            UVs.Set(VIID, 0, FVector2f(Mesh.UVs[i]));

        InstanceIDs.Add(VIID);
    }

    for (int32 t = 0; t + 2 < Mesh.Triangles.Num(); t += 3)
    {
        const int32 I0 = Mesh.Triangles[t], I1 = Mesh.Triangles[t + 1], I2 = Mesh.Triangles[t + 2];
        if (!InstanceIDs.IsValidIndex(I0) || !InstanceIDs.IsValidIndex(I1) || !InstanceIDs.IsValidIndex(I2))
            continue;
        MeshDescription.CreateTriangle(PolyGroup, { InstanceIDs[I0], InstanceIDs[I1], InstanceIDs[I2] });
    }

    // --- Build ---
    // Réutilisation du UStaticMesh existant : sinon chaque apply en crée un neuf
    // et l'ancien devient déchet -> le compte d'UObjects explose et les passes de
    // GC deviennent des freezes de plusieurs secondes.
    UStaticMesh* StaticMesh = SMC->GetStaticMesh();
    if (!StaticMesh)
    {
        StaticMesh = NewObject<UStaticMesh>(this);
        StaticMesh->GetStaticMaterials().Add(FStaticMaterial(ClusterMaterial));
    }

    UStaticMesh::FBuildMeshDescriptionsParams Params;
    Params.bBuildSimpleCollision = false;
    Params.bFastBuild = true;
    Params.bMarkPackageDirty = false;
    StaticMesh->BuildFromMeshDescriptions({ &MeshDescription }, Params);

    SMC->SetStaticMesh(StaticMesh);   // EN DERNIER
    NotifyFootprint();
}
/*
void AChunckManager::ProcessPendingClusters()
{
    if (!VoxelWorld) return;
    int32 Dispatched = 0;

    auto ProcessTier = [&](TSet<FIntVector>& Pending, int32 LOD)
        {
            if (Pending.Num() == 0) return;
            for (const FIntVector& CC : Pending.Array())
            {
                if (Dispatched >= MaxClusterDispatchPerFrame || CurrentMeshJob >= MaxMeshJob) return;
                if (TryDispatchClusterMesh(CC, LOD)) { Pending.Remove(CC); ++Dispatched; }
            }
        };

    ProcessTier(PendingClusterTier1, 1);
    ProcessTier(PendingClusterTier2, 2);
    ProcessTier(PendingClusterTier3, 3);
}
*/

void AChunckManager::GenerateGreedyMesh(FChunckMeshData& OutMesh, const TArray<FVoxelDataStructure>& PaddedVoxels, FIntVector Coord, int32 LOD)
{
    int32 Step = 1 << LOD;
    int32 VertexCount = 0;
    int32 EffectiveSize = ChunkSize / Step;
    const int32 PaddedSize = ChunkSize + 2;

    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        const int32 Axis1 = (Axis + 1) % 3;
        const int32 Axis2 = (Axis + 2) % 3;

        FIntVector AxisMask = FIntVector::ZeroValue;
        AxisMask[Axis] = 1;

        TArray<FMask> Mask;
        Mask.SetNum(EffectiveSize * EffectiveSize);

        FIntVector Iter = FIntVector::ZeroValue;

        // On itère sur les indices "LOD" (0, 1, 2... EffectiveSize)
        for (Iter[Axis] = -1; Iter[Axis] < EffectiveSize; ++Iter[Axis])
        {
            int32 N = 0;

            // === 1. Construire le Mask ===
            for (Iter[Axis2] = 0; Iter[Axis2] < EffectiveSize; ++Iter[Axis2])
            {
                for (Iter[Axis1] = 0; Iter[Axis1] < EffectiveSize; ++Iter[Axis1])
                {
                    // On multiplie par Step UNIQUEMENT pour lire la donnée voxel
                    const int32 SX = Iter.X * Step;
                    const int32 SY = Iter.Y * Step;
                    const int32 SZ = Iter.Z * Step;

                    const bool CurrentSolid = IsVoxelSolidLocal(SX, SY, SZ, PaddedVoxels, PaddedSize);
                    // On compare avec le bloc suivant (décalé de Step)
                    const bool CompareSolid = IsVoxelSolidLocal(SX + AxisMask.X * Step, SY + AxisMask.Y * Step, SZ + AxisMask.Z * Step, PaddedVoxels, PaddedSize);

                    if (CurrentSolid == CompareSolid) {
                        Mask[N++] = FMask{ 0, 0 };
                    }
                    else if (CurrentSolid) {
                        Mask[N++] = FMask{ 1, 1 };
                    }
                    else {
                        Mask[N++] = FMask{ 1, -1 };
                    }
                }
            }

            N = 0;
            // === 2. Générer les quads ===
            for (int32 j = 0; j < EffectiveSize; ++j)
            {
                for (int32 i = 0; i < EffectiveSize; )
                {
                    if (Mask[N].Normal != 0)
                    {
                        const FMask CurrentMask = Mask[N];
                        int32 Width = 1;
                        while (i + Width < EffectiveSize && CompareMask(Mask[N + Width], CurrentMask))
                            ++Width;

                        int32 Height = 1;
                        bool Done = false;
                        for (; Height + j < EffectiveSize; ++Height)
                        {
                            for (int32 k = 0; k < Width; ++k)
                            {
                                if (!CompareMask(Mask[N + k + Height * EffectiveSize], CurrentMask))
                                {
                                    Done = true; break;
                                }
                            }
                            if (Done) break;
                        }

                        // Calcul des positions (on reste en indices LOD pour l'instant)
                        FIntVector V1 = Iter;
                        V1[Axis1] = i;
                        V1[Axis2] = j;
                        V1[Axis] += 1;
                        // if (CurrentMask.Normal > 0) V1[Axis] += 1;

                        FIntVector V2 = V1; V2[Axis1] += Width;
                        FIntVector V3 = V1; V3[Axis2] += Height;
                        FIntVector V4 = V2; V4[Axis2] += Height;

                        // CLÉ : On multiplie par Step ici pour remettre le quad à la bonne taille monde
                        CreateQuad(CurrentMask, AxisMask, Width, Height,
                            V1 * Step, V2 * Step, V3 * Step, V4 * Step,
                            VertexCount, OutMesh, Step);

                        VertexCount += 4;

                        for (int32 l = 0; l < Height; ++l)
                            for (int32 k = 0; k < Width; ++k)
                                Mask[N + k + l * EffectiveSize] = FMask{ 0, 0 };

                        i += Width; N += Width;
                    }
                    else { ++i; ++N; }
                }
            }
        }
    }
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

bool AChunckManager::IsVoxelSolidLocal(int x, int y, int z, const TArray<FVoxelDataStructure>& LocalVoxels, int32 PaddedSize)
{

    int px = x + 1;
    int py = y + 1;
    int pz = z + 1;
    if (px < 0 || px >= PaddedSize || py < 0 || py >= PaddedSize || pz < 0 || pz >= PaddedSize)
        return false;

    int index = px + py * PaddedSize + pz * PaddedSize * PaddedSize;

    if (!LocalVoxels.IsValidIndex(index))
        return false;

    return LocalVoxels[index].Material.Id > 0;
}

void AChunckManager::CreateQuad(const FMask& Mask, const FIntVector& AxisMask, int32 Width, int32 Height, const FIntVector& V1, const FIntVector& V2, const FIntVector& V3, const FIntVector& V4, int32& VertexCount, FChunckMeshData& MeshData, int32 Step)
{
    const FVector Normal = FVector(AxisMask) * Mask.Normal * VoxelSize;

    const int32 StartIndex = MeshData.Vertices.Num();
    MeshData.Vertices.Append({ FVector(V1) * VoxelSize, FVector(V2) * VoxelSize,
                               FVector(V3) * VoxelSize, FVector(V4) * VoxelSize });

    // Winding automatique (le fameux trick +Mask.Normal / -Mask.Normal)
    MeshData.Triangles.Append({
        StartIndex + 0,
        StartIndex + 2 + Mask.Normal,
        StartIndex + 2 - Mask.Normal,
        StartIndex + 3,
        StartIndex + 1 - Mask.Normal,
        StartIndex + 1 + Mask.Normal
        });

    MeshData.Normals.Append({ Normal, Normal, Normal, Normal });

    // UVs scalées selon l'axe
    if (AxisMask.X != 0) // faces X
    {
        MeshData.UVs.Append({
            FVector2D(0, 0),
            FVector2D(Height, 0),
            FVector2D(Height, Width),
            FVector2D(0, Width)
            });
    }
    else // faces Y ou Z
    {
        MeshData.UVs.Append({
            FVector2D(0, 0),
            FVector2D(Width, 0),
            FVector2D(Width, Height),
            FVector2D(0, Height)
            });
    }

    VertexCount += 4;
}
/*
void AChunckManager::UpdateVisibleChunks(const TSet<FIntVector>& ChunksToKeep)
{
    if (!VoxelWorld)
    {
        UE_LOG(LogTemp, Error, TEXT("VoxelWorld NULL"));
        return;
    }
    int32 SpawnCount = 0;
    TArray<FIntVector> SortedChunks = ChunksToKeep.Array();

    APawn* Pawn = GetWorld()->GetFirstPlayerController()->GetPawn();
    if (!Pawn)
    {
        return;
    }
    FVector PlayerPos = Pawn->GetActorLocation();

    SortedChunks.Sort([&](const FIntVector& A, const FIntVector& B)
        {

            FIntVector DeltaA = A - GetPlayerChunck(PlayerPos);
            FIntVector DeltaB = B - GetPlayerChunck(PlayerPos);

            // Priorité horizontale
            int32 RingA = FMath::Max(FMath::Abs(DeltaA.X), FMath::Abs(DeltaA.Y));
            int32 RingB = FMath::Max(FMath::Abs(DeltaB.X), FMath::Abs(DeltaB.Y));
            if (RingA != RingB)
            {
                return RingA < RingB;
            }
            return FMath::Abs(DeltaA.Z) < FMath::Abs(DeltaB.Z);

        });
    for (const FIntVector& Coord : SortedChunks)
    {
        if (VoxelWorld->Chuncks.Contains(Coord))
        {
            continue;
        }
        //UE_LOG(LogTemp, Warning, TEXT("UpdateVisibleChunks(const TSet<FIntVector>& ChunksToKeep) -> : Coord : %d"), Coord.XYZ);
        if (!VoxelWorld->Chuncks.Contains(Coord))
        {
            if (SpawnCount >= MaxSpawnPerFrame)
            {
                break;
            }
            SpawnChunk(Coord);

            SpawnCount++;
        }
    }

    // 🔥 UNLOAD
    TArray<FIntVector> ToRemove;


    for (auto& Pair : VoxelWorld->Chuncks)
    {
        if (!ChunksToKeep.Contains(Pair.Key))
        {
            ToRemove.Add(Pair.Key);
        }
    }

    for (const FIntVector& Coord : ToRemove)
    {
        FScopeLock Lock(&VoxelWorld->ChunckMutex);
        FChunckDataStructure* Data = VoxelWorld->Chuncks.Find(Coord);
        if (!Data)
        {
            continue;
        }
        if (Data->VoxelChunck.IsValid())
        {
            Data->VoxelChunck->bIsBeingDestroyed = true;
            Data->VoxelChunck->Destroy();
        }
        VoxelWorld->Chuncks.Remove(Coord);
    }
}
*/


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

        if (bReadyToCommit) PendingCommitSet.Add(Result.Coord);

        if (bAccepted)                                    // bloc, PAS un continue
        {
            if (ResultLOD == 0)
            {
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
    if (Now - LastDiag > 1.0)
    {
        LastDiag = Now;
        UE_LOG(LogTemp, Warning,
            TEXT("CLUSTERS pending T1=%d T2=%d T3=%d | dispatches=%d | jobs chunk=%d/%d cluster=%d/%d"),
            PendingClusterTier1.Num(), PendingClusterTier2.Num(), PendingClusterTier3.Num(),
            Dispatched, CurrentMeshJob, MaxMeshJob, CurrentClusterMeshJob, EffectiveClusterBudget);
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
    const int32 MaxSurfaceVoxel = BaseHeight + FMath::CeilToInt(SurfaceAmplitude) + ChunkSize;
    return ChunkMinZVoxel >= MaxSurfaceVoxel;   // tout le chunk est au-dessus de toute surface possible
}

/*
void AChunckManager::InvalidateCluster(FIntVector ClusterCoord, int32 LOD)
{
    if (LOD < 1) return;

    // 1) Effacement immédiat : c'est LE point qui manquait. Ne dépend d'aucun budget,
    //    d'aucun readiness, d'aucun job async. Suppression garantie, comme un Destroy().
    TMap<FIntVector, UStaticMeshComponent*>* Pool =
        (LOD == 1) ? &ClusterPoolTier1 : (LOD == 2) ? &ClusterPoolTier2 : &ClusterPoolTier3;
    if (UStaticMeshComponent** Found = Pool->Find(ClusterCoord))
        //if (UStaticMeshComponent* SMC = *Found)
            //SMC->SetStaticMesh(nullptr);

    // 2) Bump de version : sans ça, un job parti AVANT cet effacement pourrait arriver
    //    APRÈS et ré-appliquer la géométrie périmée par-dessus la section vidée.
    TMap<FIntVector, int32>& VMap =
        (LOD == 1) ? ClusterMeshVersionTier1 :
        (LOD == 2) ? ClusterMeshVersionTier2 : ClusterMeshVersionTier3;
    ++VMap.FindOrAdd(ClusterCoord);

    // 3) Reconstruction demandée (peut prendre plusieurs frames — le trou est visible, tant mieux).
    if (LOD == 1) {
        PendingClusterTier1.Add(ClusterCoord);
        PendingMeshClusterCount += 1;
    }
    else if (LOD == 2) {
        PendingMeshClusterCount += 1;
        PendingClusterTier2.Add(ClusterCoord);
    }
    else {
        PendingMeshClusterCount += 1;
        PendingClusterTier3.Add(ClusterCoord);
    }
}

*/

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
        UE_LOG(LogTemp, Warning, TEXT("LOD WATCHDOG (%d,%d,%d) : libération forcée après %.1fs"),
            Coord.X, Coord.Y, Coord.Z, LODSwapWatchdogSeconds);
        NotifyDisplayApplied(Coord);
    }
}

void AChunckManager::NukeClusters()
{
    int32 Cleared = 0;
    for (auto* Pool : { &ClusterPoolTier1, &ClusterPoolTier2, &ClusterPoolTier3 })
        for (auto& P : *Pool)
            if (P.Value) { P.Value->SetStaticMesh(nullptr); ++Cleared; }
    UE_LOG(LogTemp, Warning, TEXT("NukeClusters : %d composants vidés (manager %s)"), Cleared, *GetName());
}