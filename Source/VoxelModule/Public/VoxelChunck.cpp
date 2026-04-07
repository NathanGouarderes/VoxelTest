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
	if (!ProceduralMeshComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("AVoxelChunck::ApplyMesh(FChunckMeshData& MeshData) --> ProceduralMeshComponent NULL"));

		return;
	}
	ProceduralMeshComponent->SetMaterial(0, nullptr);
	if (MeshData.Vertices.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("AVoxelChunck::ApplyMesh(FChunckMeshData& MeshData) --> MeshData.Vertices.Num() == 0"));
		ProceduralMeshComponent->ClearMeshSection(0);
		return;
	}
	//ProceduralMeshComponent->ClearMeshSection(0);
	UE_LOG(LogTemp, Warning, TEXT("AVoxelChunck::ApplyMesh(const FChunckMeshData& MeshData) --> Chunk %s | Vertices: %d | Triangles: %d"), *Coord.ToString(), MeshData.Vertices.Num(), MeshData.Triangles.Num() / 3);
	if (MeshData.Vertices.Num() > 60000)
	{
		UE_LOG(LogTemp, Error, TEXT("AVoxelChunck::ApplyMesh(const FChunckMeshData& MeshData) --> !!! MESH TROP GROS (%d verts) - risque de crash mémoire !!!"), MeshData.Vertices.Num());
		return;
	}
	FVector ChunkWorldPos = FVector(Coord) * ChunckManager->ChunkSize * ChunckManager->VoxelSize;
	FVector PlayerPos = GetWorld()->GetFirstPlayerController()->GetPawn()
		? GetWorld()->GetFirstPlayerController()->GetPawn()->GetActorLocation()
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
	MeshData.Triangles.Add(StartIndex + 1);
	MeshData.Triangles.Add(StartIndex + 2);

	MeshData.Triangles.Add(StartIndex + 0);
	MeshData.Triangles.Add(StartIndex + 2);
	MeshData.Triangles.Add(StartIndex + 3);

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

	AddQuad(P0, P1, P2, P3, FVector(0, 0, -1), MeshData);
}

void AVoxelChunck::AddQuadZPositive(int x, int y, int z, int width, int height, FChunckMeshData& MeshData)
{
	float S = VoxelSize;
	FVector P0(x * S, y * S, (z + 1) * S);
	FVector P1((x + width) * S, y * S, (z + 1) * S);
	FVector P2((x + width) * S, (y + height) * S, (z + 1) * S);
	FVector P3(x * S, (y + height) * S, (z + 1) * S);

	AddQuad(P0, P1, P2, P3, FVector(0, 0, 1), MeshData);
}

void AVoxelChunck::AddQuadYPositive(int x, int y, int z, int width, int height, FChunckMeshData& MeshData)
{
	float S = VoxelSize;
	FVector P0(x * S, (y + 1) * S, z * S);
	FVector P1((x + width) * S, (y + 1) * S, z * S);
	FVector P2((x + width) * S, (y + 1) * S, (z + height) * S);
	FVector P3(x * S, (y + 1) * S, (z + height) * S);

	AddQuad(P0, P1, P2, P3, FVector(0, 1, 0), MeshData);
}

void AVoxelChunck::AddQuadYNegative(int x, int y, int z, int width, int height, FChunckMeshData& MeshData)
{
	float S = VoxelSize;
	FVector P0(x * S, y * S, z * S);
	FVector P1((x + width) * S, y * S, z * S);
	FVector P2((x + width) * S, y * S, (z + height) * S);
	FVector P3(x * S, y * S, (z + height) * S);

	AddQuad(P0, P1, P2, P3, FVector(0, -1, 0), MeshData);
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
	if (!ChunckManager || !ChunckManager->VoxelWorld)
		return;

	const FIntVector CurrentCoord = Coord;
	const int32 LOD = InLOD;

	const FChunckDataStructure* CD = ChunckManager->VoxelWorld->Chuncks.Find(CurrentCoord);
	if (!CD)
	{
		UE_LOG(LogTemp, Error, TEXT("AVoxelChunck::GenerateAsyncGreedyMesh(): CD non trouvé pour %s"), *CurrentCoord.ToString());
		return;
	}

	TArray<FVoxelDataStructure> LocalVoxels = CD->Voxels;

	Async(EAsyncExecution::ThreadPool, [this, LocalVoxels = MoveTemp(LocalVoxels), CurrentCoord, LOD]() mutable
		{
			FChunckMeshData MeshData;
			GenerateGreedyMesh(MeshData, LocalVoxels, LOD);

			AsyncTask(ENamedThreads::GameThread, [this, MeshData = MoveTemp(MeshData)]()
				{
					ApplyMesh(MeshData);
				});
			//UE_LOG(LogTemp, Warning, TEXT("GenerateGreedyMesh terminé pour chunk %s | LOD=%d | Vertices générés = %d"), *CurrentCoord.ToString(), LOD, MeshData.Vertices.Num());
		});

}

