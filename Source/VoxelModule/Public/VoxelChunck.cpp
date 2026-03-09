// Fill out your copyright notice in the Description page of Project Settings.


#include "VoxelChunck.h"
#include "ChunckManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

// Sets default values
AVoxelChunck::AVoxelChunck()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	Size = 16;
	VoxelValues.SetNum(Size * Size * Size);
	ProceduralMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMeshComponent"));
	RootComponent = ProceduralMeshComponent;
	ProceduralMeshComponent->bUseAsyncCooking = true;
	VoxelSize = 10.0f;
	ChunckManager = nullptr;


}

// Called when the game starts or when spawned
void AVoxelChunck::BeginPlay()
{
	Super::BeginPlay();

	ChunckManager = Cast<AChunckManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AChunckManager::StaticClass())
	);
	FillChunck(EChunkVariant::Full);
	//GenerateMesh();
	//GenerateFacedMesh();
	GenerateAsyncGreedyMesh();
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

void AVoxelChunck::SetVoxel(int32 X, int32 Y, int32 Z, FVoxelDataStructure VoxelData)
{
	
}

void AVoxelChunck::GenerateAsyncGreedyMesh()
{
	Async(EAsyncExecution::ThreadPool, [this]() 
	{
			FChunckMeshData ChunckMeshData;

			GenerateGreedyMesh(ChunckMeshData);

			AsyncTask(ENamedThreads::GameThread, [this, ChunckMeshData = MoveTemp(ChunckMeshData)]()
				{
					ApplyMesh(ChunckMeshData);
				});

	});
}


void AVoxelChunck::GenerateGreedyMesh(FChunckMeshData& MeshData)
{
	//UE_LOG(LogTemp, Warning, TEXT("AVoxelChunck::GenerateGreedyMesh()"));
	MeshData.Vertices.Empty();
	MeshData.Triangles.Empty();
	MeshData.Normals.Empty();
	MeshData.UVs.Empty();
	MeshData.VertexColors.Empty();
	MeshData.Tangents.Empty();

	// Génère pour tous les axes sans reset intermédiaire
	GenerateGreedyXPlan(true, MeshData);  // +X
	GenerateGreedyXPlan(false, MeshData);  // -X
	GenerateGreedyYPlan(true, MeshData);  // +Y
	GenerateGreedyYPlan(false, MeshData);
	GenerateGreedyZPlan(true, MeshData);
	GenerateGreedyZPlan(false, MeshData);

}

void AVoxelChunck::ApplyMesh(const FChunckMeshData& MeshData)
{
	if (!ProceduralMeshComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("AVoxelChunck::ApplyMesh(FChunckMeshData& MeshData) --> ProceduralMeshComponent NULL"));

		return;
	}
	if (MeshData.Vertices.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AVoxelChunck::ApplyMesh(FChunckMeshData& MeshData) --> MeshData.Vertices.Num() == 0"));
		return;
	}
	ProceduralMeshComponent->ClearMeshSection(0);
	ProceduralMeshComponent->CreateMeshSection(
		0,
		MeshData.Vertices,
		MeshData.Triangles,
		MeshData.Normals,
		MeshData.UVs,
		MeshData.VertexColors,
		MeshData.Tangents,
		true  // Collision activée
	);
}

void AVoxelChunck::AddQuadZNegative(int x, int y, int z, int width, int height, FChunckMeshData& MeshData)
{
	int StartIndex = MeshData.Vertices.Num();
	float S = VoxelSize;

	float X = x * S;
	float Y = y * S;
	float Z = z * S;

	FVector P0(X, Y, Z);
	FVector P1(X, Y + height * S, Z);
	FVector P2(X + width * S, Y + height * S, Z);
	FVector P3(X + width * S, Y, Z);

	MeshData.Vertices.Add(P0);
	MeshData.Vertices.Add(P1);
	MeshData.Vertices.Add(P2);
	MeshData.Vertices.Add(P3);

	// triangles inversés
	MeshData.Triangles.Add(StartIndex + 0);
	MeshData.Triangles.Add(StartIndex + 2);
	MeshData.Triangles.Add(StartIndex + 1);

	MeshData.Triangles.Add(StartIndex + 0);
	MeshData.Triangles.Add(StartIndex + 3);
	MeshData.Triangles.Add(StartIndex + 2);

	FVector Normal(0, 0, -1);

	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);

	MeshData.UVs.Add(FVector2D(0, 0));
	MeshData.UVs.Add(FVector2D(0, height));
	MeshData.UVs.Add(FVector2D(width, height));
	MeshData.UVs.Add(FVector2D(width, 0));
}

