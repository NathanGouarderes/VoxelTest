// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "EVoxelAxis.h"
#include "FChunckMeshData.h"
#include "FVoxelDataStructure.h"
#include "VoxelChunck.generated.h"

class AChunckManager;

enum class EChunkVariant
{
	Full,       // 100% solides
	HalfFull,   // 50% random solides
	Sparse,     // 20% solides avec clusters
	Empty,      // 0% (pour tests)
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
	void FillChunck(EChunkVariant Variant);
	void GenerateMesh();
	void GenerateFacedMesh();
	void GenerateGreedyMesh(FChunckMeshData& MeshData);
	void GenerateAsyncGreedyMesh();
	void ApplyMesh(const FChunckMeshData& MeshData);

	void GenerateGreedyXPlan(bool IsPositive, FChunckMeshData& MeshData);
	void BuildMaskXPositive(int x, TArray<bool>& Mask);
	void GenerateGreedyXPositive(FChunckMeshData& MeshData);
	void Greedy2DX(int x, TArray<bool>& Mask, bool IsBositive, FChunckMeshData& MeshData);
	void AddQuadXPositive(int x, int y, int z, int width, int height, FChunckMeshData& MeshData);

	void BuildMaskXNegative(int x, TArray<bool>& Mask);
	void GenerateGreedyXNegative(FChunckMeshData& MeshData);
	void AddQuadXNegative(int x, int y, int z, int width, int height, FChunckMeshData& MeshData);

	void GenerateGreedyYPositive(FChunckMeshData& MeshData);
	void GenerateGreedyYPlan(bool IsPositive, FChunckMeshData& MeshData);
	void AddQuadYPositive(int x, int y, int z, int width, int height, FChunckMeshData& MeshData);
	void Greedy2DY(int y, TArray<bool>& Mask, bool IsPositive, FChunckMeshData& MeshData);
	void BuildMaskYPositive(int y, TArray<bool>& Mask);
	
	void GenerateGreedyYNegative(FChunckMeshData& MeshData);
	void AddQuadYNegative(int x, int y, int z, int width, int height, FChunckMeshData& MeshData);
	void BuildMaskYNegative(int y, TArray<bool>& Mask);

	void GenerateGreedyZPlan(bool IsPositive, FChunckMeshData& MeshData);
	void BuildMaskZPositive(int x, TArray<bool>& Mask);
	void GenerateGreedyZPositive(FChunckMeshData& MeshData);
	void Greedy2DZ(int x, TArray<bool>& Mask, bool IsBositive, FChunckMeshData& MeshData);
	void AddQuadZPositive(int x, int y, int z, int width, int height, FChunckMeshData& MeshData);

	void GenerateGreedyZNegative(FChunckMeshData& MeshData);
	void AddQuadZNegative(int x, int y, int z, int width, int height, FChunckMeshData& MeshData);
	void BuildMaskZNegative(int y, TArray<bool>& Mask);

	void AddCube(FVector Position);
	void AddRightFace(FVector Position);
	void AddTopFace(FVector Position);
	void AddBackFace(FVector Position);
	void AddBottomFace(FVector Position);
	void AddLeftFace(FVector Position);
	void AddFrontFace(FVector Position);


	void RemoveVoxel(int X, int Y, int Z);
	bool IsVoxelSolid(int x, int y, int z);
	//bool IsFaceVisible(int X, int Y, int Z, bool IsPositiveDirection);
	//void GreedyDirection(Axis axis, bool positive);

	void SetChunckManager(AChunckManager* Manager);
	void SetVoxel(int32 X, int32 Y, int32 Z, FVoxelDataStructure VoxelData);


	bool bIsDirty;
	int8 Size;
	TArray<int8> VoxelValues;
	UProceduralMeshComponent* ProceduralMeshComponent;
	float VoxelSize;
	FChunckMeshData ChunckDataMesh;
	AChunckManager* ChunckManager;


};
