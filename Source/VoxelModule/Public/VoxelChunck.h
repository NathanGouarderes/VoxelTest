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
	void GenerateGreedyMesh(FChunckMeshData& MeshData, const TArray<FVoxelDataStructure>& LocalVoxels);
	void GenerateAsyncGreedyMesh(int32 InLOD = 0);
	void ApplyMesh(const FChunckMeshData& MeshData);
	void CreateQuad(const FMask& Mask, const FIntVector& AxisMask, int32 Width, int32 Height, const FIntVector& V1, const FIntVector& V2, const FIntVector& V3, const FIntVector& V4, int32& VertexCount, FChunckMeshData& MeshData);
	bool CompareMask(const FMask& M1, const FMask& M2) const;

	void AddQuadXPositive(int x, int y, int z, int width, int height, FChunckMeshData& MeshData);
	void AddQuadXNegative(int x, int y, int z, int width, int height, FChunckMeshData& MeshData);
	void AddQuadYPositive(int x, int y, int z, int width, int height, FChunckMeshData& MeshData);
	void AddQuadYNegative(int x, int y, int z, int width, int height, FChunckMeshData& MeshData);
	void AddQuadZPositive(int x, int y, int z, int width, int height, FChunckMeshData& MeshData);
	void AddQuadZNegative(int x, int y, int z, int width, int height, FChunckMeshData& MeshData);
	void AddQuad(FVector P0, FVector P1, FVector P2, FVector P3, FVector Normal, FChunckMeshData& MeshData);



	void RemoveVoxel(int X, int Y, int Z);
	bool IsVoxelSolid(int x, int y, int z);
	bool IsVoxelSolidLocal(int x, int y, int z, const TArray<FVoxelDataStructure>& LocalVoxels);

	//bool IsFaceVisible(int X, int Y, int Z, bool IsPositiveDirection);
	//void GreedyDirection(Axis axis, bool positive);

	void SetChunckManager(AChunckManager* Manager);


	int32 CurrentLOD;
	bool bIsDirty;
	int8 Size;
	UProceduralMeshComponent* ProceduralMeshComponent;
	float VoxelSize;
	FChunckMeshData ChunckDataMesh;
	AChunckManager* ChunckManager;
	FIntVector Coord;
	bool bIsQueued;
	bool bIsBeingDestroyed = false;
	//TArray<FVoxelDataStructure> VoxelData;


};
