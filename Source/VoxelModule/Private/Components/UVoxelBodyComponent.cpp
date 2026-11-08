#include "Components/UVoxelBodyComponent.h"
#include "UVoxelBodyAsset.h"
#include "AnimationRuntime.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Animation/Skeleton.h"
#include "DebugMacro.h"

DEFINE_LOG_CATEGORY_STATIC(LogVoxelBody, Log, All);

UVoxelBodyComponent::UVoxelBodyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UVoxelBodyComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!Body)
	{
		return;
	}

	// 1) DONNÉES (client ET serveur)
	Target = ResolveSkeletalTarget();
	if (!Target)
	{
		return;   // la raison est déjà loguée
	}

	if (!BuildInstanceData())
	{
		return;
	}

	// 2) GARDE RÉSEAU : un serveur dédié n'affiche rien
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// 3) AFFICHAGE
	BuildVisuals();
}

void UVoxelBodyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	int32 RebuildCountThisFrame = 0;
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	for (int32 Slot = 0; Slot < Body->BoneNames.Num(); Slot++)
	{
		if (DirtyBones[Slot] == true)
		{
			RebuildBoneVisual(Slot);
			DirtyBones[Slot] = false;
			RebuildCountThisFrame++;
			SetComponentTickEnabled(false); //sera remis à true à la prochaine destruction
		}
	}
	if (RebuildCountThisFrame != 0)
	{
		NathanDebug(TEXT("RebuildCountThisFrame : %d"), RebuildCountThisFrame);
	}
}

USkeletalMeshComponent* UVoxelBodyComponent::ResolveSkeletalTarget()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	const int32 NumSlots = Body->BoneNames.Num();
	SlotToBoneIndex.Init(INDEX_NONE, NumSlots);   // AVANT les boucles

	// a) Choisir le skeletal mesh construit sur le squelette du corps voxel
	TArray<USkeletalMeshComponent*> MeshComponents;
	Owner->GetComponents<USkeletalMeshComponent>(MeshComponents);

	USkeletalMeshComponent* Kept = nullptr;
	for (USkeletalMeshComponent* Candidate : MeshComponents)
	{
		const USkeletalMesh* SkeletalMesh = Candidate ? Candidate->GetSkeletalMeshAsset() : nullptr;
		if (!SkeletalMesh)
		{
			continue;   // composant sans mesh : pas de squelette
		}
		if (SkeletalMesh->GetSkeleton() == Body->Skeleton.Get())
		{
			Kept = Candidate;
			break;
		}
	}

	if (!Kept)
	{
		UE_LOG(LogVoxelBody, Error, TEXT("%s : aucun skeletal mesh n'utilise le squelette %s"),
			*GetNameSafe(Owner), *GetNameSafe(Body->Skeleton.Get()));
		return nullptr;
	}

	// b) Nom d'os -> indice dans le mesh actuel
	int32 NumResolved = 0;
	for (int32 Slot = 0; Slot < NumSlots; ++Slot)
	{
		const FName BoneName = Body->BoneNames[Slot];
		const int32 BoneIndex = Kept->GetBoneIndex(BoneName);
		SlotToBoneIndex[Slot] = BoneIndex;

		if (BoneIndex == INDEX_NONE)
		{
			UE_LOG(LogVoxelBody, Warning, TEXT("%s : l'os '%s' (slot %d) n'existe pas dans ce mesh"),
				*GetNameSafe(Owner), *BoneName.ToString(), Slot);
		}
		else
		{
			++NumResolved;
		}
	}

	UE_LOG(LogVoxelBody, Log, TEXT("%s : %d/%d os résolus"), *GetNameSafe(Owner), NumResolved, NumSlots);
	return Kept;
}

