// Fill out your copyright notice in the Description page of Project Settings.


#include "VoxelGameMode.h"
#include "Components/PlayerControllerComponent.h"
#include "ChunckManager.h"
#include "VoxelChunck.h"

AVoxelGameMode::AVoxelGameMode()
{
	PlayerControllerClass = APlayerControllerComponent::StaticClass();
	static ConstructorHelpers::FClassFinder<APawn> PawnClass(
		TEXT("/Game/Characters/BP_BaseCharacter"));

	if (PawnClass.Succeeded())
	{
		DefaultPawnClass = PawnClass.Class;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("BP_BaseCharacter introuvable"));
	}
}

void AVoxelGameMode::BeginPlay()
{
	Super::BeginPlay();
	ChunckManager = GetWorld()->SpawnActor<AChunckManager>(FVector::ZeroVector, FRotator::ZeroRotator);
	//Generate20Chuncks();
}

void AVoxelGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AVoxelGameMode::Generate20Chuncks()
{
	float Size = 10 * 64;
	int NumberX = 5;
	int NumberY = 4;
	int NumberZ = 1;
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