void AVoxelChunck::BuildMaskZNegative(int z, TArray<bool>& Mask)
{
	for (int y = 0; y < Size; y++)
	{
		for (int x = 0; x < Size; x++)
		{
			bool CurrentSolid = IsVoxelSolid(x, y, z);

			bool NeighborSolid =
				(z - 1 >= 0) ? IsVoxelSolid(x, y, z - 1) : false;

			bool Visible = CurrentSolid && !NeighborSolid;

			int index = y * Size + x;

			Mask[index] = Visible;
		}
	}
}

void AVoxelChunck::AddQuadZPositive(int x, int y, int z, int width, int height, FChunckMeshData& MeshData)
{
	int StartIndex = MeshData.Vertices.Num();
	float S = VoxelSize;

	float X = x * S;
	float Y = y * S;
	float Z = (z + 1) * S;

	FVector P0(X, Y, Z);
	FVector P1(X + width * S, Y, Z);
	FVector P2(X + width * S, Y + height * S, Z);
	FVector P3(X, Y + height * S, Z);

	MeshData.Vertices.Add(P0);
	MeshData.Vertices.Add(P1);
	MeshData.Vertices.Add(P2);
	MeshData.Vertices.Add(P3);

	MeshData.Triangles.Add(StartIndex + 0);
	MeshData.Triangles.Add(StartIndex + 2);
	MeshData.Triangles.Add(StartIndex + 1);

	MeshData.Triangles.Add(StartIndex + 0);
	MeshData.Triangles.Add(StartIndex + 3);
	MeshData.Triangles.Add(StartIndex + 2);

	FVector Normal(0, 0, 1);

	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);

	MeshData.UVs.Add(FVector2D(0, 0));
	MeshData.UVs.Add(FVector2D(width, 0));
	MeshData.UVs.Add(FVector2D(width, height));
	MeshData.UVs.Add(FVector2D(0, height));
}

void AVoxelChunck::Greedy2DZ(int z, TArray<bool>& Mask, bool IsPositive, FChunckMeshData& MeshData)
{
	for (int y = 0; y < Size; y++)
	{
		for (int x = 0; x < Size; x++)
		{
			int index = y * Size + x;

			if (!Mask[index])
				continue;

			int width = 1;

			while (x + width < Size &&
				Mask[y * Size + (x + width)])
			{
				width++;
			}

			int height = 1;
			bool done = false;

			while (y + height < Size && !done)
			{
				for (int k = 0; k < width; k++)
				{
					if (!Mask[(y + height) * Size + (x + k)])
					{
						done = true;
						break;
					}
				}

				if (!done)
					height++;
			}

			for (int dy = 0; dy < height; dy++)
			{
				for (int dx = 0; dx < width; dx++)
				{
					Mask[(y + dy) * Size + (x + dx)] = false;
				}
			}

			if (IsPositive)
			{
				AddQuadZPositive(x, y, z, width, height, MeshData);
			}
			else
			{
				AddQuadZNegative(x, y, z, width, height, MeshData);
			}

			x += width - 1;
		}
	}
}

void AVoxelChunck::GenerateGreedyZPlan(bool IsPositive, FChunckMeshData& MeshData)
{
	if (IsPositive)
	{
		GenerateGreedyZPositive(MeshData);  // Test seulement +X
	}
	else
	{
		GenerateGreedyZNegative(MeshData);  // Test seulement -X
	}
}

void AVoxelChunck::GenerateGreedyZPositive(FChunckMeshData& MeshData)
{
	TArray<bool> Mask;
	Mask.SetNum(Size * Size);

	for (int z = 0; z < Size; z++)
	{
		BuildMaskZPositive(z, Mask);
		Greedy2DZ(z, Mask, true, MeshData);
	}
}
void AVoxelChunck::GenerateGreedyZNegative(FChunckMeshData& MeshData)
{
	TArray<bool> Mask;
	Mask.SetNum(Size * Size);

	for (int z = 0; z < Size; z++)
	{
		BuildMaskZNegative(z, Mask);
		Greedy2DZ(z, Mask, false, MeshData);
	}
}

void AVoxelChunck::BuildMaskZPositive(int z, TArray<bool>& Mask)
{
	for (int y = 0; y < Size; y++)
	{
		for (int x = 0; x < Size; x++)
		{
			bool CurrentSolid = IsVoxelSolid(x, y, z);

			bool NeighborSolid =
				(z + 1 < Size) ? IsVoxelSolid(x, y, z + 1) : false;

			bool Visible = CurrentSolid && !NeighborSolid;

			int index = y * Size + x;

			Mask[index] = Visible;
		}
	}
}

void AVoxelChunck::GenerateGreedyYPositive(FChunckMeshData& MeshData)
{
	TArray<bool> Mask;
	Mask.SetNum(Size * Size);  // Pas besoin d'init false, SetNum met à false par défaut pour bool
	for (int y = 0; y < Size; y++)
	{
		BuildMaskYPositive(y, Mask);
		Greedy2DY(y, Mask, true, MeshData);
	}
}