bool UVoxelBodyComponent::BuildInstanceData()
{
	// a) Données partagées de l'asset (construites une seule fois par machine)
	Body->BuildRuntimeDataIfNeeded();
	if (!Body->IsRuntimeDataBuilt())
	{
		return false;   // l'asset a logué la raison
	}

	int32 NumSlots = Body->BoneNames.Num();
	DirtyBones.Init(false, NumSlots);
	SlotMin.Init(FIntVector(MAX_int32), NumSlots);
	SlotMax.Init(FIntVector(MIN_int32), NumSlots);
	for (const FVoxelBodyCell& Cell : Body->Cells)
	{
		FIntVector& Low = SlotMin[Cell.BoneSlot];
		FIntVector& High = SlotMax[Cell.BoneSlot];
		Low.X = FMath::Min(Low.X, (int32)Cell.I);
		High.X = FMath::Max(High.X, (int32)Cell.I);
		Low.Y = FMath::Min(Low.Y, (int32)Cell.J);
		High.Y = FMath::Max(High.Y, (int32)Cell.J);
		Low.Z = FMath::Min(Low.Z, (int32)Cell.K);
		High.Z = FMath::Max(High.Z, (int32)Cell.K);
	}
	// b) Copie propre à ce personnage, déduite de SlotGrid -> cohérente par construction
	const TArray<uint8>& SlotGrid = Body->GetSlotGrid();
	InstanceGrid.Init(FVoxelDataStructure(), SlotGrid.Num());   // tout vide : Id = 0, Flags = 0
	

	int32 NumSolid = 0;
	for (int32 GridIndex = 0; GridIndex < SlotGrid.Num(); ++GridIndex)
	{
		if (SlotGrid[GridIndex] == UVoxelBodyAsset::EmptySlot)
		{
			continue;   // case vide : le cas NORMAL, pas une erreur
		}
		InstanceGrid[GridIndex].Material.Id = 1;   // plus tard : vrai identifiant de matériau
		++NumSolid;
	}
	NumAlive = NumSolid;

	if (NumSolid != Body->Cells.Num())
	{
		UE_LOG(LogVoxelBody, Error, TEXT("%s : InstanceGrid incohérente, %d cases pleines pour %d cellules"),
			*GetNameSafe(GetOwner()), NumSolid, Body->Cells.Num());
		return false;
	}

	// c) Matrices de bind (l'ancien "bloc 2") : le serveur en aura besoin pour les impacts
	const USkeletalMesh* SkeletalMesh = Target->GetSkeletalMeshAsset();
	if (!SkeletalMesh)
	{
		return false;
	}
	const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();

	BoneCSPerSlot.Init(FTransform::Identity, NumSlots);   // AVANT la boucle
	for (int32 Slot = 0; Slot < NumSlots; ++Slot)
	{
		if (SlotToBoneIndex[Slot] == INDEX_NONE)
		{
			continue;
		}
		BoneCSPerSlot[Slot] = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, SlotToBoneIndex[Slot]);
	}

	UE_LOG(LogVoxelBody, Log, TEXT("%s : InstanceGrid prête, %d cases pleines sur %d"),
		*GetNameSafe(GetOwner()), NumSolid, InstanceGrid.Num());
	if (Body->Cells.Num() > 0)
	{
		const FVoxelBodyCell& TestCell = Body->Cells[Body->Cells.Num() / 2];
		UE_LOG(LogVoxelBody, Log, TEXT("Cellule test : %d %d %d"), TestCell.I, TestCell.J, TestCell.K);
	}
	return true;
}

void UVoxelBodyComponent::BuildVisuals()
{
	if (!MeshCube)
	{
		return;
	}

	// Échelle calculée une seule fois. BoxExtent = DEMI-taille du cube.
	const double CubeSize = MeshCube->GetBounds().BoxExtent.X * 2.0;
	if (CubeSize <= UE_KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogVoxelBody, Error, TEXT("%s : MeshCube de taille nulle"), *GetNameSafe(GetOwner()));
		return;
	}
	CubeScale = Body->CellSize / CubeSize;

	AActor* Owner = GetOwner();
	const int32 NumSlots = Body->BoneNames.Num();
	MeshCubeComponents.Init(nullptr, NumSlots);   // AVANT la boucle

	int32 NumCreated = 0;
	for (int32 Slot = 0; Slot < NumSlots; ++Slot)
	{
		if (SlotToBoneIndex[Slot] == INDEX_NONE)
		{
			continue;
		}

		const FName BoneName = Body->BoneNames[Slot];
		UInstancedStaticMeshComponent* ISM =
			NewObject<UInstancedStaticMeshComponent>(Owner, FName(TEXT("VoxelISM_") + BoneName.ToString()));
		ISM->SetStaticMesh(MeshCube);
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->SetMobility(EComponentMobility::Movable);   // AVANT RegisterComponent : il suit un os animé
		ISM->SetupAttachment(Target, BoneName);           // repère de l'ISM = repère de l'os
		ISM->RegisterComponent();

		MeshCubeComponents[Slot] = ISM;
		++NumCreated;
	}

	UE_LOG(LogVoxelBody, Log, TEXT("%s : %d ISM créés"), *GetNameSafe(Owner), NumCreated);
	FillInstance();
}

