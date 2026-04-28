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
	Size = 32;
	//VoxelData.SetNum(Size * Size * Size);
	ProceduralMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMeshComponent"));
	RootComponent = ProceduralMeshComponent;
	ProceduralMeshComponent->bUseAsyncCooking = true;
	VoxelSize = 10.0f;
	ChunckManager = nullptr;
	bIsQueued = false;


}

// Called when the game starts or when spawned
void AVoxelChunck::BeginPlay()
{
	Super::BeginPlay();

	/*
	ChunckManager = Cast<AChunckManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AChunckManager::StaticClass())
	);
	FillChunck(EChunkVariant::Full);
	//GenerateMesh();
	//GenerateFacedMesh();
	GenerateAsyncGreedyMesh();
	*/

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
	//UE_LOG(LogTemp, Warning, TEXT("AVoxelChunck::ApplyMesh(FChunckMeshData& MeshData)"));

	if (!ProceduralMeshComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("AVoxelChunck::ApplyMesh(FChunckMeshData& MeshData) --> ProceduralMeshComponent NULL"));

		return;
	}
	ProceduralMeshComponent->SetMaterial(0, nullptr);
	if (MeshData.Vertices.Num() == 0)
	{
		//UE_LOG(LogTemp, Error, TEXT("AVoxelChunck::ApplyMesh(FChunckMeshData& MeshData) --> MeshData.Vertices.Num() == 0"));
		ProceduralMeshComponent->ClearMeshSection(0);
		return;
	}
	//ProceduralMeshComponent->ClearMeshSection(0);
	//UE_LOG(LogTemp, Warning, TEXT("AVoxelChunck::ApplyMesh(const FChunckMeshData& MeshData) --> Chunk %s | Vertices: %d | Triangles: %d"), *Coord.ToString(), MeshData.Vertices.Num(), MeshData.Triangles.Num() / 3);
	if (MeshData.Vertices.Num() > 60000)
	{
		//UE_LOG(LogTemp, Error, TEXT("AVoxelChunck::ApplyMesh(const FChunckMeshData& MeshData) --> !!! MESH TROP GROS (%d verts) - risque de crash mémoire !!!"), MeshData.Vertices.Num());
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("AVoxelChunck::ApplyMesh(const FChunckMeshData& MeshData) aborted: World is null"));
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("AVoxelChunck::ApplyMesh(const FChunckMeshData& MeshData) aborted: PlayerController null"));
		return;
	}

	FVector ChunkWorldPos = FVector(Coord) * ChunckManager->ChunkSize * ChunckManager->VoxelSize;
	FVector PlayerPos = PC->GetPawn()
		? PC->GetPawn()->GetActorLocation()
		: FVector::ZeroVector;

	bool bCreateCollision = FVector::Dist(ChunkWorldPos, PlayerPos) < 6000.0f;  // 60 mètres
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
	ProceduralMeshComponent->SetMeshSectionVisible(0, true);
	if (MeshData.Vertices.Num() == 0)
	{
		bIsDirty = true;
		return;
	}
	//UE_LOG(LogTemp, Warning, TEXT("Avant ApplyMesh - Vertices = %d | Triangles = %d"), MeshData.Vertices.Num(), MeshData.Triangles.Num());
}