void AVoxelChunck::BuildMaskYPositive(int y, TArray<bool>& Mask)
{
	for (int i = 0; i < Mask.Num(); i++) Mask[i] = false;  // Reset explicitement
	for (int z = 0; z < Size; z++)  // Outer Z (up)
	{
		for (int x = 0; x < Size; x++)  // Inner X (forward/right, horizontal pour +Y)
		{
			bool CurrentSolid = IsVoxelSolid(x, y, z);
			bool NeighborSolid = (y + 1 < Size) ? IsVoxelSolid(x, y + 1, z) : false;
			bool Visible = CurrentSolid && !NeighborSolid;
			int index = z * Size + x;  // Index: Z row (vertical), X column (horizontal)
			Mask[index] = Visible;
		}
	}
}

void AVoxelChunck::Greedy2DY(int y, TArray<bool>& Mask, bool IsPositive, FChunckMeshData& MeshData)
{
	for (int z = 0; z < Size; z++)  // Scan vertical (Z up)
	{
		for (int x = 0; x < Size; x++)  // Scan horizontal (X forward/right)
		{
			int index = z * Size + x;
			if (!Mask[index])
			{
				continue;
			}
			// Trouver width horizontal (en X)
			int width = 1;
			while (x + width < Size && Mask[z * Size + (x + width)])
			{
				width++;
			}
			// Trouver height vertical (en Z)
			int height = 1;
			bool done = false;  // Initialisé ici !
			while (z + height < Size && !done)
			{
				for (int k = 0; k < width; k++)
				{
					if (!Mask[(z + height) * Size + (x + k)])
					{
						done = true;
						break;
					}
				}
				if (!done)
				{
					height++;
				}
			}
			// Marquer le rectangle comme processé
			for (int dz = 0; dz < height; dz++)
			{
				for (int dx = 0; dx < width; dx++)
				{
					Mask[(z + dz) * Size + (x + dx)] = false;
				}
			}
			// Ajouter le quad fusionné
			if (IsPositive)
			{
				AddQuadYPositive(x, y, z, width, height, MeshData);
			}
			else
			{
				AddQuadYNegative(x, y, z, width, height, MeshData);
			}
			x += width - 1;  // Skip les colonnes processées
		}
	}
}

void AVoxelChunck::AddQuadYPositive(int x, int y, int z, int width, int height, FChunckMeshData& MeshData)
{
	int StartIndex = MeshData.Vertices.Num();
	float S = VoxelSize;
	float Y = (y + 1) * S;  // Face +Y au bord du voxel (fixé en Y)
	float X = x * S;
	float Z = z * S;
	// Vertices : P0 bottom left (low X low Z), P1 bottom right (high X low Z), P2 top right (high X high Z), P3 top left (low X high Z)
	// +X forward (horizontal), +Z up (vertical)
	FVector P0(X, Y, Z);                    // Bottom left (swap pour +Y : Y fixe, extend X/Z)
	FVector P1(X + width * S, Y, Z);       // Bottom right
	FVector P2(X + width * S, Y, Z + height * S);  // Top right
	FVector P3(X, Y, Z + height * S);      // Top left
	MeshData.Vertices.Add(P0);
	MeshData.Vertices.Add(P1);
	MeshData.Vertices.Add(P2);
	MeshData.Vertices.Add(P3);

	// Triangles flipped pour outward normals (+Y)
	MeshData.Triangles.Add(StartIndex + 0);
	MeshData.Triangles.Add(StartIndex + 1);
	MeshData.Triangles.Add(StartIndex + 2);

	MeshData.Triangles.Add(StartIndex + 0);
	MeshData.Triangles.Add(StartIndex + 2);
	MeshData.Triangles.Add(StartIndex + 3);

	// Normales +Y outward
	FVector Normal(0, 1, 0);
	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);

	// UVs pour tiling (U horizontal X/width, V vertical Z/height)
	MeshData.UVs.Add(FVector2D(0, 0));        // P0
	MeshData.UVs.Add(FVector2D(width, 0));    // P1
	MeshData.UVs.Add(FVector2D(width, height));  // P2
	MeshData.UVs.Add(FVector2D(0, height));  // P3
	UE_LOG(LogTemp, Warning, TEXT("AVoxelChunck::AddQuadYPositive(int x, int y, int z, int width, int height) --> Adding quad at Y=%f, width=%d, height=%d"), Y, width, height);
}

