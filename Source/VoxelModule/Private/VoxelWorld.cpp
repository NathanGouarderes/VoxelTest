// Fill out your copyright notice in the Description page of Project Settings.


#include "VoxelWorld.h"
//#include "FChunkMeshResult.h"
#include "FChunckMeshData.h"
#include "ChunckManager.h"


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

    if (!ChunckManager)
    {
        ChunckManager = GetWorld()->SpawnActor<AChunckManager>(FVector::ZeroVector, FRotator::ZeroRotator);
        UE_LOG(LogTemp, Warning, TEXT("AVoxelWorld::BeginPlay() : ChunckManager Cree"));
        if (ChunckManager)
        {
            UE_LOG(LogTemp, Warning, TEXT("AVoxelWorld::BeginPlay() : ChunckManager valide"));
            ChunckManager->VoxelWorld = this;
            UE_LOG(LogTemp, Warning, TEXT("AVoxelWorld::BeginPlay() : ChunckManager spawné et LIÉ avec succès !"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("AVoxelWorld::BeginPlay() : ÉCHEC spawn du ChunckManager"));
        }
    }
    //GenerateWorld();


    // Optionnel : pour tester tout de suite la queue
    // ProcessDirtyChunks();   // décommente si tu veux voir RegisterDirtyChunk
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
                    ChunckManager->SpawnChunk(Coord);
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
