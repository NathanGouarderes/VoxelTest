#pragma once

#include "CoreMinimal.h"
struct FHeightMaxPyramid //étage A construit une seule fois au démarrage
{
    TArray<float> Datas;
    TArray<int32> LevelOffsets;
    float CellWorldSize = 1600.0f; // taille en cm
    int32 GridSize = 1024;
    int32 NumLevel = 11;
    FVector2D WorldOrigin;
    float GlobalMaxHeight;
    float GlobalMargin;
    float SafetyMargin = 0.f;
    bool IsValid() const;
    void  Allocate(int32 InGridSize, float InCellWorldSize, const FVector2D& InWindowCenterCm);
    float SampleMax(int32 Level, int32 CX, int32 CY) const;          // 1 ligne
    bool  WorldToCell(const FVector2D& WorldCm, int32 Level, int32& OutCX, int32& OutCY) const;
    float GetCellSize(int32 Level) const { return CellWorldSize * (1 << Level); }

}; 