void AVoxelChunck::GenerateGreedyYPlan(bool IsPositive, FChunckMeshData& MeshData)
{
	if (IsPositive)
	{
		GenerateGreedyYPositive(MeshData);  // Test seulement +Y
	}
	else
	{
		GenerateGreedyYNegative(MeshData);
	}
}

void AVoxelChunck::AddQuadYNegative(int x, int y, int z, int width, int height, FChunckMeshData& MeshData)
{
	int StartIndex = MeshData.Vertices.Num();
	float S = VoxelSize;

	float X = x * S;
	float Y = y * S;
	float Z = z * S;

	FVector P0(X, Y, Z);
	FVector P1(X + width * S, Y, Z);
	FVector P2(X + width * S, Y, Z + height * S);
	FVector P3(X, Y, Z + height * S);

	MeshData.Vertices.Add(P0);
	MeshData.Vertices.Add(P1);
	MeshData.Vertices.Add(P2);
	MeshData.Vertices.Add(P3);

	// triangles inversés pour Y-
	MeshData.Triangles.Add(StartIndex + 0);
	MeshData.Triangles.Add(StartIndex + 2);
	MeshData.Triangles.Add(StartIndex + 1);

	MeshData.Triangles.Add(StartIndex + 0);
	MeshData.Triangles.Add(StartIndex + 3);
	MeshData.Triangles.Add(StartIndex + 2);

	FVector Normal(0, -1, 0);

	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);

	MeshData.UVs.Add(FVector2D(0, 0));
	MeshData.UVs.Add(FVector2D(width, 0));
	MeshData.UVs.Add(FVector2D(width, height));
	MeshData.UVs.Add(FVector2D(0, height));
}

void AVoxelChunck::BuildMaskYNegative(int y, TArray<bool>& Mask)
{
	for (int z = 0; z < Size; z++)
	{
		for (int x = 0; x < Size; x++)
		{
			bool CurrentSolid = IsVoxelSolid(x, y, z);

			bool NeighborSolid =
				(y - 1 >= 0) ? IsVoxelSolid(x, y - 1, z) : false;

			bool Visible = CurrentSolid && !NeighborSolid;

			int index = z * Size + x;

			Mask[index] = Visible;
		}
	}
}

void AVoxelChunck::GenerateGreedyYNegative(FChunckMeshData& MeshData)
{
	TArray<bool> Mask;
	Mask.SetNum(Size * Size);

	for (int y = 0; y < Size; y++)
	{
		BuildMaskYNegative(y, Mask);
		Greedy2DY(y, Mask, false, MeshData);
	}
}


void AVoxelChunck::GenerateGreedyXPositive(FChunckMeshData& MeshData)
{
	TArray<bool> Mask;
	Mask.SetNum(Size * Size);  // Pas besoin d'init false, SetNum met à false par défaut pour bool
	for (int x = 0; x < Size; x++)
	{
		BuildMaskXPositive(x, Mask);
		Greedy2DX(x, Mask, true, MeshData);
	}
}

void AVoxelChunck::BuildMaskXPositive(int x, TArray<bool>& Mask)
{
	for (int i = 0; i < Mask.Num(); i++) Mask[i] = false;  // Reset explicitement
	for (int z = 0; z < Size; z++)  // Outer Z (up)
	{
		for (int y = 0; y < Size; y++)  // Inner Y (right)
		{
			bool CurrentSolid = IsVoxelSolid(x, y, z);
			bool NeighborSolid = (x + 1 < Size) ? IsVoxelSolid(x + 1, y, z) : false;
			bool Visible = CurrentSolid && !NeighborSolid;
			int index = z * Size + y;  // Index: Z row, Y column
			Mask[index] = Visible;
		}
	}
}

void AVoxelChunck::AddQuadXPositive(int x, int y, int z, int width, int height, FChunckMeshData& MeshData)
{
	int StartIndex = MeshData.Vertices.Num();
	float S = VoxelSize;
	float X = (x + 1) * S;  // Face +X au bord du voxel
	float Y = y * S;
	float Z = z * S;
	// Vertices : P0 bottom left (low Y low Z), P1 bottom right (high Y low Z), P2 top right (high Y high Z), P3 top left (low Y high Z)
	// Pour matcher UE : +Y right, +Z up
	FVector P0(X, Y, Z);                    // Bottom left
	FVector P1(X, Y + width * S, Z);       // Bottom right
	FVector P2(X, Y + width * S, Z + height * S);  // Top right
	FVector P3(X, Y, Z + height * S);      // Top left
	MeshData.Vertices.Add(P0);
	MeshData.Vertices.Add(P1);
	MeshData.Vertices.Add(P2);
	MeshData.Vertices.Add(P3);

	// Triangles flipped pour matcher ton winding précédent (normales outward)
	MeshData.Triangles.Add(StartIndex + 0);
	MeshData.Triangles.Add(StartIndex + 2);
	MeshData.Triangles.Add(StartIndex + 1);
	MeshData.Triangles.Add(StartIndex + 0);
	MeshData.Triangles.Add(StartIndex + 3);
	MeshData.Triangles.Add(StartIndex + 2);

	// Normales +X outward
	FVector Normal(1, 0, 0);
	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);

	// UVs pour tiling (U horizontal Y/width, V vertical Z/height)
	MeshData.UVs.Add(FVector2D(0, 0));        // P0
	MeshData.UVs.Add(FVector2D(width, 0));    // P1
	MeshData.UVs.Add(FVector2D(width, height));  // P2
	MeshData.UVs.Add(FVector2D(0, height));  // P3
}

