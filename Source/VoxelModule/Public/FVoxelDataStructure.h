#pragma once
#include "CoreMinimal.h"
#include "FVoxelMaterial.h"

struct FVoxelDataStructure
{
    FVoxelMaterial Material;
    uint8 Flags = 0;

};
static_assert(sizeof(FVoxelDataStructure) == 2);