// Fill out your copyright notice in the Description page of Project Settings.


#include "VoxelChunck.h"
#include "ChunckManager.h"
#include "Kismet/GameplayStatics.h"
#include "FChunckDataStructure.h"
#include "VoxelWorld.h"
#include "Interface/Core/RealtimeMeshCollision.h"
#include "RealtimeMeshSimple.h"
#include "RealtimeMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"

// Sets default values
AVoxelChunck::AVoxelChunck()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	Size = 128;
	//VoxelData.SetNum(Size * Size * Size);
	RealtimeMeshComponent = CreateDefaultSubobject<URealtimeMeshComponent>(TEXT("RealtimeMeshComponent"));
	SetRootComponent(RealtimeMeshComponent);
	RealtimeMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	RealtimeMeshComponent->SetCollisionObjectType(ECC_WorldStatic);
	RealtimeMeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	RealtimeMeshComponent->bVisibleInRayTracing = false;
	VoxelSize = 10.0f;
	ChunckManager = nullptr;
	bIsQueued = false;
	CurrentLOD = 0;   
	bIsDirty = true;
}

// Called when the game starts or when spawned
void AVoxelChunck::BeginPlay()
{
	Super::BeginPlay();
	FRealtimeMeshCollisionConfiguration CollisionConfiguration;
	CollisionConfiguration.bUseComplexAsSimpleCollision = true;
	CollisionConfiguration.bUseAsyncCook = true;
	RealtimeMeshSimple = RealtimeMeshComponent->InitializeRealtimeMesh<URealtimeMeshSimple>();
	if (RealtimeMeshSimple)
	{
		RealtimeMeshSimple->SetupMaterialSlot(0, TEXT("Terrain"), TerrainMaterial);
		RealtimeMeshSimple->SetCollisionConfig(CollisionConfiguration);
	}

}


// Called every frame
void AVoxelChunck::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}
void AVoxelChunck::SetChunckManager(AChunckManager* Manager)
{
	ChunckManager = Manager;
}


float AVoxelChunck::GetVoxelSize()
{
	return VoxelSize;
}

bool AVoxelChunck::IsVoxelSolid(int x, int y, int z)
{

	if (!ChunckManager || !ChunckManager->VoxelWorld)
		return false;

	int lx = x, ly = y, lz = z;
	FIntVector Offset(0, 0, 0);

	if (x < 0)
	{ 
		lx += Size;
		Offset.X = -1;
	}
	else if (x >= Size)
	{ 
		lx -= Size;
		Offset.X = 1;
	}

	if (y < 0) { 
		ly += Size;
		Offset.Y = -1;
	}
	else if (y >= Size)
	{ 
		ly -= Size;
		Offset.Y = 1;
	}

	if (z < 0) 
	{ 
		lz += Size;
		Offset.Z = -1;
	}
	else if (z >= Size)
	{ 
		lz -= Size;
		Offset.Z = 1; 
	}

	FIntVector TargetCoord = Coord + Offset;
	
	//{
		//FScopeLock Lock(&ChunckManager->VoxelWorld->ChunckMutex);
		const FChunckDataStructure* ChunkData = ChunckManager->VoxelWorld->Chuncks.Find(TargetCoord);
		if (!ChunkData)
			return false;                   // chunk non chargé = plein (correct)

		int index = lx + ly * Size + lz * Size * Size;
		if (!ChunkData->Voxels.IsValidIndex(index))
			return false;

		return ChunkData->Voxels[index].Material.Id > 0;
	//}
}