void AVoxelChunck::GenerateGreedyXPlan(bool IsPositive, FChunckMeshData& MeshData)
{
	if(IsPositive)
	{
		GenerateGreedyXPositive(MeshData);  // Test seulement +X
	}
	else
	{
		GenerateGreedyXNegative(MeshData);  // Test seulement -X
	}
}



void AVoxelChunck::AddQuadXNegative(int x, int y, int z, int width, int height, FChunckMeshData& MeshData)
{
	int StartIndex = MeshData.Vertices.Num();
	float S = VoxelSize;

	float X = x * S;
	float Y = y * S;
	float Z = z * S;

	FVector P0(X, Y, Z);
	FVector P1(X, Y, Z + height * S);
	FVector P2(X, Y + width * S, Z + height * S);
	FVector P3(X, Y + width * S, Z);

	MeshData.Vertices.Add(P0);
	MeshData.Vertices.Add(P1);
	MeshData.Vertices.Add(P2);
	MeshData.Vertices.Add(P3);

	// triangles inversés
	MeshData.Triangles.Add(StartIndex + 0);
	MeshData.Triangles.Add(StartIndex + 2);
	MeshData.Triangles.Add(StartIndex + 1);

	MeshData.Triangles.Add(StartIndex + 0);
	MeshData.Triangles.Add(StartIndex + 3);
	MeshData.Triangles.Add(StartIndex + 2);

	FVector Normal(-1, 0, 0);

	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);

	MeshData.UVs.Add(FVector2D(0, 0));
	MeshData.UVs.Add(FVector2D(0, height));
	MeshData.UVs.Add(FVector2D(width, height));
	MeshData.UVs.Add(FVector2D(width, 0));
}

void AVoxelChunck::BuildMaskXNegative(int x, TArray<bool>& Mask)
{
	for (int z = 0; z < Size; z++)
	{
		for (int y = 0; y < Size; y++)
		{
			bool CurrentSolid = IsVoxelSolid(x, y, z);

			bool NeighborSolid =
				(x - 1 >= 0) ? IsVoxelSolid(x - 1, y, z) : false;

			bool Visible = CurrentSolid && !NeighborSolid;

			int index = z * Size + y;

			Mask[index] = Visible;
		}
	}
}

void AVoxelChunck::Greedy2DX(int x, TArray<bool>& Mask, bool IsBositive, FChunckMeshData& MeshData)
{
	for (int z = 0; z < Size; z++)
	{
		for (int y = 0; y < Size; y++)
		{
			int index = z * Size + y;

			if (!Mask[index])
				continue;

			int width = 1;

			while (y + width < Size &&
				Mask[z * Size + (y + width)])
			{
				width++;
			}

			int height = 1;
			bool done = false;

			while (z + height < Size && !done)
			{
				for (int k = 0; k < width; k++)
				{
					if (!Mask[(z + height) * Size + (y + k)])
					{
						done = true;
						break;
					}
				}

				if (!done)
					height++;
			}

			for (int dz = 0; dz < height; dz++)
			{
				for (int dy = 0; dy < width; dy++)
				{
					Mask[(z + dz) * Size + (y + dy)] = false;
				}
			}

			if (IsBositive)
			{
				AddQuadXPositive(x, y, z, width, height, MeshData);
			}
			else
			{
				AddQuadXNegative(x, y, z, width, height, MeshData);

			}

			y += width - 1;
		}
	}
}

void AVoxelChunck::GenerateGreedyXNegative(FChunckMeshData& MeshData)
{
	TArray<bool> Mask;
	Mask.SetNum(Size * Size);

	for (int x = 0; x < Size; x++)
	{
		BuildMaskXNegative(x, Mask);
		Greedy2DX(x, Mask, false, MeshData);
	}
}



float AVoxelChunck::GetVoxelSize()
{
	return VoxelSize;
}

