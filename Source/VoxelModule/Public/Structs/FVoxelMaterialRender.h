#pragma once

#include "CoreMinimal.h"
#include "FVoxelMaterialRender.generated.h"
USTRUCT()
struct FVoxelMaterialRender
{
    GENERATED_BODY()
    TObjectPtr<UTexture2D> Albedo;
    TObjectPtr<UTexture2D> NormalMap;
    TObjectPtr<UTexture2D> ORM;
    float TriplanarScale;
    FLinearColor TintVariation;
};