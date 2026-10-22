#pragma once
#include "CoreMinimal.h"
#include "RealtimeMeshComponent.h"
#include "Interface/Core/RealtimeMeshDataStream.h"
struct FChunckMeshData
{
    RealtimeMesh::FRealtimeMeshStreamSet Streams;
    FIntVector ChunckCoord;
    bool bIsEmpty = true;

    FChunckMeshData() = default;

    FChunckMeshData(FChunckMeshData&&) = default;
    FChunckMeshData& operator=(FChunckMeshData&&) = default;
    FChunckMeshData(const FChunckMeshData&) = delete;
    FChunckMeshData& operator=(const FChunckMeshData&) = delete;
};
