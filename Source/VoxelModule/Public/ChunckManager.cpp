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
    MaxMeshJob = FMath::Clamp(NumThreads - 2, 2, 14);
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
            int32 ChunkLOD = 0;
            if (FChunckDataStructure* Data = VoxelWorld->Chuncks.Find(Coord))
            {
                ChunkLOD = Data->LOD;
                FillChunck(EChunkVariant::Full, Coord, ChunkLOD);
            }
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

            const FIntVector Dirs[6] = { {-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1} };
            ChunckData->Voxels = MoveTemp(Result.Voxels);
            ChunckData->bIsChunckGenerated = true;
            if (Result.LOD == 0)
            {
                //UE_LOG(LogTemp, Warning, TEXT("LOD = 0"));
                AVoxelChunck* VoxelChunck = ChunckData->VoxelChunck.Get();
                if (!IsValid(VoxelChunck))
                {
                    //continue;
                }

                if (!Result.bIsAllEmpty)
                {
                    DirtyChuncks.Add(Result.Coord);
                }
                for (const FIntVector& Dir : Dirs)
                {
                    FIntVector NeighborCoord = Result.Coord + Dir;
                    if (VoxelWorld->Chuncks.Contains(NeighborCoord))
                        DirtyChuncks.Add(NeighborCoord);
                }
            }
            else
            {
                // === PARTIE 1 : le chunk courant est-il prêt à être meshé ? ===
                // On vérifie que ses 6 voisins existent ET ont leurs voxels générés.
                // Si un seul manque → salle d'attente. Sinon → file de meshing.
                bool bAllNeighborsReady = true;
                for (const FIntVector& Dir : Dirs)
                {
                    FIntVector NeighborCoord = Result.Coord + Dir;
                    FChunckDataStructure* Neighbor = VoxelWorld->Chuncks.Find(NeighborCoord);
                    if (!Neighbor)
                    {
                        continue;
                    }
                    else
                    {
                        if (!Neighbor->bIsChunckGenerated)
                        {
                            bAllNeighborsReady = false;
                            break;
                        }
                    }
                }

                if (bAllNeighborsReady)
                {
                    FChunckDataStructure* ResultData = VoxelWorld->Chuncks.Find(Result.Coord);
                    if (ResultData->bIsQueued == false)
                    {
                        ResultData->bIsQueued = true;
                        PendingMeshToApply.Enqueue(Result.Coord);
                    }
                }
                else
                {
                    PendingLODMesh.Add(Result.Coord);
                    //UE_LOG(LogTemp, Warning, TEXT("AChunckManager::Tick(float DeltaTime) --> Coordonnées %d, %d %d ont été ajoutées dans PendingLODMesh"), Result.Coord.X, Result.Coord.Y, Result.Coord.Z);
                }
            }
            

            // === PARTIE 2 : ce chunk débloque-t-il des voisins en attente ? ===
            // Ce chunk vient d'être prêt — il était peut-être le dernier voisin
            // manquant pour un chunk qui attendait dans PendingLODMesh.
            for (const FIntVector& Dir : Dirs)
            {
                FIntVector NeighborCoord = Result.Coord + Dir;
                if (!PendingLODMesh.Contains(NeighborCoord))
                    continue;

                // Ce voisin attendait. On re-vérifie SES propres 6 voisins.
                // Variable séparée : indépendante de bAllNeighborsReady.
                bool bNeighborNowReady = true;
                for (const FIntVector& Dir2 : Dirs)
                {
                    FIntVector NeighborOfNeighbor = NeighborCoord + Dir2;
                    FChunckDataStructure* NON = VoxelWorld->Chuncks.Find(NeighborOfNeighbor);
                    if (!NON || !NON->bIsChunckGenerated)
                    {
                        bNeighborNowReady = false;
                        break;
                    }
                }

                if (bNeighborNowReady)
                {
                    PendingLODMesh.Remove(NeighborCoord);
                    FChunckDataStructure* NeighboorData = VoxelWorld->Chuncks.Find(NeighborCoord);
                    if (NeighboorData->bIsQueued == false)
                    {
                        NeighboorData->bIsQueued = true;
                        PendingMeshToApply.Enqueue(NeighborCoord);
                    }
                    //UE_LOG(LogTemp, Warning, TEXT("AChunckManager::Tick(float DeltaTime) --> Coordonnées %d, %d %d ont été enque dans PendingMeshToApply"), NeighborCoord.X, NeighborCoord.Y, NeighborCoord.Z);
                }
            }

        }
        Count++;
        
    }

    FClusterGenResult ClusterResult;
    TArray<FIntVector> CoordList;
   
    while (ClusterGenerationResult.Dequeue(ClusterResult))
    {
        FIntVector ClusterCoord = GetClusterCoord(ClusterResult.MeshData.ChunckCoord, ClusterResult.LOD);
        switch (ClusterResult.LOD)
        {
        case 1:
        {
            TArray<FChunckMeshData>& ClusterArray = ClusterMapTier1.FindOrAdd(ClusterCoord);

            // Vérification doublon AVANT l'ajout, dans le bon cluster
            bool bAlreadyPresent = false;
            for (const FChunckMeshData& Existing : ClusterArray)
            {
                if (Existing.ChunckCoord == ClusterResult.MeshData.ChunckCoord)
                {
                    bAlreadyPresent = true;
                    UE_LOG(LogTemp, Warning, TEXT("Doublon dans cluster (%d,%d,%d) : chunk (%d,%d,%d)"),
                        ClusterCoord.X, ClusterCoord.Y, ClusterCoord.Z,
                        ClusterResult.MeshData.ChunckCoord.X,
                        ClusterResult.MeshData.ChunckCoord.Y,
                        ClusterResult.MeshData.ChunckCoord.Z);
                    break;
                }
            }

            if (!bAlreadyPresent)
            {
                ClusterArray.Add(ClusterResult.MeshData);
            }

            if (ClusterArray.Num() >= 16)
            {
                ApplyMeshToCluster(ClusterArray, ClusterPoolTier1, ClusterCoord, ClusterResult.LOD);
                ClusterMapTier1.Remove(ClusterCoord);
            }
            break;
        }
        case 2:
            ClusterMapTier2.FindOrAdd(ClusterCoord).Add(ClusterResult.MeshData);
            if (ClusterMapTier2[ClusterCoord].Num() == 25) //6
            {
                ApplyMeshToCluster(ClusterMapTier2[ClusterCoord], ClusterPoolTier2, ClusterCoord, ClusterResult.LOD);
                ClusterMapTier2.Remove(ClusterCoord);
            }
            break;
        case 3:
            ClusterMapTier3.FindOrAdd(ClusterCoord).Add(ClusterResult.MeshData);
            if (ClusterMapTier3[ClusterCoord].Num() == 1024)
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
                if (!IsValid(VoxelChunck))
                    continue;
                VoxelChunck->CurrentLOD = DesiredLOD;
                if (Chunck->bIsQueued == false)
                {
                    Chunck->bIsQueued = true;
                    PendingMeshToApply.Enqueue(VoxelChunck->Coord);
                }
                
            }
            DirtyChuncks.Remove(Coord);
            RebuildCount++;
        }
    }
    while (CurrentMeshJob < MaxMeshJob && !PendingMeshToApply.IsEmpty())
    {
        FIntVector Coord;
        if (!PendingMeshToApply.Dequeue(Coord))
            break;

        if (Coord.IsZero())
            continue;

        UE_LOG(LogTemp, Warning,
            TEXT("START MESH JOB : (%d,%d,%d)"),
            Coord.X,
            Coord.Y,
            Coord.Z);
        CurrentMeshJob++;
        GenerateAsyncGreedyMesh(Coord);
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
    SurfaceNoise.SetFrequency(SurfaceFrequency);
    CaveNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    CaveNoise.SetFrequency(CaveFrequency);
}



