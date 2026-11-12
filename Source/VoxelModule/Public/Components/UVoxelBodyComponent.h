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