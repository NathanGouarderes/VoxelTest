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
	CurrentLOD = 0;   
	bIsDirty = true;
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


void AVoxelChunck::GenerateAsyncGreedyMesh()
{
	if (!ChunckManager || !ChunckManager->VoxelWorld)
		return;

	FScopeLock Lock(&ChunckManager->VoxelWorld->ChunckMutex);
	const FChunckDataStructure* CD = ChunckManager->VoxelWorld->Chuncks.Find(Coord);
	if (!CD || CD->Voxels.Num() == 0)
		return;

	const int32 PaddedSize = Size + 2;
	TArray<FVoxelDataStructure> PaddedVoxels;
	PaddedVoxels.SetNum(PaddedSize * PaddedSize * PaddedSize);

	for (int z = 0; z < Size; z++)
	{
		for (int y = 0; y < Size; y++)
		{
			for (int x = 0; x < Size; x++)
			{
				int srcIdx = x + y * Size + z * Size * Size;
				int dstIdx = (x + 1) + (y + 1) * PaddedSize + (z + 1) * PaddedSize * PaddedSize;//On récupère les données des voxels voisins 
				PaddedVoxels[dstIdx] = CD->Voxels[srcIdx]; //En bordure de Chunk, si les voisins sont solides, le chunk voisin aura ses coutures solides aussi. Ici on récupère juste l'info.
			}
		}
	}

	const FIntVector Neighbors[6] = {
	{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}
	};

	for (const FIntVector& Dir : Neighbors)
	{
		const FChunckDataStructure* Neighbor = ChunckManager->VoxelWorld->Chuncks.Find(Coord + Dir);
		if (!Neighbor || Neighbor->Voxels.Num() == 0) continue;

		// Itérer sur la face de bordure correspondante
		for (int a = 0; a < Size; a++)
		{
			for (int b = 0; b < Size; b++)
			{
				int nx = (Dir.X == -1) ? (Size - 1) : (Dir.X == 1) ? 0 : a;
				int ny = (Dir.Y == -1) ? (Size - 1) : (Dir.Y == 1) ? 0 :
					(Dir.X != 0) ? a : b;
				int nz = (Dir.Z == -1) ? (Size - 1) : (Dir.Z == 1) ? 0 :
					(Dir.X != 0) ? b : (Dir.Y != 0) ? b : 0;

				// Position dans le buffer paddé (bordure du côté Dir)
				int px = (Dir.X == -1) ? 0 : (Dir.X == 1) ? PaddedSize - 1 : a + 1;
				int py = (Dir.Y == -1) ? 0 : (Dir.Y == 1) ? PaddedSize - 1 :
					(Dir.X != 0) ? a + 1 : b + 1;
				int pz = (Dir.Z == -1) ? 0 : (Dir.Z == 1) ? PaddedSize - 1 :
					(Dir.X != 0) ? b + 1 : (Dir.Y != 0) ? b + 1 : 1;

				int srcIdx = nx + ny * Size + nz * Size * Size;
				int dstIdx = px + py * PaddedSize + pz * PaddedSize * PaddedSize;
				if (Neighbor->Voxels.IsValidIndex(srcIdx))
					PaddedVoxels[dstIdx] = Neighbor->Voxels[srcIdx];
			}
		}
	}
		

	TWeakObjectPtr<AVoxelChunck> WeakThis(this);
	TWeakObjectPtr<AChunckManager> WeakManager(ChunckManager);


	//TArray<FVoxelDataStructure> LocalVoxels = CD->Voxels;
	int32 CapturedLOD = CurrentLOD;
	Async(EAsyncExecution::ThreadPool, [WeakThis, WeakManager, PaddedVoxels = MoveTemp(PaddedVoxels), CapturedLOD]() mutable
		{
			FChunckMeshData MeshData;

			if (!WeakThis.IsValid())
			{
				return;
			}

			WeakThis->GenerateGreedyMesh(MeshData, PaddedVoxels);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, WeakManager, MeshData = MoveTemp(MeshData), CapturedLOD]() mutable
				{
					if (!WeakThis.IsValid())
					{
						return;
					}
					AVoxelChunck* Chunck = WeakThis.Get();
					if (!Chunck || Chunck->bIsBeingDestroyed)
					{
						return;
					}
					if (CapturedLOD == 0)
					{
						Chunck->ApplyMesh(MeshData);
						if (AChunckManager* Manager = WeakManager.Get())
						{
							Manager->CurrentMeshJob = FMath::Max(0, Manager->CurrentMeshJob - 1);
						}
					}
					else
					{
						if (AChunckManager* Manager = WeakManager.Get())
						{
							FClusterGenResult ClusterGenResult;
							ClusterGenResult.LOD = CapturedLOD;
							ClusterGenResult.MeshData = MeshData;
							Manager->ClusterGenerationResult.Enqueue(ClusterGenResult);
							Manager->CurrentMeshJob = FMath::Max(0, Manager->CurrentMeshJob - 1);
						}
					}
					Chunck->bIsQueued = false;
				});
		});
}