void AVoxelChunck::GenerateGreedyMesh(FChunckMeshData& MeshData, const TArray<FVoxelDataStructure>& LocalVoxels, int32 LOD /*= 0*/)
{
	MeshData.Vertices.Empty();
	MeshData.Triangles.Empty();
	MeshData.Normals.Empty();
	MeshData.UVs.Empty();
	MeshData.VertexColors.Empty();
	MeshData.Tangents.Empty();

	int32 Step = 1 << LOD;  // LOD 0=1, 1=2, 2=4, 3=8

	GenerateGreedyXPlan(true, MeshData, LocalVoxels, Step);
	GenerateGreedyXPlan(false, MeshData, LocalVoxels, Step);
	GenerateGreedyYPlan(true, MeshData, LocalVoxels, Step);
	GenerateGreedyYPlan(false, MeshData, LocalVoxels, Step);
	GenerateGreedyZPlan(true, MeshData, LocalVoxels, Step);
	GenerateGreedyZPlan(false, MeshData, LocalVoxels, Step);

	int SolidCount = 0;
	for (auto& V : LocalVoxels)
	{
		if (V.Material.Id > 0)
			SolidCount++;
	}

	UE_LOG(LogTemp, Warning, TEXT("SOLID VOXELS = %d"), SolidCount);
}

// ====================== X AXIS ======================
void AVoxelChunck::GenerateGreedyXPlan(bool IsPositive, FChunckMeshData& MeshData, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step)
{
	if (IsPositive) GenerateGreedyXPositive(MeshData, LocalVoxels, Step);
	else            GenerateGreedyXNegative(MeshData, LocalVoxels, Step);
}

void AVoxelChunck::GenerateGreedyXPositive(FChunckMeshData& MeshData, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step)
{
	TArray<bool> Mask;
	Mask.Init(false, Size * Size);

	for (int x = 0; x < Size; x += Step)
	{
		BuildMaskXPositive(x, Mask, LocalVoxels, Step);   // mask rempli correctement
		Greedy2DX(x, Mask, true, MeshData, Step);
	}
}

void AVoxelChunck::GenerateGreedyXNegative(FChunckMeshData& MeshData, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step)
{
	TArray<bool> Mask;
	Mask.Init(false, Size * Size);

	for (int x = 0; x < Size; x += Step)
	{
		BuildMaskXNegative(x, Mask, LocalVoxels, Step);
		Greedy2DX(x, Mask, false, MeshData, Step);
	}
}

void AVoxelChunck::Greedy2DX(int x, TArray<bool>& Mask, bool IsPositive, FChunckMeshData& MeshData, int32 Step)
{
	for (int z = 0; z < Size; z += Step)
	{
		int y = 0;
		while (y < Size)
		{
			int index = y * Size + z;
			if (!Mask[index])
			{
				y += Step;
				continue;
			}

			// Largeur along Y
			int width = 1;
			while (y + width * Step < Size && Mask[(y + width * Step) * Size + z])
				width++;

			// Hauteur along Z
			int height = 1;
			bool done = false;
			while (z + height * Step < Size && !done)
			{
				for (int k = 0; k < width; k++)
				{
					if (!Mask[(y + k * Step) * Size + (z + height * Step)])
					{
						done = true;
						break;
					}
				}
				if (!done) height++;
			}

			// Marquer comme traité
			for (int dz = 0; dz < height; dz++)
				for (int dy = 0; dy < width; dy++)
					Mask[(y + dy * Step) * Size + (z + dz * Step)] = false;

			// Ajouter le quad
			if (IsPositive)
				AddQuadXPositive(x, y, z, width * Step, height * Step, MeshData);
			else
				AddQuadXNegative(x, y, z, width * Step, height * Step, MeshData);

			y += width * Step;
		}
	}
}

