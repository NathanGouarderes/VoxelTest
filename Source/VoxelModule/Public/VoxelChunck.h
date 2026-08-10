// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "EVoxelAxis.h"
#include "Math/IntVector.h"
#include "FChunckMeshData.h"
#include "FVoxelDataStructure.h"
#include "VoxelChunck.generated.h"

class AChunckManager;

struct FMask
{
	int8 Block = 0;
	int8 Normal = 0;
};


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
	//void GenerateFacedMesh();
	void GenerateAsyncGreedyMesh(int32 InLOD = 0);
	void ApplyMesh(const FChunckMeshData& MeshData);

	void RemoveVoxel(int X, int Y, int Z);
	bool IsVoxelSolid(int x, int y, int z);

	//bool IsFaceVisible(int X, int Y, int Z, bool IsPositiveDirection);
	//void GreedyDirection(Axis axis, bool positive);

	void SetChunckManager(AChunckManager* Manager);


	int32 CurrentLOD;
	bool bIsDirty;
	int8 Size;
	UPROPERTY(VisibleAnywhere) TObjectPtr<URealtimeMeshComponent> RealtimeMeshComponent;
  	UPROPERTY(Transient) TObjectPtr<URealtimeMeshSimple>    RealtimeMeshSimple;
	float VoxelSize;
	FChunckMeshData ChunckDataMesh;
	AChunckManager* ChunckManager;
	FIntVector Coord;
	bool bIsQueued;
	bool bIsBeingDestroyed = false;
	//TArray<FVoxelDataStructure> VoxelData;


};
