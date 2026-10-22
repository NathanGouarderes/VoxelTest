#pragma once

#include "CoreMinimal.h"

enum EVoxelRayHitExit : uint8
{
    Hit,
    Destroyed,
    Deflected,
    OutOfRange,
    StepBudgetExhausted,
    StartedInsideSolid,
    Unknown
};

struct FVoxelRayHit
{
    bool bHit = false;
    FVector ImpactPointCm;
    FIntVector VoxelCoord;
    FVector Normal;
    int32 MaterialId = 0;
    float DistanceCm = 0.0f;
    int32 ResolvedLOD;
    int32 StepsTaken;
    EVoxelRayHitExit ExitReason;

    FString ToString() const
    {
        return FString::Printf(
            TEXT("hit=%d voxel=(%d,%d,%d) dist=%.1fcm n=(%.0f,%.0f,%.0f) mat=%u steps=%d exit=%d"),
            bHit ? 1 : 0, VoxelCoord.X, VoxelCoord.Y, VoxelCoord.Z, DistanceCm,
            Normal.X, Normal.Y, Normal.Z,
            MaterialId, StepsTaken, (int32)ExitReason);
    }
};