void AVoxelChunck::ApplyMesh(FChunckMeshData&& MeshData)
{
	// Le URealtimeMeshSimple est cree au BeginPlay. Sans lui, rien a alimenter.
	if (!RealtimeMeshSimple || bIsBeingDestroyed)
	{
		bIsQueued = false;
		return;
	}

	// Cle deterministe : reconstruite a chaque appel, jamais stockee en membre.
	const FRealtimeMeshSectionGroupKey GroupKey =
		FRealtimeMeshSectionGroupKey::Create(FRealtimeMeshLODKey(0), FName("Chunk"));

	// Chunk sans geometrie : on retire le groupe au lieu de pousser un StreamSet vide.
	// RMC 5.3.2 a un crash connu sur le commit d'un stream set vide (corrige en 5.4).
	if (MeshData.bIsEmpty)
	{
		if (bHasSectionGroup)
		{
			RealtimeMeshSimple->RemoveSectionGroup(GroupKey);
			bHasSectionGroup = false;
		}
		bIsDirty = false;
		bIsQueued = false;
		return;
	}

	if (!bHasSectionGroup)
	{
		// Premier mesh : cree le groupe et les sections associees.
		RealtimeMeshSimple->CreateSectionGroup(GroupKey, MoveTemp(MeshData.Streams));
		const FRealtimeMeshSectionKey SectionKey = FRealtimeMeshSectionKey::CreateForPolyGroup(GroupKey, 0);
		RealtimeMeshSimple->UpdateSectionConfig(SectionKey, FRealtimeMeshSectionConfig(), true);
		bHasSectionGroup = true;
	}
	else
	{
		// Chemin rapide : reutilise l'infrastructure existante au lieu de la recreer.
		RealtimeMeshSimple->UpdateSectionGroup(GroupKey, MoveTemp(MeshData.Streams));
	}

	bIsDirty = false;
	bIsQueued = false;
}

void AVoxelChunck::RemoveVoxel(int X, int Y, int Z)
{
	if (!ChunckManager || !ChunckManager->VoxelWorld)
	{
		UE_LOG(LogTemp, Error, TEXT(" AVoxelChunck::RemoveVoxel(int X, int Y, int Z) --> ChunckManager ou VoxelWorld invalide"));
		return;
	}
	FScopeLock Lock(&ChunckManager->VoxelWorld->ChunckMutex);
	//Vérification des limites
	if (X < 0 || X >= Size || Y < 0 || Y >= Size || Z < 0 || Z >= Size)
	{
		return;
	}
	if (!IsVoxelSolid(X, Y, Z))
	{
		return;
	}
	FIntVector G = Coord * Size + FIntVector(X, Y, Z);
	FVoxelDataStructure VoxelAir;
	VoxelAir.Material.Id = 0;
	ChunckManager->SetVoxelAt(G, VoxelAir);
	/*
	FChunckDataStructure* ChunckData = ChunckManager->VoxelWorld->Chuncks.Find(Coord);
	if (!ChunckData)
	{
		UE_LOG(LogTemp, Error, TEXT("AVoxelChunck::RemoveVoxel(int X, int Y, int Z): ChunckData non trouvé pour coord %s"), *Coord.ToString());
		return;
	}
	int index = X + Y * Size + Z * Size * Size;
	if (FChunkEditLayer* Layer = ChunckManager->VoxelWorld->EditLayers.Find(Coord))
	{
		Layer->Edits.Find(index)->Material.Id = 0;
	}
	ChunckData->Voxels[index].Material.Id = 0;

	bIsDirty = true;
	ChunckManager->RegisterDirtyChunk(Coord);
	if (X == 0) ChunckManager->RegisterDirtyChunk(Coord + FIntVector(-1, 0, 0));
	if (X == Size - 1) ChunckManager->RegisterDirtyChunk(Coord + FIntVector(1, 0, 0));

	if (Y == 0) ChunckManager->RegisterDirtyChunk(Coord + FIntVector(0, -1, 0));
	if (Y == Size - 1) ChunckManager->RegisterDirtyChunk(Coord + FIntVector(0, 1, 0));

	if (Z == 0) ChunckManager->RegisterDirtyChunk(Coord + FIntVector(0, 0, -1));
	if (Z == Size - 1) ChunckManager->RegisterDirtyChunk(Coord + FIntVector(0, 0, 1));
	*/
}