#pragma once
#include "CoreMinimal.h"
#include "FQuad.generated.h"

USTRUCT(BlueprintType)
struct FQuad
{
	GENERATED_BODY()
	TArray<FVector> Vertices[4];
	TArray<FVector> Normals[4];
	TArray<FVector2D> UVs[4];
};