void AVoxelChunck::GenerateGreedyMesh(FChunckMeshData& MeshData, const TArray<FVoxelDataStructure>& LocalVoxels)
{
    int32 Step = 1 << CurrentLOD;
	int32 VertexCount = 0;
    int32 EffectiveSize = Size / Step;
	const int32 PaddedSize = Size + 2;

    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        const int32 Axis1 = (Axis + 1) % 3;
        const int32 Axis2 = (Axis + 2) % 3;

        FIntVector AxisMask = FIntVector::ZeroValue;
        AxisMask[Axis] = 1;

        TArray<FMask> Mask;
        Mask.SetNum(EffectiveSize * EffectiveSize);

        FIntVector Iter = FIntVector::ZeroValue;

        // On itère sur les indices "LOD" (0, 1, 2... EffectiveSize)
        for (Iter[Axis] = -1; Iter[Axis] < EffectiveSize; ++Iter[Axis])
        {
            int32 N = 0;

            // === 1. Construire le Mask ===
            for (Iter[Axis2] = 0; Iter[Axis2] < EffectiveSize; ++Iter[Axis2])
            {
                for (Iter[Axis1] = 0; Iter[Axis1] < EffectiveSize; ++Iter[Axis1])
                {
                    // On multiplie par Step UNIQUEMENT pour lire la donnée voxel
                    const int32 SX = Iter.X * Step;
                    const int32 SY = Iter.Y * Step;
                    const int32 SZ = Iter.Z * Step;

                    const bool CurrentSolid = IsVoxelSolidLocal(SX, SY, SZ, LocalVoxels, PaddedSize);
                    // On compare avec le bloc suivant (décalé de Step)
                    const bool CompareSolid = IsVoxelSolidLocal(SX + AxisMask.X * Step, SY + AxisMask.Y * Step, SZ + AxisMask.Z * Step, LocalVoxels, PaddedSize);

                    if (CurrentSolid == CompareSolid) {
                        Mask[N++] = FMask{ 0, 0 };
                    } else if (CurrentSolid) {
                        Mask[N++] = FMask{ 1, 1 };
                    } else {
                        Mask[N++] = FMask{ 1, -1 };
                    }
                }
            }

            N = 0;
            // === 2. Générer les quads ===
            for (int32 j = 0; j < EffectiveSize; ++j)
            {
                for (int32 i = 0; i < EffectiveSize; )
                {
                    if (Mask[N].Normal != 0)
                    {
                        const FMask CurrentMask = Mask[N];
                        int32 Width = 1;
                        while (i + Width < EffectiveSize && CompareMask(Mask[N + Width], CurrentMask))
                            ++Width;

                        int32 Height = 1;
                        bool Done = false;
                        for (; Height + j < EffectiveSize; ++Height)
                        {
                            for (int32 k = 0; k < Width; ++k)
                            {
                                if (!CompareMask(Mask[N + k + Height * EffectiveSize], CurrentMask))
                                { Done = true; break; }
                            }
                            if (Done) break;
                        }

                        // Calcul des positions (on reste en indices LOD pour l'instant)
                        FIntVector V1 = Iter;
                        V1[Axis1] = i;
                        V1[Axis2] = j;
						V1[Axis] += 1;
                       // if (CurrentMask.Normal > 0) V1[Axis] += 1;

                        FIntVector V2 = V1; V2[Axis1] += Width;
                        FIntVector V3 = V1; V3[Axis2] += Height;
                        FIntVector V4 = V2; V4[Axis2] += Height;

                        // CLÉ : On multiplie par Step ici pour remettre le quad à la bonne taille monde
                        CreateQuad(CurrentMask, AxisMask, Width, Height, 
                                   V1 * Step, V2 * Step, V3 * Step, V4 * Step, 
                                   VertexCount, MeshData, Step);

						VertexCount += 4;

                        for (int32 l = 0; l < Height; ++l)
                            for (int32 k = 0; k < Width; ++k)
                                Mask[N + k + l * EffectiveSize] = FMask{ 0, 0 };

                        i += Width; N += Width;
                    }
                    else { ++i; ++N; }
                }
            }
        }
    }
}

void AVoxelChunck::CreateQuad(const FMask& Mask, const FIntVector& AxisMask, int32 Width, int32 Height, const FIntVector& V1, const FIntVector& V2, const FIntVector& V3, const FIntVector& V4, int32& VertexCount, FChunckMeshData& MeshData, int32 Step)
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

bool AVoxelChunck::IsVoxelSolidLocal(int x, int y, int z, const TArray<FVoxelDataStructure>& LocalVoxels, int32 PaddedSize)
{

	int px = x + 1;
	int py = y + 1;
	int pz = z + 1;
	if (px < 0 || px >= PaddedSize || py < 0 || py >= PaddedSize || pz < 0 || pz >= PaddedSize)
		return false;

	int index = px + py * PaddedSize + pz * PaddedSize * PaddedSize;

	if (!LocalVoxels.IsValidIndex(index))
		return false;

	return LocalVoxels[index].Material.Id > 0;
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