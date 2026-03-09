// Fill out your copyright notice in the Description page of Project Settings.


#include "VoxelGameMode.h"
#include "Components/PlayerControllerComponent.h"
#include "ChunckManager.h"
#include "VoxelChunck.h"

AVoxelGameMode::AVoxelGameMode()
{
	PlayerControllerClass = APlayerControllerComponent::StaticClass();
}

void AVoxelGameMode::BeginPlay()
{
	Super::BeginPlay();
	ChunckManager = GetWorld()->SpawnActor<AChunckManager>(FVector::ZeroVector, FRotator::ZeroRotator);
	//Generate20Chuncks();
}

void AVoxelGameMode::Tick(float DeltaTime)
{

}

void AVoxelGameMode::Generate20Chuncks()
{
	float Size = 10 * 16;
	int NumberX = 50;
	int NumberY = 4;
	int NumberZ = 11;
	for (int x = -NumberX; x <= NumberX; x++)
	{
		for (int y = -NumberY; y <= NumberY; y++)
		{
			for (int z = -NumberZ; z <= NumberZ; z++)
			{
				FVector Location = FVector(x * Size, y * Size, z * Size); // Espacement de 1000 units
				AVoxelChunck* NewChunck =  GetWorld()->SpawnActor<AVoxelChunck>(Location, FRotator::ZeroRotator);

				if (NewChunck && ChunckManager)
				{
					NewChunck->SetChunckManager(ChunckManager);
				}
			}
		}
	}
}