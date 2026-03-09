// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelChunck.h"
#include "ChunckManager.generated.h"

UCLASS()
class VOXELMODULE_API AChunckManager : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AChunckManager();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	void RegisterDirtyChunk(AVoxelChunck* Chunk);

	TQueue<AVoxelChunck*> DirtyChuncks;
	int MaxRebuildPerFrame;

};
