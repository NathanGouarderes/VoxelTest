// Fill out your copyright notice in the Description page of Project Settings.


#include "VoxelChunck.h"
#include "ChunckManager.h"
#include "Kismet/GameplayStatics.h"
#include "FChunckDataStructure.h"
#include "VoxelWorld.h"
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
    RealtimeMeshSimple = RealtimeMeshComponent->InitializeRealtimeMesh<URealtimeMeshSimple>();
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

void AVoxelChunck::ApplyMesh(const FChunckMeshData& MeshData)
{
	// 1. Vérification de base du composant
	if (!ProceduralMeshComponent || bIsBeingDestroyed)
	{
		return;
	}

	// 2. Sécurité : Nettoyage si mesh vide
	if (MeshData.Vertices.Num() == 0)
	{
		ProceduralMeshComponent->ClearMeshSection(0);
		bIsDirty = true; // On marque comme sale car rien n'est affiché
		return;
	}

	// 3. Sécurité Renderer : On s'assure que le monde est valide
	UWorld* World = GetWorld();
	if (!World) return;

	// 4. Calcul de collision (simplifié)
	// Au lieu de chercher le PlayerController ici, utilise une variable 
	// ou passe l'info depuis le Manager si possible. 
	// Sinon, garde cette logique mais avec IsValid()
	bool bCreateCollision = false;
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			// Note: Utilise ta variable membre 'Coord' et les constantes 
			// pour éviter de toucher au ChunckManager
			float ChunkScale = 100.0f; // Remplace par une valeur fixe ou membre
			if (ChunckManager) ChunkScale = ChunckManager->VoxelSize;

			FVector ChunkWorldPos = GetActorLocation();
			bCreateCollision = FVector::DistSquared(ChunkWorldPos, Pawn->GetActorLocation()) < FMath::Square(6000.0f);
		}
	}

	// 5. APPLICATION CRITIQUE	
	/*
	if (ProceduralMeshComponent->GetNumSections() > 0)
	{
		ProceduralMeshComponent->UpdateMeshSection(
			0,
			MeshData.Vertices,
			MeshData.Normals,
			MeshData.UVs,
			MeshData.VertexColors,
			MeshData.Tangents
		);
	}
	else
	{
	*/
	
		ProceduralMeshComponent->CreateMeshSection(
			0,
			MeshData.Vertices,
			MeshData.Triangles,
			MeshData.Normals,
			MeshData.UVs,
			MeshData.VertexColors,
			MeshData.Tangents,
			bCreateCollision
		);
	//}

	ProceduralMeshComponent->SetMeshSectionVisible(0, true);

	// 6. Finalisation
	bIsDirty = false;
	bIsQueued = false;
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


void AVoxelChunck::RemoveVoxel(int X, int Y, int Z)
{
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
	if (!ChunckManager || !ChunckManager->VoxelWorld)
	{
		UE_LOG(LogTemp, Error, TEXT(" AVoxelChunck::RemoveVoxel(int X, int Y, int Z) --> ChunckManager ou VoxelWorld invalide"));
		return;
	}
	FChunckDataStructure* ChunckData = ChunckManager->VoxelWorld->Chuncks.Find(Coord);
	if (!ChunckData)
	{
		UE_LOG(LogTemp, Error, TEXT("AVoxelChunck::RemoveVoxel(int X, int Y, int Z): ChunckData non trouvé pour coord %s"), *Coord.ToString());
	}
	int index = X + Y * Size + Z * Size * Size;
	ChunckData->Voxels[index].Material.Id = 0;

	bIsDirty = true;
	ChunckManager->RegisterDirtyChunk(Coord);
	if (X == 0) ChunckManager->RegisterDirtyChunk(Coord + FIntVector(-1, 0, 0));
	if (X == Size - 1) ChunckManager->RegisterDirtyChunk(Coord + FIntVector(1, 0, 0));

	if (Y == 0) ChunckManager->RegisterDirtyChunk(Coord + FIntVector(0, -1, 0));
	if (Y == Size - 1) ChunckManager->RegisterDirtyChunk(Coord + FIntVector(0, 1, 0));

	if (Z == 0) ChunckManager->RegisterDirtyChunk(Coord + FIntVector(0, 0, -1));
	if (Z == Size - 1) ChunckManager->RegisterDirtyChunk(Coord + FIntVector(0, 0, 1));	
}
