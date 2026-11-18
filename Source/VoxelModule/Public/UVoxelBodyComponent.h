#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FVoxelDataStructure.h"
#include "UVoxelBodyComponent.generated.h"

class UVoxelBodyAsset;
class UStaticMesh;
class USkeletalMeshComponent;
class UInstancedStaticMeshComponent;

struct FBrickDelta
{
	uint64 Removed[8] = {};   // 8 × 64 = 512 bits, 1 = cellule détruite
};

struct FCellHit
{
	int32 Index;     // position dans la grille ENTIÈRE du corps (pas relative à l'os)
	int32 Slot;      // os auquel la cellule appartient
	FIntVector Cell; // coordonnées I, J, K : seulement pour le debug
	double T;        // position le long du segment : 0 en A, 1 en B
	double W;        // 1 sur l'axe du poing, 0 au bord
};
************************************************************************************************************************************************
#pragma once
	#include "CoreMinimal.h"
	#include "Net/Serialization/FastArraySerializer.h"
class UVoxelBodyComponent;
USTRUCT()
struct FVoxelBodySegment
{
	GENERATED_BODY()
	UPROPERTY()
	FIntVector A = FIntVector::ZeroValue;
	UPROPERTY()
	FIntVector B = FIntVector::ZeroValue;
};

USTRUCT()
struct FVoxelBodyEditEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()
	UPROPERTY()
	TArray<FVoxelBodySegment> VoxelBodySegment;
	UPROPERTY()
	int32 HitRadius = 0;
	UPROPERTY()
	int32 EnergyIn = 0;
	UPROPERTY()
	int32 StrikePressure = 0;
	UPROPERTY()
	int32 Seq = 0;
};
USTRUCT()
struct FVoxelBodyEditJournal : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FVoxelBodyEditEntry> Items;                              // les lignes du journal (repliquees)

	UPROPERTY(NotReplicated)
	TObjectPtr<UVoxelBodyComponent> TargetVoxelBody = nullptr; 

	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);

	// Point d'entree de la serialisation delta : le moteur l'appelle a la place de la serialisation normale.
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		// Compare l'etat connu du client a l'etat actuel et n'ecrit que la difference.
		return FFastArraySerializer::FastArrayDeltaSerialize<FVoxelBodyEditEntry, FVoxelBodyEditJournal>(Items, DeltaParms, *this);
	}
};

// Trait : dit au moteur "cette struct fournit son propre NetDeltaSerialize, utilise-le".
// Sans lui, le tableau serait replique en entier a chaque changement.
template<>
struct TStructOpsTypeTraits<FVoxelBodyEditJournal> : public TStructOpsTypeTraitsBase2<FVoxelBodyEditJournal>
{
	enum { WithNetDeltaSerializer = true };                     // active NetDeltaSerialize
};


DANS UVoxelBodyComponent

virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
void UVoxelBodyComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);    // garde les proprietes repliquees d'AActor
    DOREPLIFETIME(UVoxelBodyComponent, VoxelBodyEditJournal);             // le journal est envoye a tous les clients
}
UPROPERTY(Replicated)
FVoxelBodyEditJournal VoxelBodyEditJournal;
int32 NextBodySeq = 0;
SetIsReplicatedByDefault(true) dans le constructeur
VoxelBodyEditJournal.TargetVoxelBody = this;


************************************************************************************************************************************************


UCLASS(ClassGroup = (Voxel), meta = (BlueprintSpawnableComponent))
class VOXELMODULE_API UVoxelBodyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVoxelBodyComponent();

	UPROPERTY(EditAnywhere, Category = "Voxel")
	TObjectPtr<UVoxelBodyAsset> Body;

	UPROPERTY(EditAnywhere, Category = "Voxel")
	TObjectPtr<UStaticMesh> MeshCube;

	// Reconstruit l'ISM d'un os depuis InstanceGrid. Renvoie son nombre d'instances.
	int32 RebuildBoneVisual(int32 Slot);
	int32 CarveSphere(int32 I0, int32 J0, int32 K0, int32 Radius);
	int32 CarveCapsuleWorld(const FVector& AWorld, const FVector& BWorld, float RadiusCm,
		int32 Pressure, int32& InOutEnergy, bool& bOutBlocked);
	void DrawCarveDebugSphere(int32 I0, int32 J0, int32 K0, int32 Radius);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// ----- Partie données : client ET serveur -----
	USkeletalMeshComponent* ResolveSkeletalTarget();
	bool BuildInstanceData();


	// ----- Partie affichage : jamais sur un serveur dédié -----
	void BuildVisuals();
	void FillInstance();

	// Pointeurs UObject stockés dans un UObject -> UPROPERTY obligatoire.
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Target;
	UPROPERTY(EditAnywhere, Category = "VoxelBody|Combat")
	float FleshResistancePerCm3 = 2000.0f;
	TArray<float> SlotToughness;
	TArray<int32> CellsMaxIntegrity;
	UPROPERTY(EditAnywhere, Category = "VoxelBody|Debug")
	bool bDebugDrawRemovedCells = true;
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> MeshCubeComponents;   // indexé par slot

	TArray<int32> SlotToBoneIndex;              // slot -> indice d'os dans le mesh actuel
	TArray<FTransform> BoneCSPerSlot;           // slot -> matrice de bind (pose de repos, repère du mesh)
	TArray<FVoxelDataStructure> InstanceGrid;   // les voxels qui composent le personnage,	 copie modifiable propre à ce personnage
	TArray<FIntVector> SlotMin; // boîte des cellules de chaque os, en coordonnées de grille
	TArray<FIntVector> SlotMax;
	double CubeScale = 1.0;
	int32 NumAlive = 0;
	UPROPERTY()
	TArray<bool> DirtyBones;
	TMap<int32, FBrickDelta> Delta;
};
