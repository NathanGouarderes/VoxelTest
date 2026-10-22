#include "../../Public/Structs/FHeightMaxPyramid.h"
#include "Math/UnrealMathUtility.h"
#include "Templates/SubclassOf.h"


bool FHeightMaxPyramid::IsValid() const
{
    // 1. GridSize doit être une puissance de 2
    if (!FMath::IsPowerOfTwo(GridSize))
    {
        return false;
    }

    // 2. NumLevel doit être = log2(GridSize) + 1
    int32 ExpectedNumLevels = FMath::FloorLog2(GridSize) + 1;
    if (NumLevel != ExpectedNumLevels)
    {
        return false;
    }

    // 3. LevelOffsets doit avoir NumLevel éléments
    if (LevelOffsets.Num() != NumLevel)
    {
        return false;
    }

    // 4. Datas doit avoir la taille totale calculée
    int32 TotalSize = 0;
    for (int32 L = 0; L < NumLevel; L++)
    {
        int32 W = GridSize >> L;
        TotalSize += W * W;
    }
    if (Datas.Num() != TotalSize)
    {
        return false;
    }

    // 5. CellWorldSize doit être > 0
    if (CellWorldSize <= 0.f)
    {
        return false;
    }

    return true;
}

float FHeightMaxPyramid::SampleMax(int32 Level, int32 CX, int32 CY) const
{
	int32 W = GridSize >> Level;
	return Datas[LevelOffsets[Level] + CY * W + CX];
}

bool FHeightMaxPyramid::WorldToCell(const FVector2D& WorldPosCm, int32 Level, int32& OutCX, int32& OutCY) const
{
    const float CellSize = GetCellSize(Level);
    const float InvCell = 1.0f / CellSize;
    OutCX = FMath::FloorToInt((WorldPosCm.X - WorldOrigin.X) * InvCell);
    OutCY = FMath::FloorToInt((WorldPosCm.Y - WorldOrigin.Y) * InvCell);
    const int32 W = GridSize >> Level;
    return (OutCX >= 0 && OutCY >= 0 && OutCX < W && OutCY < W);
}

void FHeightMaxPyramid::Allocate(int32 InGridSize, float InCellWorldSize,
    const FVector2D& InWindowCenterCm)
{
    checkf(FMath::IsPowerOfTwo(InGridSize), TEXT("GridSize doit etre une puissance de 2"));
    checkf(InCellWorldSize > 0.f, TEXT("CellWorldSize doit etre > 0"));

    GridSize = InGridSize;
    CellWorldSize = InCellWorldSize;
    NumLevel = FMath::FloorLog2(InGridSize) + 1;

    // Le centre de la fenetre donne le coin min.
    const float HalfExtent = GridSize * CellWorldSize * 0.5f;
    WorldOrigin = InWindowCenterCm - FVector2D(HalfExtent, HalfExtent);

    // Offsets + taille totale, dans la meme passe.
    LevelOffsets.SetNumUninitialized(NumLevel);
    int32 Total = 0;
    for (int32 L = 0; L < NumLevel; ++L)
    {
        LevelOffsets[L] = Total;
        const int32 W = GridSize >> L;
        Total += W * W;
    }

    Datas.SetNumUninitialized(Total);

    GlobalMaxHeight = 0.f;   // rempli par ReducePyramid
    SafetyMargin = 0.f;   // mesure plus tard

    UE_LOG(LogTemp, Warning,
        TEXT("PYRAMID Allocate : GridSize=%d NumLevel=%d Total=%d (%.2f Mo) Origin=(%.0f, %.0f)"),
        GridSize, NumLevel, Total, Total * sizeof(float) / 1048576.0f,
        WorldOrigin.X, WorldOrigin.Y);
}

