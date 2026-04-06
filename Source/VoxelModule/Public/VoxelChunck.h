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
	void GenerateMesh();
	//void GenerateFacedMesh();
	void GenerateGreedyMesh(FChunckMeshData& MeshData, const TArray<FVoxelDataStructure>& LocalVoxels, int32 InLOD = 0);
	void GenerateAsyncGreedyMesh(int32 InLOD = 0);
	void ApplyMesh(const FChunckMeshData& MeshData);

	void AddQuadXPositive(int x, int y, int z, int width, int height, FChunckMeshData& MeshData);
	void AddQuadXNegative(int x, int y, int z, int width, int height, FChunckMeshData& MeshData);
	void AddQuadYPositive(int x, int y, int z, int width, int height, FChunckMeshData& MeshData);
	void AddQuadYNegative(int x, int y, int z, int width, int height, FChunckMeshData& MeshData);
	void AddQuadZPositive(int x, int y, int z, int width, int height, FChunckMeshData& MeshData);
	void AddQuadZNegative(int x, int y, int z, int width, int height, FChunckMeshData& MeshData);
	void AddQuad(FVector P0, FVector P1, FVector P2, FVector P3, FVector Normal, FChunckMeshData& MeshData);

	void GenerateGreedyXPlan(bool IsPositive, FChunckMeshData& MeshData, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step);
	void GenerateGreedyYPlan(bool IsPositive, FChunckMeshData& MeshData, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step);
	void GenerateGreedyZPlan(bool IsPositive, FChunckMeshData& MeshData, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step);

	void GenerateGreedyXPositive(FChunckMeshData& MeshData, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step);
	void GenerateGreedyXNegative(FChunckMeshData& MeshData, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step);
	void GenerateGreedyYPositive(FChunckMeshData& MeshData, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step);
	void GenerateGreedyYNegative(FChunckMeshData& MeshData, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step);
	void GenerateGreedyZPositive(FChunckMeshData& MeshData, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step);
	void GenerateGreedyZNegative(FChunckMeshData& MeshData, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step);

	void BuildMaskXPositive(int x, TArray<bool>& Mask, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step);
	void BuildMaskXNegative(int x, TArray<bool>& Mask, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step);
	void BuildMaskYPositive(int y, TArray<bool>& Mask, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step);
	void BuildMaskYNegative(int y, TArray<bool>& Mask, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step);
	void BuildMaskZPositive(int z, TArray<bool>& Mask, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step);
	void BuildMaskZNegative(int z, TArray<bool>& Mask, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step);

	void Greedy2DX(int x, TArray<bool>& Mask, bool IsBositive, FChunckMeshData& MeshData, int32 Step);
	void Greedy2DY(int y, TArray<bool>& Mask, bool IsPositive, FChunckMeshData& MeshData, int32 Step);
	void Greedy2DZ(int z, TArray<bool>& Mask, bool IsPositive, FChunckMeshData& MeshData, int32 Step);

	void AddCube(FVector Position);
	void AddRightFace(FVector Position);
	void AddTopFace(FVector Position);
	void AddBackFace(FVector Position);
	void AddBottomFace(FVector Position);
	void AddLeftFace(FVector Position);
	void AddFrontFace(FVector Position);


	void RemoveVoxel(int X, int Y, int Z);
	bool IsVoxelSolid(int x, int y, int z);
	bool IsVoxelSolidLocal(int x, int y, int z, const TArray<FVoxelDataStructure>& LocalVoxels);

	//bool IsFaceVisible(int X, int Y, int Z, bool IsPositiveDirection);
	//void GreedyDirection(Axis axis, bool positive);

	void SetChunckManager(AChunckManager* Manager);


	int32 CurrentLOD = 0;
	bool bIsDirty;
	int8 Size;
	UProceduralMeshComponent* ProceduralMeshComponent;
	float VoxelSize;
	FChunckMeshData ChunckDataMesh;
	AChunckManager* ChunckManager;
	FIntVector Coord;
	//TArray<FVoxelDataStructure> VoxelData;


};
