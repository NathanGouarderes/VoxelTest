// Fill out your copyright notice in the Description page of Project Settings.


#include "ChunckManager.h"
#include "FChunckDataStructure.h"
#include "GameFramework/GameModeBase.h"  
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformMisc.h"
#include "VoxelWorld.h"

// Sets default values
AChunckManager::AChunckManager():
    SurfaceFrequency(0.026f)
    , SurfaceAmplitude(80.0f)
    , BaseHeight(108)
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
}

// Called when the game starts or when spawned
void AChunckManager::BeginPlay()
{
    Super::BeginPlay();
    MaxMeshJob = FMath::Clamp(NumThreads - 2, 2, 8);

    NumThreads = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
    MaxGenPerFrame = FMath::Clamp(NumThreads / 2, 2, 8);

    FTimerHandle Timer;
    GetWorld()->GetTimerManager().SetTimer(Timer, [this]()
        {
            APawn* Pawn = GetWorld()->GetFirstPlayerController()->GetPawn();
            if (!Pawn) return;

            FVector Pos = Pawn->GetActorLocation();
            Pos.Z += 810.f; // 🔥 hauteur safe

            Pawn->SetActorLocation(Pos, false, nullptr, ETeleportType::TeleportPhysics);

        }, 0.5f, false);

    InitNoise();

}

