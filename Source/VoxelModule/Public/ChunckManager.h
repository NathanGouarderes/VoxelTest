// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelChunck.h"
#include "FChunckDataStructure.h"
#include "FastNoiseLite.h"
#include "ChunckManager.generated.h"

class AVoxelWorld;

enum class EChunkVariant
{
	Full,       // 100% solides
	HalfFull,   // 50% random solides
	Sparse,     // 20% solides avec clusters
	Empty,      // 0% (pour tests)
};

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
	void RegisterDirtyChunk(FIntVector Coord);
	void SpawnChunk(FIntVector Coord);
	void UpdateVisibleChunks(const TSet<FIntVector>& ChunksToKeep);
	void GetAllPlayerChunks(TSet<FIntVector>& GlobalChunksToKeep);
	FIntVector GetPlayerChunck(const FVector& PlayerPos) const;
	void GenerateTerrain(FChunckDataStructure& Data, FIntVector Coord);
	void FillChunck(EChunkVariant Variant, FIntVector Coord);
	int32 GetLODForChunck(const FIntVector& Coord, const FVector& PlayerPos) const;
	float GetNoise(float WorldX, float WorldY);
	void InitNoise();

	TSet<FIntVector> DirtyChuncks;
	UPROPERTY(EditAnywhere, Category = "Voxel")
	TSubclassOf<AVoxelChunck> VoxelChunckClass;

	UPROPERTY()
	AVoxelWorld* VoxelWorld;
	UPROPERTY(EditAnywhere)
	int VoxelSize = 10;

	UPROPERTY(EditAnywhere)
	int ChunkSize = 32;

	//void UpdateVisibleChunks(const FVector& PlayerLocation);
	int32 HorizontalViewDistance = 10;
	int32 VerticalViewDistance = 10;

	UPROPERTY(EditAnywhere, Category = "Voxel | LOD")
	TArray<float> LODDistances = { 0.0f, 8000.0f, 16000.0f, 32000.0f };
	UPROPERTY(EditAnywhere, Category = "Voxel | LOD")
	int MaxLOD = 3;

	UPROPERTY(EditAnywhere, Category = "Voxel | Spawn")
	float PlayerSpawnHeight = 110.0f;

	UPROPERTY(EditAnywhere, Category = "Voxel | Performance")
	int32 MaxSpawnPerFrame = 400;
	UPROPERTY(EditAnywhere, Category = "Voxel | Performance")
	int MaxRebuildPerFrame = 300;
	float LastUpdateTime = 0.0f;
	bool bForceUpdate = true;
	FIntVector LastPlayerChunk = FIntVector::ZeroValue;
	TMap<APawn*, FIntVector> LastPlayerChunks;
	bool bNeedUpdate;
	TQueue<AVoxelChunck*> PendingMeshToApply;
	TQueue<FIntVector> ChunckGenerationQueue;

	int32 NumThreads;
	int MaxGenPerFrame;

	int32 CurrentMeshJob;
	int32 MaxMeshJob;

	//Bruit
	FastNoiseLite SurfaceNoise;
	FastNoiseLite CaveNoise;
	UPROPERTY(EditAnywhere)
	float SurfaceFrequency;     // 2D → collines larges et naturelles 0.006
	UPROPERTY(EditAnywhere)
    float SurfaceAmplitude;      // hauteur des montagnes
	UPROPERTY(EditAnywhere)
    int   BaseHeight;               // niveau moyen du sol
	UPROPERTY(EditAnywhere)
    float CaveFrequency;        // 3D → taille des grottes
	UPROPERTY(EditAnywhere)
    float CaveThreshold;      // plus bas = plus de grottes
	UPROPERTY(EditAnywhere)
    int   SeaLevel;         // niveau de la mer (lacs + océan)

};
