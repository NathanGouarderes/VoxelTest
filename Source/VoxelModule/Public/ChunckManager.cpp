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
            AVoxelChunck* VoxelChunck = ChunckData->VoxelChunck.Get();
            if (!IsValid(VoxelChunck))
            {
                continue;
            }

            ChunckData->Voxels = MoveTemp(Result.Voxels);
            ChunckData->bIsChunckGenerated = true;

        }

        if (!Result.bIsAllEmpty && Result.bIsAllSolid)
        {
            DirtyChuncks.Add(Result.Coord);
        }
        const FIntVector Dirs[6] = { {-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1} };
        for (const FIntVector& Dir : Dirs)
        {
            FIntVector NeighborCoord = Result.Coord + Dir;
            FScopeLock Lock(&VoxelWorld->ChunckMutex);
            if (VoxelWorld->Chuncks.Contains(NeighborCoord))
                DirtyChuncks.Add(NeighborCoord);
        }
        Count++;
        
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
            return FMath::Clamp(i, 0, MaxLOD);
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("AChunckManager::GetLODForChunck --> ChunkCoord : X %d Y %d, Z %d, LOD : "), Coord.X, Coord.Y, Coord.Z, MaxLOD);
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

    


void AChunckManager::SpawnChunk(FIntVector Coord)
{
    if (!VoxelWorld || !VoxelChunckClass || !IsValid(VoxelWorld))
        return;
    if (VoxelWorld->Chuncks.Contains(Coord))
    {
        return;
    }

    FVector Location = FVector(Coord) * ChunkSize * VoxelSize;

    AVoxelChunck* VoxelChunck = GetWorld()->SpawnActor<AVoxelChunck>(
        VoxelChunckClass, Location, FRotator::ZeroRotator);

    //UE_LOG(LogTemp, Warning, TEXT("AChunckManager::SpawnChunk(FIntVector Coord) --> SPAWN Chunk %s at WorldPos %s"), *Coord.ToString(), *Location.ToString());

    if (!IsValid(VoxelChunck))
    {
        UE_LOG(LogTemp, Error, TEXT("AChunckManager::SpawnChunk(FIntVector Coord) --> VoxelChunck null"));
        return;
    }

    // 1️⃣ Data
    FChunckDataStructure NewData;
    NewData.Voxels.SetNum(ChunkSize * ChunkSize * ChunkSize);
    NewData.VoxelChunck = VoxelChunck;
    
    //VoxelWorld->Chuncks.Add(Coord, NewData);
    {
        FScopeLock Lock(&VoxelWorld->ChunckMutex);
        NewData.GenerationId = 1;
        VoxelWorld->Chuncks.Add(Coord, MoveTemp(NewData));
    }
    VoxelChunck->SetChunckManager(this);
    VoxelChunck->Coord = Coord;

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


bool AChunckManager::UpdatePlayerChunkState()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        bPlayerChangedChunk = false;
        return false;
    }

    bool bAnyChanged = false;

    // Sert à savoir quels pawns existent encore cette frame
    TSet<APawn*> SeenThisFrame;
    SeenThisFrame.Reserve(LastPlayerChunks.Num() + 1);

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PlayerController = It->Get();
        if (!PlayerController)
        {
            continue;
        }

        APawn* Pawn = PlayerController->GetPawn();
        if (!Pawn)
        {
            continue;
        }

        SeenThisFrame.Add(Pawn);

        const FIntVector CurrentChunk = GetPlayerChunck(Pawn->GetActorLocation());

        if (FIntVector* CachedChunk = LastPlayerChunks.Find(Pawn))
        {
            if (*CachedChunk != CurrentChunk)
            {
                *CachedChunk = CurrentChunk;
                bAnyChanged = true;
            }
        }
        else
        {
            LastPlayerChunks.Add(Pawn, CurrentChunk);
            bAnyChanged = true;
        }
    }

    // Nettoyage des pawns qui n'existent plus / ne sont plus possédés
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

void AChunckManager::MarkVisibilityClean()
{
    bVisibilityInitialized = false;
    bPlayerChangedChunk = false;
    bStreamingSettingsDirty = false;
    bForceVisibilityRefresh = false;
}


bool AChunckManager::ShouldRebuildVisibility() const
{
    return bNeedsInitialBuild
        || bPlayerChangedChunk
        || bStreamingSettingsDirty
        || bForceVisibilityRefresh;
}