// Called every frame
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
            return; // 🔥 BLOQUANT
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
        FIntVector Coord;
        if (!ChunckGenerationQueue.Dequeue(Coord))
        {
            break;
        }
        FChunckDataStructure* ChunckData = VoxelWorld->Chuncks.Find(Coord);
        if (!ChunckData)
        {
            continue;
        }
        FillChunck(EChunkVariant::Full, Coord);
        //DirtyChuncks.Add(Coord);
    }

    // ===================================================================
    // 3. TRAITEMENT DES DIRTY CHUNKS → triés par distance (near → far)
    // ===================================================================
    APawn* Pawn = GetWorld()->GetFirstPlayerController()->GetPawn();
    if (Pawn && VoxelWorld)
    {
        FVector PlayerPos = Pawn->GetActorLocation();
        int32 RebuildCount = 0;

        TArray<FIntVector> DirtyToProcess = DirtyChuncks.Array();
        //DirtyToProcess.Reserve(DirtyChuncks.Num());

        DirtyToProcess.Sort([&](const FIntVector& A, const FIntVector& B)
            {
                FVector PosA = FVector(A) * ChunkSize * VoxelSize;
                FVector PosB = FVector(B) * ChunkSize * VoxelSize;
                return FVector::DistSquared(PlayerPos, PosA) < FVector::DistSquared(PlayerPos, PosB);
            });

        for (const FIntVector& Coord : DirtyToProcess)
        {
            if (RebuildCount >= MaxRebuildPerFrame)
                break;

            if (!DirtyChuncks.Contains(Coord))
                continue;


            FChunckDataStructure* Chunck = VoxelWorld->Chuncks.Find(Coord);

            if (!Chunck || !Chunck->VoxelChunck || !Chunck->bIsChunckGenerated)
            {
                continue;
            }
            if (Chunck && IsValid(Chunck->VoxelChunck))
            {
                if (!Chunck->VoxelChunck->bIsQueued)
                {
                    Chunck->VoxelChunck->bIsQueued = true;
                    PendingMeshToApply.Enqueue(Chunck->VoxelChunck);
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("Chunk deja queued: %s"), *Coord.ToString());
                }
                if (Chunck && Chunck->VoxelChunck)
                {
                    int32 DesiredLOD = GetLODForChunck(Coord, PlayerPos);

                    // On ne rebuild que si le LOD a changé OU si c'est la première fois
                    if (Chunck->VoxelChunck->CurrentLOD != DesiredLOD || Chunck->VoxelChunck->bIsDirty)
                    {
                        Chunck->VoxelChunck->CurrentLOD = DesiredLOD;
                        Chunck->VoxelChunck->GenerateAsyncGreedyMesh();
                        //PendingMeshToApply.Enqueue(Chunck->VoxelChunck);
                        RebuildCount++;
                    }
                }
            }
            DirtyChuncks.Remove(Coord);            
        }
    }
    while (CurrentMeshJob < MaxMeshJob && !PendingMeshToApply.IsEmpty())
    {
        //UE_LOG(LogTemp, Warning, TEXT("AChunckManager::Tick(float DeltaTime) --> CurrentMeshJob : %d  MaxMeshJob %d"), CurrentMeshJob, MaxMeshJob);
        AVoxelChunck* ChunckToProcess;
        PendingMeshToApply.Dequeue(ChunckToProcess);
        ChunckToProcess->bIsQueued = false;
        if (!ChunckToProcess)
        {
            continue;
        }
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

    if (VoxelWorld->Chuncks.Find(Coord))
    {
        DirtyChuncks.Add(Coord);
        //UE_LOG(LogTemp, Warning, TEXT(" RegisterDirtyChunk(FIntVector Coord)  → Chunk %s enfilé (existe dans la map)"), *Coord.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT(" RegisterDirtyChunk(FIntVector Coord)  → Chunk %s NON ENFILÉ (pas encore créé dans VoxelWorld)"), *Coord.ToString());
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
    
    FChunckDataStructure* ChunckData = VoxelWorld->Chuncks.Find(Coord);
    if (!ChunckData) return;
    int32 TotalSize = ChunkSize * ChunkSize * ChunkSize;
    

    Async(EAsyncExecution::ThreadPool, [this, Coord, TotalSize] {

        // ================================
        // 1. BUFFER LOCAL (THREAD SAFE)
        // ================================


        TArray<FVoxelDataStructure> LocalVoxel;
        LocalVoxel.SetNumZeroed(TotalSize);

        // ================================
        // 2. CALCUL COMPLET (THREAD SAFE)
        // ================================
        for (int x = 0; x < ChunkSize; x++)
        {
            for (int y = 0; y < ChunkSize; y++)
            {
                // Coordonnées globales pour un terrain seamless
                float WorldX = (Coord.X * ChunkSize + x) * SurfaceFrequency;
                float WorldY = (Coord.Y * ChunkSize + y) * SurfaceFrequency;
                float GlobalSurfaceNoiseValue = SurfaceNoise.GetNoise(WorldX, WorldY);

                // Hauteur globale en voxels (pas en unités !)

                int GlobalSurfaceHeight = BaseHeight + FMath::FloorToInt(GlobalSurfaceNoiseValue * SurfaceAmplitude);

                GlobalSurfaceHeight = FMath::Clamp(GlobalSurfaceHeight, 0, ChunkSize * 40); // max 4 chunks de hauteur

                for (int z = 0; z < ChunkSize; z++)
                {
                    int Index = x + y * ChunkSize + z * ChunkSize * ChunkSize;
                    int GlobalZ = Coord.Z * ChunkSize + z;

                    bool IsSolid = (GlobalZ < GlobalSurfaceHeight);

                    if (IsSolid)
                    {
                        float CaveNoiseValue = CaveNoise.GetNoise((Coord.X * ChunkSize + x) * CaveFrequency, (Coord.Y * ChunkSize + y) * CaveFrequency, GlobalZ * CaveFrequency);
                        if (CaveNoiseValue > CaveThreshold)
                        {
                            IsSolid = false; //On creuse un trou
                        }
                    }

                    if (!IsSolid && GlobalZ < SeaLevel)
                    {
                        LocalVoxel[Index].Material.Id = 0; //Remplacer par l'id de l'eau;
                    }
                    else
                    {
                            LocalVoxel[Index].Material.Id = IsSolid ? 1 : 0; 
                    }
                }
            }
        }
        AsyncTask(ENamedThreads::GameThread, [this, Coord, LocalVoxel = MoveTemp(LocalVoxel)]() mutable
            {
                if (!VoxelWorld)
                {
                    UE_LOG(LogTemp, Error, TEXT("AChunckManager::FillChunck(EChunkVariant Variant, FIntVector Coord) → ChunckManager ou VoxelWorld nul !"));
                    return;
                }
                FChunckDataStructure* ChunckData = VoxelWorld->Chuncks.Find(Coord);
                if (!ChunckData)
                {
                    UE_LOG(LogTemp, Error, TEXT("AChunckManager::FillChunck(EChunkVariant Variant, FIntVector Coord) → ChunckData nul !"));
                    return;
                }
                ChunckData->Voxels = MoveTemp(LocalVoxel);
                ChunckData->bIsChunckGenerated = true;
                DirtyChuncks.Add(Coord);
            });
        });
    
}
    


void AChunckManager::SpawnChunk(FIntVector Coord)
{
    if (!VoxelWorld || !VoxelChunckClass)
        return;

    FVector Location = FVector(Coord) * ChunkSize * VoxelSize;

    AVoxelChunck* VoxelChunck = GetWorld()->SpawnActor<AVoxelChunck>(
        VoxelChunckClass, Location, FRotator::ZeroRotator);

    //UE_LOG(LogTemp, Warning, TEXT("AChunckManager::SpawnChunk(FIntVector Coord) --> SPAWN Chunk %s at WorldPos %s"), *Coord.ToString(), *Location.ToString());

    if (!VoxelChunck)
    {
        UE_LOG(LogTemp, Error, TEXT("AChunckManager::SpawnChunk(FIntVector Coord) --> VoxelChunck null"));
        return;
    }

    // 1️⃣ Data
    FChunckDataStructure NewData;
    NewData.Voxels.SetNum(ChunkSize * ChunkSize * ChunkSize);
    NewData.VoxelChunck = VoxelChunck;
    VoxelWorld->Chuncks.Add(Coord, NewData);

    // 2️⃣ Setup
    VoxelChunck->SetChunckManager(this);
    VoxelChunck->Coord = Coord;

    // 3️⃣ Fill
    ChunckGenerationQueue.Enqueue(Coord);
    //FillChunck(EChunkVariant::Full, Coord);
    //GenerateTerrain(NewData, Coord);

    APawn* Pawn = GetWorld()->GetFirstPlayerController()->GetPawn();
    if (Pawn)
    {
        FVector PlayerPos = Pawn->GetActorLocation();

        FIntVector PlayerChunk = GetPlayerChunck(PlayerPos);

        // 🔥 CREUSER UNIQUEMENT SI C'EST LE CHUNK DU JOUEUR
        if (Coord == PlayerChunk)
        {
            FVector ChunkWorldPos = FVector(Coord) * ChunkSize * VoxelSize;
            FVector LocalPos = (PlayerPos - ChunkWorldPos) / VoxelSize;

            int px = FMath::FloorToInt(LocalPos.X);
            int py = FMath::FloorToInt(LocalPos.Y);
            int pz = FMath::FloorToInt(LocalPos.Z);

            int Radius = 3;

            for (int x = px - Radius; x <= px + Radius; x++)
                for (int y = py - Radius; y <= py + Radius; y++)
                    for (int z = pz - Radius; z <= pz + Radius; z++)
                    {
                        if (x >= 0 && x < ChunkSize &&
                            y >= 0 && y < ChunkSize &&
                            z >= 0 && z < ChunkSize)
                        {
                            VoxelChunck->RemoveVoxel(x, y, z);
                        }
                    }
        }
    }

    // 4️⃣ Mesh
    /*

    */
    //FChunckDataStructure* CD = VoxelWorld->Chuncks.Find(Coord);
    if (VoxelWorld->Chuncks.Find(Coord))
    {
        
        //VoxelChunck->GenerateFacedMesh();
        //
        //DirtyChuncks.Add(Coord);
    }
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
    FVector PlayerPos = Pawn->GetActorLocation();

    SortedChunks.Sort([&](const FIntVector& A, const FIntVector& B)
        {

            FIntVector DeltaA = A - GetPlayerChunck(PlayerPos);
            FIntVector DeltaB = B - GetPlayerChunck(PlayerPos);

            // Priorité horizontale
            int32 RingA = FMath::Max(FMath::Abs(DeltaA.X), FMath::Abs(DeltaA.Y));
            int32 RingB = FMath::Max(FMath::Abs(DeltaB.X), FMath::Abs(DeltaB.Y));
            //UE_LOG(LogTemp, Warning, TEXT("Ring A : %d ------------- Ring B : %d"), RingA, RingB);
            if (RingA != RingB)
            {
                return RingA < RingB;
            }
            return FMath::Abs(DeltaA.Z) < FMath::Abs(DeltaB.Z);

            /*
            FVector PosA = FVector(A) * ChunkSize * VoxelSize;
            FVector PosB = FVector(B) * ChunkSize * VoxelSize;

            float HorizontalDistA = FVector2D(PosA.X - PlayerPos.X, PosA.Y - PlayerPos.Y).SizeSquared();
            float HorizontalDistB = FVector2D(PosB.X - PlayerPos.X, PosB.Y - PlayerPos.Y).SizeSquared();

            if (FMath::Abs(HorizontalDistA - HorizontalDistB) > 0.1f)
            {
                return HorizontalDistA < HorizontalDistB;
            }

            float VerticalDistA = FMath::Square(PosA.Z - PlayerPos.Z);
            float VerticalDistB = FMath::Square(PosB.Z - PlayerPos.Z);


            //return FVector::DistSquared(PlayerPos, PosA) < FVector::DistSquared(PlayerPos, PosB);
            return VerticalDistA < VerticalDistB;
            */
            
        });
    for (const FIntVector& Coord : SortedChunks)
    {
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
        if (VoxelWorld->Chuncks[Coord].VoxelChunck)
        {
            VoxelWorld->Chuncks[Coord].VoxelChunck->Destroy();
        }
        VoxelWorld->Chuncks.Remove(Coord);
    }
}


