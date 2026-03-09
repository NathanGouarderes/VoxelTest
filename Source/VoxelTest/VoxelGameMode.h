// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ChunckManager.h"
#include "VoxelGameMode.generated.h"

/**
 * 
 */
UCLASS()
class VOXELTEST_API AVoxelGameMode : public AGameModeBase
{
	GENERATED_BODY()

	AVoxelGameMode();
	void BeginPlay() override;
	void Generate20Chuncks();

	void Tick(float DeltaTime) override;

	AChunckManager* ChunckManager;
	
};