void UVoxelBodyComponent::FillInstance()
{
	int32 TotalInstances = 0;
	for (int32 Slot = 0; Slot < MeshCubeComponents.Num(); ++Slot)
	{
		TotalInstances += RebuildBoneVisual(Slot);
	}

	// Au spawn, aucune cellule détruite : on doit retrouver toutes les cellules de l'asset.
	const int32 Expected = Body->Cells.Num();
	if (TotalInstances == Expected)
	{
		UE_LOG(LogVoxelBody, Log, TEXT("%s : %d instances sur %d cellules"),
			*GetNameSafe(GetOwner()), TotalInstances, Expected);
	}
	else
	{
		UE_LOG(LogVoxelBody, Error, TEXT("%s : %d instances sur %d cellules (ÉCART)"),
			*GetNameSafe(GetOwner()), TotalInstances, Expected);
	}
}

int32 UVoxelBodyComponent::RebuildBoneVisual(const int32 Slot)
{
	NathanDebug(TEXT("RebuildBoneVisual"));
	// IsValidIndex teste l'INDICE ; Contains chercherait une VALEUR dans le tableau.
	if (!MeshCubeComponents.IsValidIndex(Slot) || !SlotToBoneIndex.IsValidIndex(Slot))
	{
		UE_LOG(LogVoxelBody, Error, TEXT("UVoxelBodyComponent::CarveSphere(int32 I0, int32 J0, int32 K0, int32 Radius) -->InstanceGrid.Num() == 0"));
		return 0;   // ex. serveur dédié : aucun ISM
	}
	UInstancedStaticMeshComponent* ISM = MeshCubeComponents[Slot];
	if (!ISM || SlotToBoneIndex[Slot] == INDEX_NONE)
	{
		UE_LOG(LogVoxelBody, Error, TEXT("UVoxelBodyComponent::CarveSphere(int32 I0, int32 J0, int32 K0, int32 Radius) -->InstanceGrid.Num() == 0"));
		return 0;
	}

	const TArray<int32>& CellsOfBone = Body->GetCellsOfSlot(Slot);   // indices dans Body->Cells

	TArray<FTransform> Transforms;
	Transforms.Reserve(CellsOfBone.Num());   // maximum connu d'avance

	for (const int32 CellIdx : CellsOfBone)
	{
		const FVoxelBodyCell& Cell = Body->Cells[CellIdx];   // const : l'asset est partagé, lecture seule
		const int32 GridIndex = Body->CellIndex(Cell.I, Cell.J, Cell.K);

		if (InstanceGrid[GridIndex].Material.Id == 0)
		{
			continue;   // cellule détruite
		}

		// FVector(...) indispensable : sans lui, opérateur virgule
		const FVector CellCenter = Body->Origin
			+ FVector(Cell.I + 0.5, Cell.J + 0.5, Cell.K + 0.5) * Body->CellSize;

		const FTransform CellMeshSpace(FQuat::Identity, CellCenter, FVector(CubeScale));
		Transforms.Add(CellMeshSpace.GetRelativeTransform(BoneCSPerSlot[Slot]));   // "bind inverse"
	}

	// Une seule reconstruction, APRÈS la boucle.
	ISM->ClearInstances();
	if (Transforms.Num() > 0)
	{
		ISM->AddInstances(Transforms, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ false, /*bUpdateNavigation*/ false);
	}

	// Mesure indépendante : ce que l'ISM contient réellement.
	return ISM->GetInstanceCount();
}