void AVoxelChunck::AddQuad(FVector P0, FVector P1, FVector P2, FVector P3, FVector Normal, FChunckMeshData& MeshData)
{
	int StartIndex = MeshData.Vertices.Num();

	MeshData.Vertices.Add(P0);
	MeshData.Vertices.Add(P1);
	MeshData.Vertices.Add(P2);
	MeshData.Vertices.Add(P3);

	// Calcul du vrai normal géométrique
	FVector Edge1 = P1 - P0;
	FVector Edge2 = P2 - P0;
	FVector TrueNormal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();

	// Toujours même winding (CCW)
	MeshData.Triangles.Add(StartIndex + 0);
	MeshData.Triangles.Add(StartIndex + 2);
	MeshData.Triangles.Add(StartIndex + 1);

	MeshData.Triangles.Add(StartIndex + 0);
	MeshData.Triangles.Add(StartIndex + 3);
	MeshData.Triangles.Add(StartIndex + 2);

	// Normales calculées automatiquement
	for (int i = 0; i < 4; i++)
	{
		MeshData.Normals.Add(TrueNormal);
	}

	// UV
	MeshData.UVs.Add(FVector2D(0, 0));
	MeshData.UVs.Add(FVector2D(1, 0));
	MeshData.UVs.Add(FVector2D(1, 1));
	MeshData.UVs.Add(FVector2D(0, 1));
	
}

void AVoxelChunck::AddQuadZNegative(int x, int y, int z, int width, int height, FChunckMeshData& MeshData)
{
	float S = VoxelSize;
	FVector P0(x * S, y * S, z * S);
	FVector P1((x + width) * S, y * S, z * S);
	FVector P2((x + width) * S, (y + height) * S, z * S);
	FVector P3(x * S, (y + height) * S, z * S);

	AddQuad(P0, P3, P2, P1, FVector(0, 0, -1), MeshData);
}

void AVoxelChunck::AddQuadZPositive(int x, int y, int z, int width, int height, FChunckMeshData& MeshData)
{
	float S = VoxelSize;
	FVector P0(x * S, y * S, (z + 1) * S);
	FVector P1((x + width) * S, y * S, (z + 1) * S);
	FVector P2((x + width) * S, (y + height) * S, (z + 1) * S);
	FVector P3(x * S, (y + height) * S, (z + 1) * S);

	AddQuad(P0, P3, P2, P1, FVector(0, 0, 1), MeshData);
}

void AVoxelChunck::AddQuadYPositive(int x, int y, int z, int width, int height, FChunckMeshData& MeshData)
{
	float S = VoxelSize;
	FVector P0(x * S, (y + 1) * S, z * S);
	FVector P1((x + width) * S, (y + 1) * S, z * S);
	FVector P2((x + width) * S, (y + 1) * S, (z + height) * S);
	FVector P3(x * S, (y + 1) * S, (z + height) * S);

	AddQuad(P0, P3, P2, P1, FVector(0, 1, 0), MeshData);
}

void AVoxelChunck::AddQuadYNegative(int x, int y, int z, int width, int height, FChunckMeshData& MeshData)
{
	float S = VoxelSize;
	FVector P0(x * S, y * S, z * S);
	FVector P1((x + width) * S, y * S, z * S);
	FVector P2((x + width) * S, y * S, (z + height) * S);
	FVector P3(x * S, y * S, (z + height) * S);

	AddQuad(P0, P3, P2, P1, FVector(0, -1, 0), MeshData);
}

void AVoxelChunck::AddQuadXPositive(int x, int y, int z, int width, int height, FChunckMeshData& MeshData)
{
	float S = VoxelSize;
	FVector P0((x + 1) * S, y * S, z * S);
	FVector P1((x + 1) * S, (y + width) * S, z * S);
	FVector P2((x + 1) * S, (y + width) * S, (z + height) * S);
	FVector P3((x + 1) * S, y * S, (z + height) * S);

	AddQuad(P0, P1, P2, P3, FVector(1, 0, 0), MeshData);
}

void AVoxelChunck::AddQuadXNegative(int x, int y, int z, int width, int height, FChunckMeshData& MeshData)
{
	float S = VoxelSize;
	FVector P0(x * S, y * S, z * S);
	FVector P1((x)*S, (y + width) * S, z * S);
	FVector P2((x)*S, (y + width) * S, (z + height) * S);
	FVector P3(x * S, y * S, (z + height) * S);

	AddQuad(P0, P1, P2, P3, FVector(-1, 0, 0), MeshData);
}


