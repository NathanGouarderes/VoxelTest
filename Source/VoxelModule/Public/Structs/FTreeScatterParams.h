#pragma once
#include "CoreMinimal.h"

struct FTreeScatterParams
{
    int32 WorldSeed;
    float CellSize = 1000;
    float JitterRatio = 0.5;
    float MaxInfluenceRadiusXY = 1000;
    float MaxInfluenceHeightZ = 2500;
    float GlobalDensityScale = 1.0;
};