void AVoxelChunck::Greedy2DY(int y, TArray<bool>& Mask, bool IsPositive, FChunckMeshData& MeshData, int32 Step)
{
	for (int z = 0; z < Size; z += Step)
	{
		int x = 0;
		while (x < Size)
		{
			int index = x * Size + z;
			if (!Mask[index])
			{
				x += Step;
				continue;
			}

			int width = 1;   // along X
			while (x + width * Step < Size && Mask[(x + width * Step) * Size + z])
				width++;

			int height = 1;  // along Z
			bool done = false;
			while (z + height * Step < Size && !done)
			{
				for (int k = 0; k < width; k++)
				{
					if (!Mask[(x + k * Step) * Size + (z + height * Step)])
					{
						done = true;
						break;
					}
				}
				if (!done) height++;
			}

			for (int dz = 0; dz < height; dz++)
				for (int dx = 0; dx < width; dx++)
					Mask[(x + dx * Step) * Size + (z + dz * Step)] = false;

			if (IsPositive)
				AddQuadYPositive(x, y, z, width * Step, height * Step, MeshData);
			else
				AddQuadYNegative(x, y, z, width * Step, height * Step, MeshData);

			x += width * Step;
		}
	}
}

void AVoxelChunck::Greedy2DZ(int z, TArray<bool>& Mask, bool IsPositive, FChunckMeshData& MeshData, int32 Step)
{
	for (int y = 0; y < Size; y += Step)
	{
		int x = 0;
		while (x < Size)
		{
			int index = x * Size + y;
			if (!Mask[index])
			{
				x += Step;
				continue;
			}

			int width = 1;   // along X
			while (x + width * Step < Size && Mask[(x + width * Step) * Size + y])
				width++;

			int height = 1;  // along Y
			bool done = false;
			while (y + height * Step < Size && !done)
			{
				for (int k = 0; k < width; k++)
				{
					if (!Mask[(x + k * Step) * Size + (y + height * Step)])
					{
						done = true;
						break;
					}
				}
				if (!done) height++;
			}

			for (int dy = 0; dy < height; dy++)
				for (int dx = 0; dx < width; dx++)
					Mask[(x + dx * Step) * Size + (y + dy * Step)] = false;

			if (IsPositive)
				AddQuadZPositive(x, y, z, width * Step, height * Step, MeshData);
			else
				AddQuadZNegative(x, y, z, width * Step, height * Step, MeshData);

			x += width * Step;
		}
	}
}

// ====================== Y AXIS ======================
void AVoxelChunck::GenerateGreedyYPlan(bool IsPositive, FChunckMeshData& MeshData, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step)
{
	if (IsPositive) GenerateGreedyYPositive(MeshData, LocalVoxels, Step);
	else            GenerateGreedyYNegative(MeshData, LocalVoxels, Step);
}

void AVoxelChunck::GenerateGreedyYPositive(FChunckMeshData& MeshData, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step)
{
	TArray<bool> Mask;
	Mask.Init(false, Size * Size);

	for (int y = 0; y < Size; y += Step)
	{
		BuildMaskYPositive(y, Mask, LocalVoxels, Step);
		Greedy2DY(y, Mask, true, MeshData, Step);
	}
}

void AVoxelChunck::GenerateGreedyYNegative(FChunckMeshData& MeshData, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step)
{
	TArray<bool> Mask;
	Mask.Init(false, Size * Size);

	for (int y = 0; y < Size; y += Step)
	{
		BuildMaskYNegative(y, Mask, LocalVoxels, Step);
		Greedy2DY(y, Mask, false, MeshData, Step);
	}
}



// ====================== Z AXIS ======================
void AVoxelChunck::GenerateGreedyZPlan(bool IsPositive, FChunckMeshData& MeshData, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step)
{
	if (IsPositive) GenerateGreedyZPositive(MeshData, LocalVoxels, Step);
	else            GenerateGreedyZNegative(MeshData, LocalVoxels, Step);
}

void AVoxelChunck::GenerateGreedyZPositive(FChunckMeshData& MeshData, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step)
{
	TArray<bool> Mask;
	Mask.Init(false, Size * Size);

	for (int z = 0; z < Size; z += Step)
	{
		BuildMaskZPositive(z, Mask, LocalVoxels, Step);
		Greedy2DZ(z, Mask, true, MeshData, Step);
	}
}

void AVoxelChunck::GenerateGreedyZNegative(FChunckMeshData& MeshData, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step)
{
	TArray<bool> Mask;
	Mask.Init(false, Size * Size);

	for (int z = 0; z < Size; z += Step)
	{
		BuildMaskZNegative(z, Mask, LocalVoxels, Step);
		Greedy2DZ(z, Mask, false, MeshData, Step);
	}
}

