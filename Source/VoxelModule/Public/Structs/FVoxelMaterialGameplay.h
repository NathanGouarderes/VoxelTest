#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NiagaraSystem.h"
#include "FVoxelMaterialGameplay.generated.h" 
USTRUCT(BlueprintType)
struct FVoxelMaterialGameplay
{
    GENERATED_BODY()
    UPROPERTY()
    FGameplayTagContainer Tags;
    UPROPERTY()
    uint8 bSupportsVegetation = 1;
    UPROPERTY()
    float Fertility = 0.0f;
    UPROPERTY()
    TObjectPtr<USoundBase> ImpactSound;
    UPROPERTY()
    TObjectPtr<UNiagaraSystem> DebrisEffect;
};