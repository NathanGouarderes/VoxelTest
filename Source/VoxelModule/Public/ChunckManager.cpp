// Fill out your copyright notice in the Description page of Project Settings.


#include "ChunckManager.h"
#include "FChunckDataStructure.h"
#include "GameFramework/GameModeBase.h"  
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformMisc.h"
#include "FChunkGenResult.h"
#include "ChunckGenWorker.h"
#include "VoxelWorld.h"

// Sets default values
AChunckManager::AChunckManager():
    SurfaceFrequency(0.026f)
    , SurfaceAmplitude(80.0f)
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
    ChunkSize = 32;
    VoxelSize = 10.0f;
}

// Called when the game starts or when spawned
void AChunckManager::BeginPlay()
{
    Super::BeginPlay();

    UProceduralMeshComponent* ClusterProceduralMeshComponent = NewObject<UProceduralMeshComponent>(this);
    ClusterProceduralMeshComponent->RegisterComponent();
    ClusterProceduralMeshComponent->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
    //ClusterProceduralMeshComponent->SetWorldLocation(ClusterWorldPos);

    NumThreads = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
    MaxMeshJob = FMath::Clamp(NumThreads - 2, 2, 8);
    MaxGenPerFrame = FMath::Clamp(NumThreads / 2, 2, 8);

    FTimerHandle Timer;
    GetWorld()->GetTimerManager().SetTimer(Timer, [this]()
        {
            APawn* Pawn = GetWorld()->GetFirstPlayerController()->GetPawn();
            if (!Pawn) return;

            FVector Pos = Pawn->GetActorLocation();
            Pos.Z += 2000.f; // 🔥 hauteur safe

            Pawn->SetActorLocation(Pos, false, nullptr, ETeleportType::TeleportPhysics);

        }, 0.5f, false);

    InitNoise();

    for (int32 i = 0; i < MaxMeshJob; i++)
    {
        ChunckGenWorker* Worker = new ChunckGenWorker(this, ChunckGenerationJobQueue);

        FRunnableThread* Thread = FRunnableThread::Create(Worker, *FString::Printf(TEXT("ChunckWorker_%d"), i));

        Workers.Add(Worker);
        WorkerThreads.Add(Thread);
    }

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

    // ===================================================================
    // 1. Mise à jour visibilité (lourde) → throttle 0.2s
    // ==================================================================
    
    if (!VoxelWorld)
    {
        TArray<AActor*> ActorsFound;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVoxelWorld::StaticClass(), ActorsFound);
        if (ActorsFound.Num() > 0)
        {
            VoxelWorld = Cast<AVoxelWorld>(ActorsFound[0]);
        }

        if (!VoxelWorld)
        {
            return;
        }
    }  
    

    bool bDoVisibilityUpdate = (GetWorld()->GetTimeSeconds() - LastUpdateTime >= 0.2f);
    

    // ===================================================================
    // 2. Mise à jour des chunks visibles (spawn / unload)
    // ===================================================================
    bNeedUpdate = false;
    TSet<FIntVector> GlobalChunksToKeep;
    GetAllPlayerChunks(GlobalChunksToKeep);

    if (bDoVisibilityUpdate && (bNeedUpdate || VoxelWorld->Chuncks.Num() < GlobalChunksToKeep.Num()))
    {
        UpdateVisibleChunks(GlobalChunksToKeep);
        LastUpdateTime = GetWorld()->GetTimeSeconds();
    }
    for (int i = 0; i < MaxGenPerFrame; i++)
    {
        {
            FScopeLock Lock(&VoxelWorld->ChunckMutex);
            FIntVector Coord;
            if (!ChunckGenerationQueue.Dequeue(Coord))
            {
                break;
            }
            FillChunck(EChunkVariant::Full, Coord);
        }

    }

    // ===================================================================
    // 3. TRAITEMENT DES DIRTY CHUNKS → triés par distance (near → far)
    // ===================================================================

    FChunkGenResult Result;

    int32 MaxApply = 30;
    int32 Count = 0;

    while (Count < MaxApply && ChunckGenerationResult.Dequeue(Result))
    {
        if (!VoxelWorld) continue;

        {
            FScopeLock Lock(&VoxelWorld->ChunckMutex);
            FChunckDataStructure* ChunckData = VoxelWorld->Chuncks.Find(Result.Coord);
            if (!ChunckData) continue;

            ChunckData->Voxels = MoveTemp(Result.Voxels);
            ChunckData->bIsChunckGenerated = true;

            if (Result.LOD == 0)
            {
                AVoxelChunck* VoxelChunck = ChunckData->VoxelChunck.Get();
                if (!IsValid(VoxelChunck))
                {
                    continue;
                }

                if (!Result.bIsAllEmpty)
                {
                    DirtyChuncks.Add(Result.Coord);
                }
                const FIntVector Dirs[6] = { {-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1} };
                for (const FIntVector& Dir : Dirs)
                {
                    FIntVector NeighborCoord = Result.Coord + Dir;
                    if (VoxelWorld->Chuncks.Contains(NeighborCoord))
                        DirtyChuncks.Add(NeighborCoord);
                }
            }
            else
            {
                PendingClusterCoordToApply.Enqueue(Result.Coord);
            }

        }
        Count++;
        
    }

    FClusterGenResult ClusterResult;
    while (ClusterGenerationResult.Dequeue(ClusterResult))
    {
        FIntVector ClusterCoord = GetClusterCoord(ClusterResult.MeshData.ChunckCoord, ClusterResult.LOD);
        switch (ClusterResult.LOD)
        {
        case 1:
            ClusterMapTier1.FindOrAdd(ClusterCoord).Add(ClusterResult.MeshData);
            if (ClusterMapTier1[ClusterCoord].Num() == 64)
            {
                ApplyMeshToCluster(ClusterMapTier1[ClusterCoord], ClusterPoolTier1, ClusterCoord, ClusterResult.LOD);
                ClusterMapTier1.Remove(ClusterCoord);
            }
            break;
        case 2:
            ClusterMapTier2.FindOrAdd(ClusterCoord).Add(ClusterResult.MeshData);
            if (ClusterMapTier2[ClusterCoord].Num() == 4096)
            {
                ApplyMeshToCluster(ClusterMapTier2[ClusterCoord], ClusterPoolTier2, ClusterCoord, ClusterResult.LOD);
                ClusterMapTier2.Remove(ClusterCoord);
            }
            break;
        case 3:
            ClusterMapTier3.FindOrAdd(ClusterCoord).Add(ClusterResult.MeshData);
            if (ClusterMapTier3[ClusterCoord].Num() == 32768)
            {
                ApplyMeshToCluster(ClusterMapTier3[ClusterCoord], ClusterPoolTier3, ClusterCoord, ClusterResult.LOD);
                ClusterMapTier3.Remove(ClusterCoord);
            }
            break;
        default:
            break;
        }
    }


    APawn* Pawn = GetWorld()->GetFirstPlayerController()->GetPawn();
    if (Pawn && VoxelWorld)
    {
        FVector PlayerPos = Pawn->GetActorLocation();
        int32 RebuildCount = 0;

        TArray<FIntVector> DirtyToProcess = DirtyChuncks.Array();

        DirtyToProcess.Sort([&](const FIntVector& A, const FIntVector& B)
            {
                FVector PosA = FVector(A) * ChunkSize * VoxelSize;
                FVector PosB = FVector(B) * ChunkSize * VoxelSize;
                return FVector::DistSquared(PlayerPos, PosA) < FVector::DistSquared(PlayerPos, PosB);
            });

        for (const FIntVector& Coord : DirtyToProcess)
        {
            DesiredLOD = GetLODForChunck(Coord, PlayerPos);
            if (RebuildCount >= MaxRebuildPerFrame)
                break;

            if (!DirtyChuncks.Contains(Coord))
                continue;
            {
                FScopeLock Lock(&VoxelWorld->ChunckMutex);
                FChunckDataStructure* Chunck = VoxelWorld->Chuncks.Find(Coord);
                if (!Chunck || !Chunck->bIsChunckGenerated)
                    continue;

                AVoxelChunck* VoxelChunck = Chunck->VoxelChunck.Get();
                VoxelChunck->CurrentLOD = DesiredLOD;
                if (!IsValid(VoxelChunck))
                    continue;
                PendingMeshToApply.Enqueue(VoxelChunck);
            }
            
            DirtyChuncks.Remove(Coord);
            RebuildCount++;
        }
    }
    while (CurrentMeshJob < MaxMeshJob && !PendingMeshToApply.IsEmpty())
    {
        AVoxelChunck* ChunckToProcess = nullptr;

        if (!PendingMeshToApply.Dequeue(ChunckToProcess))
        {
            break;
        }

        if (!IsValid(ChunckToProcess))
        {
            continue;
        }
        ChunckToProcess->bIsQueued = false;
        CurrentMeshJob++;
        
        ChunckToProcess->GenerateAsyncGreedyMesh();
    }

    while (CurrentMeshJob < MaxMeshJob && !PendingClusterCoordToApply.IsEmpty())
    {
        FIntVector Coord;
        if (!PendingClusterCoordToApply.Dequeue(Coord))
        {
            break;
        }
        CurrentMeshJob++;
        GenerateGreedyMesh(Coord);

    }
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
            UE_LOG(LogTemp, Warning, TEXT("AChunckManager::GetLODForChunck --> ChunkCoord : X %d Y %d, Z %d, LOD : %d"), Coord.X, Coord.Y, Coord.Z, FMath::Clamp(i, 0, MaxLOD));
            return FMath::Clamp(i, 0, MaxLOD);
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("AChunckManager::GetLODForChunck --> ChunkCoord : X %d Y %d, Z %d, LOD : %d"), Coord.X, Coord.Y, Coord.Z, MaxLOD);
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
    SurfaceNoise.SetFrequency(SurfaceFrequency);
    CaveNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    CaveNoise.SetFrequency(CaveFrequency);
}