void AVoxelChunck::BuildMaskXPositive(int x, TArray<bool>& Mask, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step)
{
    int Marked = 0;
    for (int y = 0; y < Size; y += Step)
    {
        for (int z = 0; z < Size; z += Step)
        {
            bool Current = IsVoxelSolidLocal(x, y, z, LocalVoxels);
            bool Neighbor = (x + Step < Size) ? IsVoxelSolidLocal(x + Step, y, z, LocalVoxels) : false;
            bool Visible = Current && !Neighbor;

            Mask[y * Size + z] = Visible;
            if (Visible) Marked++;
        }
    }
    if (Marked > 0)
        UE_LOG(LogTemp, Warning, TEXT("BuildMaskXPositive(x=%d) → %d faces marquées"), x, Marked);
}

void AVoxelChunck::BuildMaskXNegative(int x, TArray<bool>& Mask, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step)
{
    int Marked = 0;
    for (int y = 0; y < Size; y += Step)
    {
        for (int z = 0; z < Size; z += Step)
        {
            bool Current = IsVoxelSolidLocal(x, y, z, LocalVoxels);
            bool Neighbor = (x - Step >= 0) ? IsVoxelSolidLocal(x - Step, y, z, LocalVoxels) : false;
            bool Visible = Current && !Neighbor;

            Mask[y * Size + z] = Visible;
            if (Visible) Marked++;
        }
    }
    if (Marked > 0)
        UE_LOG(LogTemp, Warning, TEXT("BuildMaskXNegative(x=%d) → %d faces marquées"), x, Marked);
}

void AVoxelChunck::BuildMaskYPositive(int y, TArray<bool>& Mask, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step)
{
    int Marked = 0;
    for (int x = 0; x < Size; x += Step)
    {
        for (int z = 0; z < Size; z += Step)
        {
            bool Current = IsVoxelSolidLocal(x, y, z, LocalVoxels);
            bool Neighbor = (y + Step < Size) ? IsVoxelSolidLocal(x, y + Step, z, LocalVoxels) : false;
            bool Visible = Current && !Neighbor;

            Mask[x * Size + z] = Visible;
            if (Visible) Marked++;
        }
    }
    if (Marked > 0)
        UE_LOG(LogTemp, Warning, TEXT("BuildMaskYPositive(y=%d) → %d faces marquées"), y, Marked);
}

void AVoxelChunck::BuildMaskYNegative(int y, TArray<bool>& Mask, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step)
{
    int Marked = 0;
    for (int x = 0; x < Size; x += Step)
    {
        for (int z = 0; z < Size; z += Step)
        {
            bool Current = IsVoxelSolidLocal(x, y, z, LocalVoxels);
            bool Neighbor = (y - Step >= 0) ? IsVoxelSolidLocal(x, y - Step, z, LocalVoxels) : false;
            bool Visible = Current && !Neighbor;

            Mask[x * Size + z] = Visible;
            if (Visible) Marked++;
        }
    }
    if (Marked > 0)
        UE_LOG(LogTemp, Warning, TEXT("BuildMaskYNegative(y=%d) → %d faces marquées"), y, Marked);
}

void AVoxelChunck::BuildMaskZPositive(int z, TArray<bool>& Mask, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step)
{
    int Marked = 0;
    for (int x = 0; x < Size; x += Step)
    {
        for (int y = 0; y < Size; y += Step)
        {
            bool Current = IsVoxelSolidLocal(x, y, z, LocalVoxels);
            bool Neighbor = (z + Step < Size) ? IsVoxelSolidLocal(x, y, z + Step, LocalVoxels) : false;
            bool Visible = Current && !Neighbor;

            Mask[x * Size + y] = Visible;
            if (Visible) Marked++;
        }
    }
    if (Marked > 0)
        UE_LOG(LogTemp, Warning, TEXT("BuildMaskZPositive(z=%d) → %d faces marquées"), z, Marked);
}

void AVoxelChunck::BuildMaskZNegative(int z, TArray<bool>& Mask, const TArray<FVoxelDataStructure>& LocalVoxels, int32 Step)
{
    int Marked = 0;
    for (int x = 0; x < Size; x += Step)
    {
        for (int y = 0; y < Size; y += Step)
        {
            bool Current = IsVoxelSolidLocal(x, y, z, LocalVoxels);
            bool Neighbor = (z - Step >= 0) ? IsVoxelSolidLocal(x, y, z - Step, LocalVoxels) : false;
            bool Visible = Current && !Neighbor;

            Mask[x * Size + y] = Visible;
            if (Visible) Marked++;
        }
    }
    if (Marked > 0)
        UE_LOG(LogTemp, Warning, TEXT("BuildMaskZNegative(z=%d) → %d faces marquées"), z, Marked);
}



float AVoxelChunck::GetVoxelSize()
{
	return VoxelSize;
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
		return true;                   // chunk non chargé = plein (correct)

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

/*
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
*/





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
	bool VoxelMask[64][64][64] = { false };

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