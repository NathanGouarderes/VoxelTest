// Fill out your copyright notice in the Description page of Project Settings.


#include "ChunckManager.h"
#include "FChunckDataStructure.h"
#include "GameFramework/GameModeBase.h"  
#include "Kismet/GameplayStatics.h"
#include "VoxelWorld.h"

// Sets default values
AChunckManager::AChunckManager()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;
    VoxelWorld = nullptr;
    VoxelChunckClass = AVoxelChunck::StaticClass();
    bNeedUpdate = false;
}

// Called when the game starts or when spawned
void AChunckManager::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Warning, TEXT("AChunckManager::BeginPlay() --> Spawné et lié par AVoxelWorld"));


    FTimerHandle Timer;
    GetWorld()->GetTimerManager().SetTimer(Timer, [this]()
        {
            APawn* Pawn = GetWorld()->GetFirstPlayerController()->GetPawn();
            if (!Pawn) return;

            FVector Pos = Pawn->GetActorLocation();
            Pos.Z += 200.f; // 🔥 hauteur safe

            Pawn->SetActorLocation(Pos, false, nullptr, ETeleportType::TeleportPhysics);

            UE_LOG(LogTemp, Warning, TEXT("Player déplacé hors des chunks"));
        }, 0.5f, false);

}

// Called every frame
// Called every frame
void AChunckManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // ===================================================================
    // 1. Mise à jour visibilité (lourde) → throttle 0.2s
    // ===================================================================

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


    if (GetWorld()->GetTimeSeconds() - LastUpdateTime < 0.2f)
    {
        if (!VoxelWorld)
        {
            TArray<AActor*> ActorsFound;
            UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVoxelWorld::StaticClass(), ActorsFound);
            if (ActorsFound.Num() > 0)
            {
                VoxelWorld = Cast<AVoxelWorld>(ActorsFound[0]);
                if (VoxelWorld)
                    UE_LOG(LogTemp, Warning, TEXT("VoxelWorld trouvé et lié !"));
            }
        }
        return;
    }

    // ===================================================================
    // 2. Mise à jour des chunks visibles (spawn / unload)
    // ===================================================================
    bNeedUpdate = false;
    TSet<FIntVector> GlobalChunksToKeep;
    GetAllPlayerChunks(GlobalChunksToKeep);

    if (bNeedUpdate || VoxelWorld->Chuncks.Num() < GlobalChunksToKeep.Num())
    {
        UpdateVisibleChunks(GlobalChunksToKeep);
    }

    // ===================================================================
    // 3. TRAITEMENT DES DIRTY CHUNKS → triés par distance (near → far)
    // ===================================================================
    APawn* Pawn = GetWorld()->GetFirstPlayerController()->GetPawn();
    if (Pawn && VoxelWorld)
    {
        FVector PlayerPos = Pawn->GetActorLocation();
        int32 RebuildCount = 0;

        TArray<FIntVector> DirtyToProcess;
        DirtyToProcess.Reserve(DirtyChuncks.Num());

        for (const FIntVector& Coord : DirtyChuncks)
            DirtyToProcess.Add(Coord);

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

            DirtyChuncks.Remove(Coord);

            FChunckDataStructure* Chunck = VoxelWorld->Chuncks.Find(Coord);
            if (Chunck && Chunck->VoxelChunck)
            {
                int32 DesiredLOD = GetLODForChunck(Coord, PlayerPos);

                // On ne rebuild que si le LOD a changé OU si c'est la première fois
                if (Chunck->VoxelChunck->CurrentLOD != DesiredLOD || Chunck->VoxelChunck->bIsDirty)
                {
                    Chunck->VoxelChunck->CurrentLOD = DesiredLOD;
                    Chunck->VoxelChunck->GenerateAsyncGreedyMesh(1);  // ← on passe le LOD
                    RebuildCount++;
                }
            }
        }
    }

    LastUpdateTime = GetWorld()->GetTimeSeconds();
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
    UE_LOG(LogTemp, Warning, TEXT("RegisterDirtyChunk : %s"), *Coord.ToString());

    if (!VoxelWorld)
    {
        UE_LOG(LogTemp, Error, TEXT("  AChunckManager::RegisterDirtyChunk(FIntVector Coord) → Impossible : VoxelWorld NULL"));
        return;
    }

    if (VoxelWorld->Chuncks.Find(Coord))
    {
        DirtyChuncks.Add(Coord);
        UE_LOG(LogTemp, Warning, TEXT(" RegisterDirtyChunk(FIntVector Coord)  → Chunk %s enfilé (existe dans la map)"), *Coord.ToString());
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

void AChunckManager::FillChunck(EChunkVariant Variant, FIntVector Coord)
{
    FChunckDataStructure* ChunckData = VoxelWorld->Chuncks.Find(Coord);
    if (!ChunckData) return;

   // for (int i = 0; i < ChunckData->Voxels.Num(); i++)
   // {
    //    ChunckData->Voxels[i].Material.Id = 1;   // ← TOUT est solide
    //}

   // UE_LOG(LogTemp, Warning, TEXT("Chunk %s rempli en FULL SOLID pour test"), *Coord.ToString());
    
    if (!VoxelWorld)
    {
        UE_LOG(LogTemp, Error, TEXT("FillChunck → ChunckManager ou VoxelWorld nul !"));
        return;
    }

    if (!ChunckData)
    {
        UE_LOG(LogTemp, Error, TEXT("FillChunck → ChunckData nul !"));
        return;
    }

    for (int x = 0; x < ChunkSize; x++)
    {
        for (int y = 0; y < ChunkSize; y++)
        {
            // Coordonnées globales pour un terrain seamless
            float WorldX = (Coord.X * ChunkSize + x) * 0.08f;  // 0.08 = belle taille de collines
            float WorldY = (Coord.Y * ChunkSize + y) * 0.08f;

            float Noise = FMath::PerlinNoise2D(FVector2D(WorldX, WorldY));

            // Hauteur globale en voxels (pas en unités !)
            int baseHeight = 100; //Niveau moyen du sol
            int Amplitude = 80; //Taille des montagnes
            int GlobalSurfaceHeight = baseHeight + FMath::FloorToInt(Noise * Amplitude);

            for (int z = 0; z < ChunkSize; z++)
            {
                int Index = x + y * ChunkSize + z * ChunkSize * ChunkSize;
                int GlobalZ = Coord.Z * ChunkSize + z;

                ChunckData->Voxels[Index].Material.Id = (GlobalZ < GlobalSurfaceHeight) ? 1 : 0;
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Chunk %s rempli avec terrain (surface ~%d)"), *Coord.ToString(), 12);
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
    FillChunck(EChunkVariant::Full, Coord);
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
        DirtyChuncks.Add(Coord);
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
    UE_LOG(LogTemp, Warning, TEXT("AChunckManager::UpdateVisibleChunks(const TSet<FIntVector>& ChunksToKeep) --> size: %d"), ChunksToKeep.Num());
    TArray<FIntVector> SortedChunks = ChunksToKeep.Array();

    APawn* Pawn = GetWorld()->GetFirstPlayerController()->GetPawn();
    FVector PlayerPos = Pawn->GetActorLocation();

    SortedChunks.Sort([&](const FIntVector& A, const FIntVector& B)
        {
            FVector PosA = FVector(A) * ChunkSize * VoxelSize;
            FVector PosB = FVector(B) * ChunkSize * VoxelSize;

            return FVector::DistSquared(PlayerPos, PosA) < FVector::DistSquared(PlayerPos, PosB);
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