bool AChunckManager::ResolveVoxelWorldIfNeeded()
{
    
     if (IsValid(VoxelWorld))
    {
        return true;
    }

    TArray<AActor*> ActorsFound;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVoxelWorld::StaticClass(), ActorsFound);

    if (ActorsFound.Num() > 0)
    {
        VoxelWorld = Cast<AVoxelWorld>(ActorsFound[0]);
    }

    return IsValid(VoxelWorld);
}



void AChunckManager::RebuildDesiredChunkSet(TSet<FIntVector>& OutChunksToKeep)
{
    OutChunksToKeep.Reset();

    const int32 HorizontalRadius = FMath::Max(0, HorizontalViewDistance);
    const int32 VerticalRadius   = FMath::Max(0, VerticalViewDistance);
    const int32 HorizontalRadiusSq = HorizontalRadius * HorizontalRadius;

    // Précondition logique :
    // LastPlayerChunks doit déjà avoir été mis à jour par UpdatePlayerChunkState()
    for (const TPair<APawn*, FIntVector>& Source : LastPlayerChunks)
    {
        APawn* Pawn = Source.Key;
        if (!IsValid(Pawn))
        {
            continue;
        }

        const FIntVector& Center = Source.Value;

        for (int32 dx = -HorizontalRadius; dx <= HorizontalRadius; ++dx)
        {
            for (int32 dy = -HorizontalRadius; dy <= HorizontalRadius; ++dy)
            {
                // Coupe cylindrique / disque horizontal
                const int32 DistSq = dx * dx + dy * dy;
                if (DistSq > HorizontalRadiusSq)
                {
                    continue;
                }

                for (int32 dz = -VerticalRadius; dz <= VerticalRadius; ++dz)
                {
                    OutChunksToKeep.Add(Center + FIntVector(dx, dy, dz));
                }
            }
        }
    }
}



void AChunckManager::BuildStreamingQueues(const TSet<FIntVector>& DesiredChunks)
{
    if (!IsValid(VoxelWorld))
    {
        return;
    }

    // =========================================================
    // 1. SPAWN : DesiredChunks - ExistingChunks
    // =========================================================
    for (const FIntVector& Coord : DesiredChunks)
    {
        bool bAlreadyExists = false;
        {
            FScopeLock Lock(&VoxelWorld->ChunckMutex);
            bAlreadyExists = VoxelWorld->Chuncks.Contains(Coord);
        }

        if (!bAlreadyExists && !PendingSpawnSet.Contains(Coord))
        {
            PendingSpawnQueue.Enqueue(Coord);
            PendingSpawnSet.Add(Coord);
        }

        // Si ce chunk était précédemment marqué pour unload,
        // mais qu'il redevient désiré avant traitement, on annule juste le flag.
        if (PendingUnloadSet.Contains(Coord))
        {
            PendingUnloadSet.Remove(Coord);
            // NOTE :
            // l'entrée éventuelle encore présente dans PendingUnloadQueue sera ignorée
            // plus tard dans ProcessUnloadQueue() grâce au test sur PendingUnloadSet.
        }
    }

    // =========================================================
    // 2. UNLOAD : ExistingChunks - DesiredChunks
    // =========================================================
    TArray<FIntVector> ExistingCoords;
    {
        FScopeLock Lock(&VoxelWorld->ChunckMutex);
        ExistingCoords.Reserve(VoxelWorld->Chuncks.Num());

        for (const TPair<FIntVector, FChunckDataStructure>& Pair : VoxelWorld->Chuncks)
        {
            ExistingCoords.Add(Pair.Key);
        }
    }

    for (const FIntVector& Coord : ExistingCoords)
    {
        if (!DesiredChunks.Contains(Coord) && !PendingUnloadSet.Contains(Coord))
        {
            PendingUnloadQueue.Enqueue(Coord);
            PendingUnloadSet.Add(Coord);
        }

        // Si ce chunk était précédemment en attente de spawn,
        // mais qu'il n'est plus désiré avant traitement, on enlève juste le flag miroir.
        if (!DesiredChunks.Contains(Coord) && PendingSpawnSet.Contains(Coord))
        {
            PendingSpawnSet.Remove(Coord);
            // NOTE :
            // l'entrée éventuelle encore présente dans PendingSpawnQueue sera ignorée
            // plus tard dans ProcessSpawnQueue() grâce au test d'existence / set miroir.
        }
    }
}


