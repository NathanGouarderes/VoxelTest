#include "UVoxelBodyAsset.h"

DEFINE_LOG_CATEGORY_STATIC(LogVoxelBodyAsset, Log, All);

void UVoxelBodyAsset::InvalidateRuntimeData()
{
	bRuntimeDataBuilt = false;
	SlotGrid.Empty();
	CellsPerSlot.Empty();
}

void UVoxelBodyAsset::BuildRuntimeDataIfNeeded()
{
	check(IsInGameThread());   // hypothèse "un seul thread", vérifiée à chaque appel

	if (bRuntimeDataBuilt)
	{
		return;
	}

	// Condition écrite pour le CAS D'ERREUR : une dimension nulle ou négative.
	if (Dims.X <= 0 || Dims.Y <= 0 || Dims.Z <= 0)
	{
		UE_LOG(LogVoxelBodyAsset, Error, TEXT("%s : dimensions invalides %s"), *GetName(), *Dims.ToString());
		return;
	}

	const int32 NumSlots = BoneNames.Num();

	SlotGrid.Init(EmptySlot, NumCells());   // AVANT la boucle qui la remplit
	CellsPerSlot.Empty();
	CellsPerSlot.SetNum(NumSlots);          // une liste vide par os

	int32 NumInvalid = 0;
	int32 NumDuplicates = 0;

	for (int32 CellIdx = 0; CellIdx < Cells.Num(); ++CellIdx)
	{
		const FVoxelBodyCell& Cell = Cells[CellIdx];

		// Protège l'écriture dans SlotGrid contre un asset corrompu.
		if (!IsInside(Cell.I, Cell.J, Cell.K) || Cell.BoneSlot >= NumSlots)
		{
			++NumInvalid;
			continue;
		}

		const int32 GridIndex = CellIndex(Cell.I, Cell.J, Cell.K);
		if (SlotGrid[GridIndex] != EmptySlot)
		{
			++NumDuplicates;   // deux cellules de la liste ont les mêmes (I, J, K)
			continue;
		}

		SlotGrid[GridIndex] = Cell.BoneSlot;
		CellsPerSlot[Cell.BoneSlot].Add(CellIdx);   // indice dans Cells, pas dans la grille
	}

	// Invariant principal : mesuré dans la grille elle-même, pas déduit des compteurs.
	int32 NumOccupied = 0;
	for (const uint8 Slot : SlotGrid)
	{
		if (Slot != EmptySlot)
		{
			++NumOccupied;
		}
	}

	bRuntimeDataBuilt = true;

	const bool bOk = (NumOccupied == Cells.Num() && NumInvalid == 0 && NumDuplicates == 0);
	if (bOk)
	{
		UE_LOG(LogVoxelBodyAsset, Log, TEXT("%s : grille partagée construite, %d/%d cases occupées, %d os"),
			*GetName(), NumOccupied, Cells.Num(), NumSlots);
	}
	else
	{
		UE_LOG(LogVoxelBodyAsset, Error, TEXT("%s : grille partagée INCOHERENTE, %d/%d cases occupées, %d invalides, %d doublons"),
			*GetName(), NumOccupied, Cells.Num(), NumInvalid, NumDuplicates);
	}
}