#pragma once
#include "CoreMinimal.h"
#include "FVoxelBodyCell.generated.h"

USTRUCT()
struct VOXELMODULE_API FVoxelBodyCell
{
    GENERATED_BODY()

    UPROPERTY()
    int16 I = 0;
    UPROPERTY()
    int16 J = 0;
    UPROPERTY()
    int16 K = 0;
    UPROPERTY()
    int16 BoneSlot = -1;
};