void AChunckManager::FillChunck(EChunkVariant Variant, FIntVector Coord, int32 LOD)
{
    
    //if (!ChunckData) return;
    int32 TotalSize = ChunkSize * ChunkSize * ChunkSize;

    ChunckGenerationJobQueue.Enqueue(FChunkGenJob(Coord, Variant, this, LOD));
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


void AChunckManager::SpawnChunk(FIntVector Coord, int32 LOD, int32 GenerationId)
{
    if (!VoxelWorld || !VoxelChunckClass || !IsValid(VoxelWorld))
        return;

    FVector Location = FVector(Coord) * ChunkSize * VoxelSize;
    FChunckDataStructure NewData;
    NewData.Voxels.SetNum(ChunkSize * ChunkSize * ChunkSize);

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

    ChunckGenerationJobQueue.Enqueue(FChunkGenJob(Coord, EChunkVariant::Full, this, LOD));
}


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
        int32 ChunkLOD = CalculChunkLODBeforeSpawn(Coord);

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
}

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
    AVoxelChunck* VoxelChunk = (ChunkData->LOD == 0) ? ChunkData->VoxelChunck.Get() : nullptr;
    if (ChunkData->LOD == 0 && (!IsValid(VoxelChunk) || VoxelChunk->bIsBeingDestroyed))
    {
        //UE_LOG(LogTemp, Error, TEXT("AChunckManager::GenerateAsyncGreedyMesh(FIntVector Coord) --> ChunkData->LOD == 0 && (!IsValid(VoxelChunk) || VoxelChunk->bIsBeingDestroyed)"));
        CurrentMeshJob--;
        return;

    }

    // On capture les données nécessaires
    const int32 PaddedSize = ChunkSize + 2;
    TArray<FVoxelDataStructure> PaddedVoxels;
    PaddedVoxels.SetNum(PaddedSize * PaddedSize * PaddedSize);

    // Remplissage du padded (copie du chunk + voisins)
    for (int z = 0; z < ChunkSize; ++z)
        for (int y = 0; y < ChunkSize; ++y)
            for (int x = 0; x < ChunkSize; ++x)
            {
                int srcIdx = x + y * ChunkSize + z * ChunkSize * ChunkSize;
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

        for (int a = 0; a < ChunkSize; ++a)
        {
            for (int b = 0; b < ChunkSize; ++b)
            {
                int nx = (Dir.X == -1) ? (ChunkSize - 1) : (Dir.X == 1) ? 0 : a;
                int ny = (Dir.Y == -1) ? (ChunkSize - 1) : (Dir.Y == 1) ? 0 : (Dir.X != 0) ? a : b;
                int nz = (Dir.Z == -1) ? (ChunkSize - 1) : (Dir.Z == 1) ? 0 : (Dir.X != 0) ? b : (Dir.Y != 0) ? b : 0;

                int px = (Dir.X == -1) ? 0 : (Dir.X == 1) ? PaddedSize - 1 : a + 1;
                int py = (Dir.Y == -1) ? 0 : (Dir.Y == 1) ? PaddedSize - 1 : (Dir.X != 0) ? a + 1 : b + 1;
                int pz = (Dir.Z == -1) ? 0 : (Dir.Z == 1) ? PaddedSize - 1 : (Dir.X != 0) ? b + 1 : (Dir.Y != 0) ? b + 1 : 1;

                int srcIdx = nx + ny * ChunkSize + nz * ChunkSize * ChunkSize;
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

            AsyncTask(ENamedThreads::GameThread, [WeakManager, WeakChunk, MeshData = MoveTemp(MeshData), CapturedLOD, Coord, CapturedGenerationId, this]() 
                {
                    if (!WeakManager.IsValid())
                        return;

                    FChunckDataStructure* Data = VoxelWorld->Chuncks.Find(Coord);
                    if (!Data ||CapturedGenerationId != Data->GenerationId)
                    {
                        return;
                    }

                    if ((!WeakChunk.IsValid() || WeakChunk->bIsBeingDestroyed) && CapturedLOD == 0)
                    {
                        UE_LOG(LogTemp, Error, TEXT("AChunckManager::GenerateAsyncGreedyMesh(FIntVector Coord) --> WeakChunk.IsValid() n'est pas valide ou  WeakChunk->bIsBeingDestroyed"));
                        return;
                    }
                    AChunckManager* Manager = WeakManager.Get();
                    if (CapturedLOD == 0 && WeakChunk.IsValid())
                    {
                        WeakChunk->ApplyMesh(MeshData);
                    }
                    else
                    {
                   
                         FClusterGenResult Result;
                         Result.LOD = CapturedLOD;
                         Result.MeshData = MeshData;
                         Result.MeshData.ChunckCoord = Coord;
                         UE_LOG(LogTemp, Warning,
                             TEXT("END MESH JOB : (%d,%d,%d)"),
                             Coord.X,
                             Coord.Y,
                             Coord.Z);
                         Manager->ClusterGenerationResult.Enqueue(Result);
                     
                    }

                    if (WeakChunk.IsValid())
                    {
                        WeakChunk->bIsQueued = false;
                    }

                    Manager->CurrentMeshJob = FMath::Max(0, Manager->CurrentMeshJob - 1);
                 
                });
        });
}

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
