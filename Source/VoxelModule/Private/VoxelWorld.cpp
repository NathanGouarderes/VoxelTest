// Fill out your copyright notice in the Description page of Project Settings.


#include "VoxelWorld.h"
//#include "FChunkMeshResult.h"
#include "FChunckMeshData.h"
#include "ChunckManager.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
AVoxelWorld::AVoxelWorld()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	ChunckManager = nullptr;

}

// Called when the game starts or when spawned
// Called when the game starts or when spawned
void AVoxelWorld::BeginPlay()
{
    Super::BeginPlay();

    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AChunckManager::StaticClass(), Found);

    if (Found.Num() > 1)
    {
        UE_LOG(LogTemp, Error, TEXT("%d ChunckManager dans le monde — il n'en faut qu'un."), Found.Num());
    }

    ChunckManager = Found.Num() > 0
        ? Cast<AChunckManager>(Found[0])
        : GetWorld()->SpawnActor<AChunckManager>(FVector::ZeroVector, FRotator::ZeroRotator);

    if (ChunckManager) ChunckManager->VoxelWorld = this;

    TArray<AActor*> DbgW; UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVoxelWorld::StaticClass(), DbgW);
    UE_LOG(LogTemp, Warning, TEXT("BeginPlay — %d VoxelWorld"), DbgW.Num());
    for (AActor* A : Found)   // Found = ta liste de managers, déjà remplie juste avant
        UE_LOG(LogTemp, Warning, TEXT("   manager %s (placé=%d)"), *A->GetName(), A->IsNetStartupActor());
}

// Called every frame
void AVoxelWorld::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AVoxelWorld::GenerateWorld()
{
    
    int WorldSizeX = 10;
    int WorldSizeY = 10;
    int WorldSizeZ = 10;
    for (int x = 0; x < WorldSizeX; x++)
    {
        for (int y = 0; y < WorldSizeY; y++)
        {
            for (int z = 0; z < WorldSizeZ; z++)
            {
                FIntVector Coord(x, y, z);
                FChunckDataStructure ChunckData;

                Chuncks.Add(Coord, ChunckData);

                UE_LOG(LogTemp, Warning, TEXT("AVoxelWorld::GenerateWorld() --> Coordonees : %d, %d, %d"), x, y, z);

                if (ChunckManager)
                {
                    //ChunckManager->SpawnChunk(Coord);
                }
            }
        }
    }
    
}


FORCEINLINE int AVoxelWorld::GetVoxelIndex(int x, int y, int z)
{
	return x + ChunckSize * (y + ChunckSize * z);
}

FVoxelDataStructure& AVoxelWorld::GetVoxel(FChunckDataStructure& Chunk, int x, int y, int z)
{
	return Chunk.Voxels[GetVoxelIndex(x, y, z)];
}

void AVoxelWorld::ProcessDirtyChunks()
{
    UE_LOG(LogTemp, Warning, TEXT("ProcessDirtyChunks : marquage de tous les chunks dirty"));

    for (auto& Pair : Chuncks)
    {
        ChunckManager->RegisterDirtyChunk(Pair.Key);
    }
}