void AChunckManager::FillChunck(EChunkVariant Variant, FIntVector Coord)
{
    
    //if (!ChunckData) return;
    int32 TotalSize = ChunkSize * ChunkSize * ChunkSize;

    ChunckGenerationJobQueue.Enqueue(FChunkGenJob(Coord, Variant, this));
}

FIntVector AChunckManager::GetClusterCoord(FIntVector Coord, int LOD)
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

    FIntVector ClusterCoord(
        FMath::FloorToInt((float)Coord.X / NbChunk),
        FMath::FloorToInt((float)Coord.Y / NbChunk),
        FMath::FloorToInt((float)Coord.Z / NbChunk)
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
    FIntVector ClusterOrigine = ClusterCoord * ClusterSize;

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
            MergedNormals.Add(Normal );
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


void AChunckManager::SpawnChunk(FIntVector Coord, int32 LOD)
{
    if (!VoxelWorld || !VoxelChunckClass || !IsValid(VoxelWorld))
        return;
    if (VoxelWorld->Chuncks.Contains(Coord))
    {
        return;
    }

    FVector Location = FVector(Coord) * ChunkSize * VoxelSize;
    FChunckDataStructure NewData;
    NewData.Voxels.SetNum(ChunkSize * ChunkSize * ChunkSize);

    if (LOD == 0)
    {
        AVoxelChunck* VoxelChunck = GetWorld()->SpawnActor<AVoxelChunck>(
            VoxelChunckClass, Location, FRotator::ZeroRotator);

        //UE_LOG(LogTemp, Warning, TEXT("AChunckManager::SpawnChunk(FIntVector Coord) --> SPAWN Chunk %s at WorldPos %s"), *Coord.ToString(), *Location.ToString());

        if (!IsValid(VoxelChunck))
        {
            UE_LOG(LogTemp, Error, TEXT("AChunckManager::SpawnChunk(FIntVector Coord) --> VoxelChunck null"));
            return;
        }

        NewData.VoxelChunck = VoxelChunck;
        VoxelChunck->SetChunckManager(this);
        VoxelChunck->Coord = Coord;

    }
    

    // 1️⃣ Data
    
    {
        FScopeLock Lock(&VoxelWorld->ChunckMutex);
        NewData.GenerationId = 1;
        VoxelWorld->Chuncks.Add(Coord, MoveTemp(NewData));
    }

    ChunckGenerationQueue.Enqueue(Coord);
    

}

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
            SpawnChunk(Coord, DesiredLOD);

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


