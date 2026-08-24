#pragma once

#include "CoreMinimal.h"
#include "UVoxelMaterialLibrary.generated.h"


UCLASS()
class VOXELMODULE_API UVoxelMaterialLibrary : public UPrimaryDataAsset
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) TArray<FVoxelMaterialPhysics>  Physics;
    UPROPERTY(EditAnywhere) TArray<FVoxelMaterialGameplay> Gameplay;
    UPROPERTY(EditAnywhere) TArray<FVoxelMaterialRender>   Render;   // vide sur dedicated server
    UPROPERTY(EditAnywhere) TArray<FName>                  DebugNames;
};