void AVoxelChunck::FillChunck(EChunkVariant Variant)
{
	for (int z = 0; z < Size; z++)
	{
		for (int y = 0; y < Size; y++)
		{
			for (int x = 0; x < Size; x++)
			{
				int index = x + y * Size + z * Size * Size;
				float Noise = FMath::PerlinNoise3D(FVector(x / (float)Size, y / (float)Size, z / (float)Size));
				switch (Variant)
				{
				case EChunkVariant::Full:
					VoxelValues[index] = 1;
					break;
				case EChunkVariant::HalfFull:
					VoxelValues[index] = (UKismetMathLibrary::RandomFloatInRange(0.0f, 1.0f) > 0.5f) ? 1 : 0;
					break;
				case EChunkVariant::Sparse:
					VoxelValues[index] = (Noise > 0.3f) ? 1 : 0;
					break;
				case EChunkVariant::Empty:
					VoxelValues[index] = 0; 
					break;
				default:
					VoxelValues[index] = 0;
					break;
				}
			}
		}
	}
}

void AVoxelChunck::AddCube(FVector Position)
{
	float S = VoxelSize;
	int32 StartIndex = ChunckDataMesh.Vertices.Num();

	ChunckDataMesh.Vertices.Add(Position + FVector(0, 0, 0));
	ChunckDataMesh.Vertices.Add(Position + FVector(S, 0, 0));
	ChunckDataMesh.Vertices.Add(Position + FVector(S, S, 0));
	ChunckDataMesh.Vertices.Add(Position + FVector(0, S, 0));

	ChunckDataMesh.Vertices.Add(Position + FVector(0, 0, S));
	ChunckDataMesh.Vertices.Add(Position + FVector(S, 0, S));
	ChunckDataMesh.Vertices.Add(Position + FVector(S, S, S));
	ChunckDataMesh.Vertices.Add(Position + FVector(0, S, S));

	int32 Tris[] = {
	  0,1,2, 0,2,3, // Bottom
	  4,6,5, 4,7,6, // Top
	  0,4,5, 0,5,1, // Front
	  1,5,6, 1,6,2, // Right
	  2,6,7, 2,7,3, // Back
	  3,7,4, 3,4,0  // Left
	};

	for (int i = 0; i < 36; i++)
	{
		ChunckDataMesh.Triangles.Add(StartIndex + Tris[i]);
	}
}

bool AVoxelChunck::IsVoxelSolid(int X, int Y, int Z)
{
	if (X < 0 || X >= Size || Y < 0 || Y >= Size || Z < 0 || Z >= Size)
	{
		return false; //On est hors du Chunk
	}

	int index = X + Y * Size + Z * Size * Size;
	return VoxelValues[index] > 0;
}

void AVoxelChunck::AddRightFace(FVector Position)
{
	float S = VoxelSize;
	int32 StartIndex = ChunckDataMesh.Vertices.Num();

	ChunckDataMesh.Vertices.Add(Position + FVector(S, 0, 0));
	ChunckDataMesh.Vertices.Add(Position + FVector(S, S, 0));
	ChunckDataMesh.Vertices.Add(Position + FVector(S, S, S));
	ChunckDataMesh.Vertices.Add(Position + FVector(S, 0, S));

	ChunckDataMesh.Triangles.Add(StartIndex + 0);
	ChunckDataMesh.Triangles.Add(StartIndex + 2);
	ChunckDataMesh.Triangles.Add(StartIndex + 1);

	ChunckDataMesh.Triangles.Add(StartIndex + 0);
	ChunckDataMesh.Triangles.Add(StartIndex + 3);
	ChunckDataMesh.Triangles.Add(StartIndex + 2);

	ChunckDataMesh.UVs.Add(FVector2D(0, 0));
	ChunckDataMesh.UVs.Add(FVector2D(1, 0));
	ChunckDataMesh.UVs.Add(FVector2D(1, 1));
	ChunckDataMesh.UVs.Add(FVector2D(0, 1));

	
}
void AVoxelChunck::AddLeftFace(FVector Position)
{
	float S = VoxelSize;
	int32 StartIndex = ChunckDataMesh.Vertices.Num();

	ChunckDataMesh.Vertices.Add(Position + FVector(0, 0, 0));
	ChunckDataMesh.Vertices.Add(Position + FVector(0, 0, S));
	ChunckDataMesh.Vertices.Add(Position + FVector(0, S, S));
	ChunckDataMesh.Vertices.Add(Position + FVector(0, S, 0));

	
	ChunckDataMesh.Triangles.Add(StartIndex + 0);
	ChunckDataMesh.Triangles.Add(StartIndex + 2);
	ChunckDataMesh.Triangles.Add(StartIndex + 1);

	ChunckDataMesh.Triangles.Add(StartIndex + 0);
	ChunckDataMesh.Triangles.Add(StartIndex + 3);
	ChunckDataMesh.Triangles.Add(StartIndex + 2);

	ChunckDataMesh.UVs.Add(FVector2D(0, 0));
	ChunckDataMesh.UVs.Add(FVector2D(1, 0));
	ChunckDataMesh.UVs.Add(FVector2D(1, 1));
	ChunckDataMesh.UVs.Add(FVector2D(0, 1));
}

