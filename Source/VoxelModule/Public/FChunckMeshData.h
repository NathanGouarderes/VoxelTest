#pragma once
#include "CoreMinimal.h"

struct FChunckMeshData
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector2D> UVs;
    TArray<FVector> Normals;
    TArray<FProcMeshTangent> Tangents;
    TArray<FColor> VertexColors;
};