void AVoxelChunck::GenerateAsyncGreedyMesh(int32 InLOD /*= 0*/)
{
	if (!IsValid(this))
	{
		return;
	}
	if (!ChunckManager || !ChunckManager->VoxelWorld)
		return;

	const FChunckDataStructure* CD = ChunckManager->VoxelWorld->Chuncks.Find(Coord);
	if (!CD || CD->Voxels.Num() == 0)
		return;

	TArray<FVoxelDataStructure> LocalVoxels = CD->Voxels;

	Async(EAsyncExecution::ThreadPool, [this, LocalVoxels = MoveTemp(LocalVoxels)]() mutable
		{
			FChunckMeshData MeshData;
			GenerateGreedyMesh(MeshData, LocalVoxels);

			AsyncTask(ENamedThreads::GameThread, [this, MeshData = MoveTemp(MeshData)]() mutable
				{
					if (IsValid(this))
					{
						ApplyMesh(MeshData);
					}

					if (ChunckManager)
					{
						ChunckManager->CurrentMeshJob = FMath::Max(0, ChunckManager->CurrentMeshJob - 1);
					}
					if (MeshData.Vertices.Num() == 0)
					{
						ChunckManager->PendingMeshToApply.Enqueue(this);
					}
					bIsQueued = false;
				});
		});
}

void AVoxelChunck::GenerateGreedyMesh(FChunckMeshData& MeshData, const TArray<FVoxelDataStructure>& LocalVoxels)
{
	MeshData.Vertices.Empty();
	MeshData.Triangles.Empty();
	MeshData.Normals.Empty();
	MeshData.UVs.Empty();
	MeshData.VertexColors.Empty();
	MeshData.Tangents.Empty();

	int32 VertexCount = 0;

	// Sweep sur les 3 axes (X, Y, Z)
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const int32 Axis1 = (Axis + 1) % 3;
		const int32 Axis2 = (Axis + 2) % 3;

		FIntVector AxisMask = FIntVector::ZeroValue;
		AxisMask[Axis] = 1;

		TArray<FMask> Mask;
		Mask.SetNum(Size * Size);

		FIntVector ChunkItr = FIntVector::ZeroValue;

		for (ChunkItr[Axis] = -1; ChunkItr[Axis] < Size;)
		{
			int32 N = 0;

			// === 1. Construire le Mask ===
			for (ChunkItr[Axis2] = 0; ChunkItr[Axis2] < Size; ++ChunkItr[Axis2])
			{
				for (ChunkItr[Axis1] = 0; ChunkItr[Axis1] < Size; ++ChunkItr[Axis1])
				{
					const bool CurrentSolid = IsVoxelSolidLocal(ChunkItr.X, ChunkItr.Y, ChunkItr.Z, LocalVoxels);
					const bool CompareSolid = IsVoxelSolidLocal(ChunkItr.X + AxisMask.X,ChunkItr.Y + AxisMask.Y, ChunkItr.Z + AxisMask.Z, LocalVoxels);

					if (CurrentSolid == CompareSolid)
					{
						Mask[N++] = FMask{ 0, 0 };
					}
					else if (CurrentSolid)
					{
						Mask[N++] = FMask{ 1, 1 };
					}
					else
					{
						Mask[N++] = FMask{ 1, -1 };
					}
				}
			}

			++ChunkItr[Axis];
			N = 0;

			// === 2. Générer les quads à partir du Mask ===
			for (int32 j = 0; j < Size; ++j)
			{
				for (int32 i = 0; i < Size;)
				{
					if (Mask[N].Normal != 0)
					{
						const FMask CurrentMask = Mask[N];

						ChunkItr[Axis1] = i;
						ChunkItr[Axis2] = j;

						// Largeur
						int32 Width = 1;
						while (i + Width < Size && CompareMask(Mask[N + Width], CurrentMask))
							++Width;

						// Hauteur
						int32 Height = 1;
						bool Done = false;
						for (; Height + j < Size; ++Height)
						{
							for (int32 k = 0; k < Width; ++k)
							{
								if (!CompareMask(Mask[N + k + Height * Size], CurrentMask))
								{
									Done = true;
									break;
								}
							}
							if (Done) break;
						}

						// === Calcul propre des 4 coins du quad (remplace les ternaires foireux) ===
						FIntVector V1 = ChunkItr;
						FIntVector V2 = V1;  V2[Axis1] += Width;
						FIntVector V3 = V1;  V3[Axis2] += Height;
						FIntVector V4 = V2;  V4[Axis2] += Height;

						// Créer le quad
						CreateQuad(CurrentMask, AxisMask, Width, Height, V1, V2, V3, V4, VertexCount, MeshData);

						// Marquer comme traité
						for (int32 l = 0; l < Height; ++l)
							for (int32 k = 0; k < Width; ++k)
								Mask[N + k + l * Size] = FMask{ 0, 0 };

						i += Width;
						N += Width;
						VertexCount += 4;
					}
					else
					{
						++i;
						++N;
					}
				}
			}
		}
	}
}