void AVoxelChunck::AddFrontFace(FVector Position)
{
	float S = VoxelSize;
	int32 StartIndex = ChunckDataMesh.Vertices.Num();

	ChunckDataMesh.Vertices.Add(Position + FVector(0, S, 0));
	ChunckDataMesh.Vertices.Add(Position + FVector(0, S, S));
	ChunckDataMesh.Vertices.Add(Position + FVector(S, S, S));
	ChunckDataMesh.Vertices.Add(Position + FVector(S, S, 0));

	
	ChunckDataMesh.Triangles.Add(StartIndex + 0);
	ChunckDataMesh.Triangles.Add(StartIndex + 2);
	ChunckDataMesh.Triangles.Add(StartIndex + 1);

	ChunckDataMesh.Triangles.Add(StartIndex + 0);
	ChunckDataMesh.Triangles.Add(StartIndex + 3);
	ChunckDataMesh.Triangles.Add(StartIndex + 2);

	ChunckDataMesh.UVs.Add(FVector2D(0, 0));
	ChunckDataMesh.UVs.Add(FVector2D(1, 0));
	ChunckDataMesh.UVs.Add(FVector2D(1, 1));
	ChunckDataMesh.UVs.Add(FVector2D(0, 1));
}

void AVoxelChunck::AddBackFace(FVector Position)
{
	float S = VoxelSize;
	int32 StartIndex = ChunckDataMesh.Vertices.Num();

	ChunckDataMesh.Vertices.Add(Position + FVector(0, 0, 0));
	ChunckDataMesh.Vertices.Add(Position + FVector(S, 0, 0));
	ChunckDataMesh.Vertices.Add(Position + FVector(S, 0, S));
	ChunckDataMesh.Vertices.Add(Position + FVector(0, 0, S));

	
	ChunckDataMesh.Triangles.Add(StartIndex + 0);
	ChunckDataMesh.Triangles.Add(StartIndex + 2);
	ChunckDataMesh.Triangles.Add(StartIndex + 1);

	ChunckDataMesh.Triangles.Add(StartIndex + 0);
	ChunckDataMesh.Triangles.Add(StartIndex + 3);
	ChunckDataMesh.Triangles.Add(StartIndex + 2);

	ChunckDataMesh.UVs.Add(FVector2D(0, 0));
	ChunckDataMesh.UVs.Add(FVector2D(1, 0));
	ChunckDataMesh.UVs.Add(FVector2D(1, 1));
	ChunckDataMesh.UVs.Add(FVector2D(0, 1));
}

void AVoxelChunck::AddBottomFace(FVector Position)
{
	float S = VoxelSize;
	int32 StartIndex = ChunckDataMesh.Vertices.Num();

	ChunckDataMesh.Vertices.Add(Position + FVector(0, 0, 0));
	ChunckDataMesh.Vertices.Add(Position + FVector(0, S, 0));
	ChunckDataMesh.Vertices.Add(Position + FVector(S, S, 0));
	ChunckDataMesh.Vertices.Add(Position + FVector(S, 0, 0));

	
	ChunckDataMesh.Triangles.Add(StartIndex + 0);
	ChunckDataMesh.Triangles.Add(StartIndex + 2);
	ChunckDataMesh.Triangles.Add(StartIndex + 1);

	ChunckDataMesh.Triangles.Add(StartIndex + 0);
	ChunckDataMesh.Triangles.Add(StartIndex + 3);
	ChunckDataMesh.Triangles.Add(StartIndex + 2);

	ChunckDataMesh.UVs.Add(FVector2D(0, 0));
	ChunckDataMesh.UVs.Add(FVector2D(1, 0));
	ChunckDataMesh.UVs.Add(FVector2D(1, 1));
	ChunckDataMesh.UVs.Add(FVector2D(0, 1));
}
void AVoxelChunck::AddTopFace(FVector Position)
{
	float S = VoxelSize;
	int32 StartIndex = ChunckDataMesh.Vertices.Num();

	ChunckDataMesh.Vertices.Add(Position + FVector(0, 0, S));
	ChunckDataMesh.Vertices.Add(Position + FVector(S, 0, S));
	ChunckDataMesh.Vertices.Add(Position + FVector(S, S, S));
	ChunckDataMesh.Vertices.Add(Position + FVector(0, S, S));

	

	ChunckDataMesh.Triangles.Add(StartIndex + 0);
	ChunckDataMesh.Triangles.Add(StartIndex + 2);
	ChunckDataMesh.Triangles.Add(StartIndex + 1);

	ChunckDataMesh.Triangles.Add(StartIndex + 0);
	ChunckDataMesh.Triangles.Add(StartIndex + 3);
	ChunckDataMesh.Triangles.Add(StartIndex + 2);

	ChunckDataMesh.UVs.Add(FVector2D(0, 0));
	ChunckDataMesh.UVs.Add(FVector2D(1, 0));
	ChunckDataMesh.UVs.Add(FVector2D(1, 1));
	ChunckDataMesh.UVs.Add(FVector2D(0, 1));
}


