#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Structs/FVoxelBodyCell.h"
#include "UVoxelBodyAsset.generated.h"

class USkeleton;

UCLASS(BlueprintType)
class VOXELMODULE_API UVoxelBodyAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// ===== Données sauvegardées, écrites par l'outil "Voxeliser" =====
	UPROPERTY(VisibleAnywhere, Category = "Voxel Body")
	double CellSize = 0.625;

	UPROPERTY(VisibleAnywhere, Category = "Voxel Body")
	FVector Origin = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Voxel Body")
	FIntVector Dims = FIntVector::ZeroValue;

	UPROPERTY(VisibleAnywhere, Category = "Voxel Body")
	TArray<FVoxelBodyCell> Cells;

	UPROPERTY(VisibleAnywhere, Category = "Voxel Body")
	TArray<FName> BoneNames;

	UPROPERTY(VisibleAnywhere, Category = "Voxel Body")
	TObjectPtr<USkeleton> Skeleton;

	// ===== Géométrie de la grille : ne dépend que de Dims =====
	static constexpr uint8 EmptySlot = 255;   // "aucun os ici" -> 255 os au maximum (0..254)

	int32 NumCells() const
	{
		return Dims.X * Dims.Y * Dims.Z;
	}

	bool IsInside(const int32 I, const int32 J, const int32 K) const
	{
		return I >= 0 && I < Dims.X
			&& J >= 0 && J < Dims.Y
			&& K >= 0 && K < Dims.Z;
	}

	// MEME convention que FVoxelGrid::Index dans VoxelModuleEditor.
	int32 CellIndex(const int32 I, const int32 J, const int32 K) const
	{
		return I + J * Dims.X + K * Dims.X * Dims.Y;
	}

	// ===== Données calculées, partagées par tous les personnages =====
	void BuildRuntimeDataIfNeeded();   // thread principal uniquement
	void InvalidateRuntimeData();      // à appeler quand Cells ou BoneNames changent

	bool IsRuntimeDataBuilt() const { return bRuntimeDataBuilt; }

	const TArray<uint8>& GetSlotGrid() const { return SlotGrid; }

	// Indices dans Cells des cellules de cet os.
	const TArray<int32>& GetCellsOfSlot(const int32 Slot) const
	{
		check(CellsPerSlot.IsValidIndex(Slot));
		return CellsPerSlot[Slot];
	}

private:
	// Sans UPROPERTY : jamais sauvegardé (de toute façon interdit pour un tableau de tableaux).
	// Aucun pointeur UObject dedans : rien à suivre pour le ramasse-miettes.
	TArray<uint8> SlotGrid;
	TArray<TArray<int32>> CellsPerSlot;
	bool bRuntimeDataBuilt = false;
};