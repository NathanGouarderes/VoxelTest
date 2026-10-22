// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "../../VoxelModule/Public/UVoxelBodyAsset.h"

struct FBindPoseGeometry
{
	TArray<FVector3f>   Positions;
	TArray<FIntVector3> Triangles;   // 3 indices dans Positions
	FBox3f              Bounds = FBox3f(ForceInit);
	TArray<int32> DominantBone;

};
struct FVoxelGrid
{
	FIntVector      Dims = FIntVector::ZeroValue;  // nombre de cellules sur X, Y, Z
	FVector3d       Origin = FVector3d::ZeroVector;  // coin de la cellule (0,0,0)
	double          CellSize = 2.5;                    // arête d'une cellule (cm)
	TArray<float>   Winding;                           // nombre d'enroulement brut, une valeur par cellule
	TArray<int32>	Bone;

	int32 Index(const int32 I, const int32 J, const int32 K) const
	{
		return I + Dims.X * (J + Dims.Y * K);
	}

	FVector3d CellCenter(const int32 I, const int32 J, const int32 K) const
	{
		return Origin + FVector3d(I + 0.5, J + 0.5, K + 0.5) * CellSize;
	}
};

class FVoxelModuleEditor : public IModuleInterface
{
	public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	private:
	void RegisterMenus();
	static bool ExtractBindPoseGeometry(const USkeletalMesh& Mesh, const int32 LODIndex, FBindPoseGeometry& Out);
	static double ComputeWindingNumber(const FBindPoseGeometry& Geometry, const FVector3d& P);
	static void LogSliceAscii(const FBindPoseGeometry& Geometry, const double HeightRatio);
	static void LogFrontSilhouette(const FVoxelGrid& Grid);
	static void LogFrontBones(const FVoxelGrid& Grid, const FReferenceSkeleton& RefSkeleton);
	static void BuildVoxelGrid(const FBindPoseGeometry& Geometry, const double CellSize, const int32 Padding, const bool bUseParallel, FVoxelGrid& Out);
	static void AssignVoxelBones(const FBindPoseGeometry& Geometry, FVoxelGrid& Grid);
	static UVoxelBodyAsset* CreateVoxelBodyAsset(USkeletalMesh& SourceMesh, const FVoxelGrid& Grid);

};
