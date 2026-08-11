// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EVoxelAxis.h"
#include "RealtimeMeshSimple.h"
#include "Math/IntVector.h"
#include "FChunckMeshData.h"
#include "FVoxelDataStructure.h"
#include "FMask.h"
#include "VoxelChunck.generated.h"

class AChunckManager;


UCLASS()
class VOXELMODULE_API AVoxelChunck : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AVoxelChunck();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;	
	float GetVoxelSize();
	void ApplyMesh(FChunckMeshData&& MeshData);



	void RemoveVoxel(int X, int Y, int Z);
	bool IsVoxelSolid(int x, int y, int z);

	//bool IsFaceVisible(int X, int Y, int Z, bool IsPositiveDirection);
	//void GreedyDirection(Axis axis, bool positive);

	void SetChunckManager(AChunckManager* Manager);


	int32 CurrentLOD;
	bool bIsDirty;
	int32 Size;
	float VoxelSize;
	FChunckMeshData ChunckDataMesh;
	UPROPERTY()
	AChunckManager* ChunckManager;
	FIntVector Coord;
	bool bIsQueued;
	bool bIsBeingDestroyed = false;
	UPROPERTY(VisibleAnywhere, Category = "Voxel")
	TObjectPtr<URealtimeMeshComponent> RealtimeMeshComponent;

	UPROPERTY(Transient)
	TObjectPtr<URealtimeMeshSimple> RealtimeMeshSimple;

	UPROPERTY(EditDefaultsOnly, Category = "Voxel")
	TObjectPtr<UMaterialInterface> TerrainMaterial;

	bool bHasSectionGroup = false;
	//TArray<FVoxelDataStructure> VoxelData;


};
