// Fill out your copyright notice in the Description page of Project Settings.


#include "ChunckManager.h"

// Sets default values
AChunckManager::AChunckManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	MaxRebuildPerFrame = 2;

}

// Called when the game starts or when spawned
void AChunckManager::BeginPlay()
{
	Super::BeginPlay();	
    UE_LOG(LogTemp, Warning, TEXT("Spawn du ChunckManager"));
}

// Called every frame
void AChunckManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    int RebuildCount = 0;
    AVoxelChunck* Chunck;  // <-- Déclaration de la variable locale
    while (!DirtyChuncks.IsEmpty() && RebuildCount < MaxRebuildPerFrame)
    {
        DirtyChuncks.Dequeue(Chunck);  // Utilisation de la variable
        if (Chunck)
        {
            UE_LOG(LogTemp, Warning, TEXT("AChunckManager::Tick(float DeltaTime) : Rebuild de %d chunck"), RebuildCount);
            Chunck->GenerateFacedMesh();
            Chunck->bIsDirty = false;
        }
        RebuildCount++;
    }

}

void AChunckManager::RegisterDirtyChunk(AVoxelChunck* Chunck)
{
	DirtyChuncks.Enqueue(Chunck);
}