int32 UVoxelBodyComponent::CarveSphere(int32 I0, int32 J0, int32 K0, int32 Radius)
{
	NathanDebug(TEXT("CarveSphere début"));
	if (InstanceGrid.Num() == 0)
	{
		UE_LOG(LogVoxelBody, Error, TEXT("UVoxelBodyComponent::CarveSphere(int32 I0, int32 J0, int32 K0, int32 Radius) -->InstanceGrid.Num() == 0"));
		return 0;
	}
	if (Radius < 0 || Radius > 100)
	{
		UE_LOG(LogVoxelBody, Error, TEXT("UVoxelBodyComponent::CarveSphere(int32 I0, int32 J0, int32 K0, int32 Radius) --> Radius < 0 || Radius > 100"));
		return 0;
	}
	int32 NumRemoved = 0;
	float SquaredRadius = Radius * Radius;
	for(int32 K = K0 - Radius; K <= K0 + Radius; K++)
		for (int32 J = J0 - Radius; J <= J0 + Radius; J++)
			for (int32 I = I0 - Radius; I <= I0 + Radius; I++)
			{
				if (!Body->IsInside(I, J, K))
				{
					continue;
				}
				int32 Dx = I - I0;
				int32 Dy = J - J0;
				int32 Dz = K - K0;
				if ((Dx * Dx + Dy * Dy + Dz * Dz) > SquaredRadius	)
				{
					continue;
				}
				int32 Index = Body->CellIndex(I, J, K);
				if (InstanceGrid[Index].Material.Id == 0)
				{
					continue;
				}
				InstanceGrid[Index].Material.Id = 0;
				NumRemoved++;
				DirtyBones[Body->GetSlotGrid()[Index]] = true;

			}
	NathanDebug(TEXT("CarveSphere sortie du for"));
	NumAlive -= NumRemoved;
	if (GetNetMode() != ENetMode::NM_DedicatedServer)
	{
		SetComponentTickEnabled(true);
	}
	/*La boucle de recontructoin doit aller dans un worker
	for (int32 Slot = 0; Slot < NumSlots; ++Slot)
	{
		if (DirtyBones[Slot])
		{
			RebuildBoneVisual(Slot);
		}
	}*
	*/
	NathanDebug(TEXT("NumAlive = %d GridInstance.Num() = %d, NumRemoved : %d"), NumAlive, InstanceGrid.Num(), NumRemoved);
	DrawCarveDebugSphere(I0, J0, K0, Radius);
	return NumRemoved;
}