void AVoxelChunck::CreateQuad(const FMask& Mask, const FIntVector& AxisMask, int32 Width, int32 Height, const FIntVector& V1, const FIntVector& V2, const FIntVector& V3, const FIntVector& V4, int32& VertexCount, FChunckMeshData& MeshData)
{
	const FVector Normal = FVector(AxisMask) * Mask.Normal * VoxelSize;

	const int32 StartIndex = MeshData.Vertices.Num();

	MeshData.Vertices.Append({ FVector(V1) * VoxelSize, FVector(V2) * VoxelSize,
							   FVector(V3) * VoxelSize, FVector(V4) * VoxelSize });

	// Winding automatique (le fameux trick +Mask.Normal / -Mask.Normal)
	MeshData.Triangles.Append({
		StartIndex + 0,
		StartIndex + 2 + Mask.Normal,
		StartIndex + 2 - Mask.Normal,
		StartIndex + 3,
		StartIndex + 1 - Mask.Normal,
		StartIndex + 1 + Mask.Normal
		});

	MeshData.Normals.Append({ Normal, Normal, Normal, Normal });

	// UVs scalées selon l'axe
	if (AxisMask.X != 0) // faces X
	{
		MeshData.UVs.Append({
			FVector2D(0, 0),
			FVector2D(Height, 0),
			FVector2D(Height, Width),
			FVector2D(0, Width)
			});
	}
	else // faces Y ou Z
	{
		MeshData.UVs.Append({
			FVector2D(0, 0),
			FVector2D(Width, 0),
			FVector2D(Width, Height),
			FVector2D(0, Height)
			});
	}

	VertexCount += 4;
}

bool AVoxelChunck::CompareMask(const FMask& M1, const FMask& M2) const
{
	return M1.Block == M2.Block && M1.Normal == M2.Normal;
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

	const FChunckDataStructure* ChunkData = ChunckManager->VoxelWorld->Chuncks.Find(TargetCoord);
	if (!ChunkData)
		return false;                   // chunk non chargé = plein (correct)

	int index = lx + ly * Size + lz * Size * Size;
	if (!ChunkData->Voxels.IsValidIndex(index))
		return false;

	return ChunkData->Voxels[index].Material.Id > 0;
}

bool AVoxelChunck::IsVoxelSolidLocal(int x, int y, int z, const TArray<FVoxelDataStructure>& LocalVoxels)
{
	if (x < 0 || x >= Size || y < 0 || y >= Size || z < 0 || z >= Size)
		return false;

	int index = x + y * Size + z * Size * Size;

	if (!LocalVoxels.IsValidIndex(index))
		return false;

	return LocalVoxels[index].Material.Id > 0;
}



void AVoxelChunck::RemoveVoxel(int X, int Y, int Z)
{
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