void AVoxelChunck::GenerateFacedMesh()
{
	ChunckDataMesh.Vertices.Empty();
	ChunckDataMesh.Triangles.Empty();
	ChunckDataMesh.Normals.Empty();
	ChunckDataMesh.UVs.Empty();
	for (int z = 0; z < Size; z++)
	{
		for (int y = 0; y < Size; y++)
		{
			for (int x = 0; x < Size; x++)
			{
				float S = VoxelSize;
				FVector Pos = FVector(x * S, y * S, z * S);
				if (IsVoxelSolid(x, y, z))
				{
					// Face droite
					if (!IsVoxelSolid(x + 1, y, z))
					{
						AddRightFace(Pos);
					}

					// Face gauche
					if (!IsVoxelSolid(x - 1, y, z))
					{
						AddLeftFace(Pos);
					}

					// Face avant
					if (!IsVoxelSolid(x, y + 1, z))
					{
						AddFrontFace(Pos);
					}

					// Face arrière
					if (!IsVoxelSolid(x, y - 1, z))
					{
						AddBackFace(Pos);
					}

					// Face haut
					if (!IsVoxelSolid(x, y, z + 1))
					{
						AddTopFace(Pos);
					}

					// Face bas
					if (!IsVoxelSolid(x, y, z - 1))
					{
						AddBottomFace(Pos);
					}
					//AddCube(Pos);
				}
			}
		}

		ProceduralMeshComponent->CreateMeshSection(
			0,
			ChunckDataMesh.Vertices,
			ChunckDataMesh.Triangles,
			ChunckDataMesh.Normals,
			ChunckDataMesh.UVs,
			ChunckDataMesh.VertexColors,
			ChunckDataMesh.Tangents,
			true
		);
	}
}




void AVoxelChunck::GenerateMesh()
{
	// Réinitialiser les tableaux
	ChunckDataMesh.Vertices.Reset();
	ChunckDataMesh.Triangles.Reset();
	ChunckDataMesh.Normals.Reset();
	ChunckDataMesh.UVs.Reset();

	// Préalouer de la mémoire
	ChunckDataMesh.Vertices.Reserve(2000);
	ChunckDataMesh.Triangles.Reserve(4000);
	ChunckDataMesh.Normals.Reserve(2000);
	ChunckDataMesh.UVs.Reserve(2000);

	// Tableau 3D pour le masque de voxels
	bool VoxelMask[16][16][16] = { false };

	// Remplir le masque
	for (int x = 0; x < Size; x++)
		for (int y = 0; y < Size; y++)
			for (int z = 0; z < Size; z++)
				VoxelMask[x][y][z] = IsVoxelSolid(x, y, z);


	// Créer la section de mesh
	if (ChunckDataMesh.Vertices.Num() > 0)
	{
		ProceduralMeshComponent->CreateMeshSection(
			0,
			ChunckDataMesh.Vertices,
			ChunckDataMesh.Triangles,
			ChunckDataMesh.Normals,
			ChunckDataMesh.UVs,
			TArray<FColor>(),
			TArray<FProcMeshTangent>(),
			true
		);
	}
}


void AVoxelChunck::RemoveVoxel(int X, int Y, int Z)
{
	//Vérification des limites
	if (X < 0 || X >= Size || Y < 0 || Y >= Size ||	Z < 0 || Z >= Size)
	{
		return;
	}

	

	// Si déjà vide on ne fait rien
	if (!IsVoxelSolid(X, Y, Z))
	{
		return;
	}

	int index = X + Y * Size + Z * Size * Size;

	//Suppression
	VoxelValues[index] = 0;
	UE_LOG(LogTemp, Warning, TEXT("AVoxelChunck::RemoveVoxel(int X, int Y, int Z) --> Voxel numero %d supprimé à la position %d, %d, %d"), VoxelValues[index], X, Y, Z);

	if (!bIsDirty)
	{
		bIsDirty = true;
		ChunckManager->RegisterDirtyChunk(this);
	}

	// Reconstruction
	//GenerateFacedMesh();
	GenerateAsyncGreedyMesh();

	
}