int32 UVoxelBodyComponent::CarveCapsuleWorld(const FVector& AWorld, const FVector& BWorld, float RadiusCm)
{
	if (!Body || !Target)
	{
		return 0;
	}

	const FBoxSphereBounds& Bounds = Target->Bounds;
	if (FMath::PointDistToSegment(Bounds.Origin, AWorld, BWorld) > Bounds.SphereRadius + RadiusCm)
	{
		return 0;
	}
	const TArray<uint8>& SlotGrid = Body->GetSlotGrid();
	const double CellSize = Body->CellSize;
	const double R = RadiusCm / CellSize;   // rayon en cellules
	const double R2 = R * R;
	int32 NumRemoved = 0;
	const int32 NumSlots = SlotToBoneIndex.Num();
	if (SlotMin.Num() != NumSlots || SlotMax.Num() != NumSlots || BoneCSPerSlot.Num() != NumSlots
		|| DirtyBones.Num() != NumSlots || InstanceGrid.Num() != Body->GetSlotGrid().Num())
	{
		UE_LOG(LogTemp, Error, TEXT("CarveCapsuleWorld %s : tableaux incoherents | Slots=%d SlotMin=%d SlotMax=%d BoneCS=%d Dirty=%d Instance=%d SlotGrid=%d"),
			*GetNameSafe(GetOwner()), NumSlots, SlotMin.Num(), SlotMax.Num(), BoneCSPerSlot.Num(),
			DirtyBones.Num(), InstanceGrid.Num(), Body->GetSlotGrid().Num());
		return 0;
	}
	for (int32 Slot = 0; Slot < SlotToBoneIndex.Num(); Slot++)
	{
		if (SlotToBoneIndex[Slot] == INDEX_NONE || SlotMin[Slot].X > SlotMax[Slot].X)
		{
			continue;   // os non résolu, ou sans aucune cellule
		}
		const FTransform BoneWorld = Target->GetBoneTransform(SlotToBoneIndex[Slot]);
		const FVector A = (BoneCSPerSlot[Slot].TransformPosition(BoneWorld.InverseTransformPosition(AWorld)) - Body->Origin) / CellSize;
		const FVector B = (BoneCSPerSlot[Slot].TransformPosition(BoneWorld.InverseTransformPosition(BWorld)) - Body->Origin) / CellSize;

		// 2. Boîte à parcourir = boîte de la capsule ∩ boîte de l'os
		const int32 I0 = FMath::Max(FMath::FloorToInt(FMath::Min(A.X, B.X) - R), SlotMin[Slot].X);
		const int32 I1 = FMath::Min(FMath::CeilToInt(FMath::Max(A.X, B.X) + R), SlotMax[Slot].X);
		const int32 J0 = FMath::Max(FMath::FloorToInt(FMath::Min(A.Y, B.Y) - R), SlotMin[Slot].Y);
		const int32 J1 = FMath::Min(FMath::CeilToInt(FMath::Max(A.Y, B.Y) + R), SlotMax[Slot].Y);
		const int32 K0 = FMath::Max(FMath::FloorToInt(FMath::Min(A.Z, B.Z) - R), SlotMin[Slot].Z);
		const int32 K1 = FMath::Min(FMath::CeilToInt(FMath::Max(A.Z, B.Z) + R), SlotMax[Slot].Z);
		if (I0 > I1 || J0 > J1 || K0 > K1)
		{
			continue;   // la capsule ne recoupe pas cet os
		}
		FVector AB = B - A;
		const double AB2 = AB.SizeSquared();
		for (int32 K = K0; K <= K1; ++K)
			for (int32 J = J0; J <= J1; ++J)
				for (int32 I = I0; I <= I1; ++I)
				{
					int32 Index = Body->CellIndex(I, J, K);
					if (SlotGrid[Index] != Slot || InstanceGrid[Index].Material.Id == 0)
					{
						continue;
					}
					const FVector P(I + 0.5, J + 0.5, K + 0.5);
					const double T = (AB2 > 0.0) ? FMath::Clamp(FVector::DotProduct(P - A, AB) / AB2, 0.0, 1.0) : 0.0;
					if (FVector::DistSquared(P, A + T * AB) > R2)
					{
						continue;
					}
					InstanceGrid[Index].Material.Id = 0;
					DirtyBones[Slot] = true;
					NumRemoved++;
				}
	}
	if (NumRemoved > 0)
	{
		NumAlive -= NumRemoved;
		SetComponentTickEnabled(true);
	}
	return NumRemoved;

}

void UVoxelBodyComponent::DrawCarveDebugSphere(int32 I0, int32 J0, int32 K0, int32 Radius)
{
	const FVector CenterRest = Body->Origin + FVector(I0 + 0.5, J0 + 0.5, K0 + 0.5) * Body->CellSize;
	const float RadiusWorld = Radius * Body->CellSize;

	// Par défaut : position en pose de référence, approximative si le corps est animé
	FVector CenterWorld = Target->GetComponentTransform().TransformPosition(CenterRest);

	if (Body->IsInside(I0, J0, K0))
	{
		const int32 Bone = Body->GetSlotGrid()[Body->CellIndex(I0, J0, K0)];
		if (Bone != UVoxelBodyAsset::EmptySlot && SlotToBoneIndex[Bone] != INDEX_NONE)
		{
			const FTransform BoneWorld = Target->GetBoneTransform(SlotToBoneIndex[Bone]);
			CenterWorld = BoneWorld.TransformPosition(BoneCSPerSlot[Bone].InverseTransformPosition(CenterRest));
		}
	}

	DrawDebugSphere(GetWorld(), CenterWorld, RadiusWorld, 16, FColor::Blue, false, 10.f, SDPG_Foreground, 1.f);
}

