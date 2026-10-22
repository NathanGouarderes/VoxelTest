// Fill out your copyright notice in the Description page of Project Settings.


#include "FVoxelQueryScope.h"
#include "ChunckManager.h"
#include "VoxelWorld.h"
#include "FChunckDataStructure.h"
#include "VoxelMath.h"

FVoxelQueryScope::FVoxelQueryScope(AChunckManager& InManager) 
	: ChunkManager(InManager)
{
	if (::IsValid(ChunkManager.VoxelWorld))
	{
		World = ChunkManager.VoxelWorld;
		World->ChunckMutex.Lock();
	}
}

FVoxelQueryScope::~FVoxelQueryScope()
{
	if (World)
	{
		World->ChunckMutex.Unlock();
	}
}

FIntVector FVoxelQueryScope::WorldToVoxel(const FVector& PosCm) const
{
	const double VS = static_cast<double>(ChunkManager.VoxelSize);
	return FIntVector(
		FMath::FloorToInt32(PosCm.X / VS),
		FMath::FloorToInt32(PosCm.Y / VS),
		FMath::FloorToInt32(PosCm.Z / VS));
}

EVoxelOccupancy FVoxelQueryScope::Sample(const FIntVector& G)
{
	if (!World)
	{
		return EVoxelOccupancy::Unknown;
	}

	int ChunkSize = ChunkManager.ChunkSize;
	const FIntVector CC(
		FloorDivInt(G.X, ChunkSize)
		, FloorDivInt(G.Y, ChunkSize)
		, FloorDivInt(G.Z, ChunkSize));

	if (!bHasCache || CC != CachedChunkCoord)
	{
		CachedChunk = World->Chuncks.Find(CC);
		CachedChunkCoord = CC;
		bHasCache = true;
	}

	const FChunckDataStructure* ChunkDataStructure = CachedChunk;
	if (!ChunkDataStructure || !ChunkDataStructure->bIsChunckGenerated)
	{
		return EVoxelOccupancy::Unknown;
	}

	int32 LOD = ChunkDataStructure->LOD;
	int32 Sub = ChunkSize >> LOD;

	if (Sub <= 0 || ChunkDataStructure->Voxels.Num() != Sub * Sub * Sub)
	{
		return EVoxelOccupancy::Unknown;
	}

	const int32 lx = (G.X - CC.X * ChunkSize) >> LOD;
	const int32 ly = (G.Y - CC.Y * ChunkSize) >> LOD;
	const int32 lz = (G.Z - CC.Z * ChunkSize) >> LOD;
	const int32 Idx = lx + ly * Sub + lz * Sub * Sub;

	return (ChunkDataStructure->Voxels[Idx].Material.Id != 0) ? EVoxelOccupancy::Solid : EVoxelOccupancy::Air;
}

FVoxelSweepHit FVoxelQueryScope::Sweep(const FVector& FromCm, const FVector& ToCm)
{
	FVoxelSweepHit Hit;
	const double VS = static_cast<double>(ChunkManager.VoxelSize);
	const FVector O = FromCm / VS;               // origine en unites voxel
	const FVector D = (ToCm - FromCm) / VS;      // deplacement en unites voxel ; t parcourt [0,1]

	FIntVector Cell = WorldToVoxel(FromCm);

	const EVoxelOccupancy StartOcc = Sample(Cell);
	if (StartOcc == EVoxelOccupancy::Unknown)
	{
		Hit.bUnknown = true;
		return Hit;
	}
	if (StartOcc == EVoxelOccupancy::Solid)
	{
		Hit.bHit = true;
		Hit.bStartedInside = true;
		Hit.Voxel = Cell;
		Hit.Point = FromCm;
		return Hit;
	}

	
	int32  Step[3];
	double TMax[3];
	double TDelta[3];
	constexpr double Big = TNumericLimits<double>::Max();

	for (int32 a = 0; a < 3; ++a)
	{
		const double Da = D[a];
		if (FMath::Abs(Da) < 1e-12)
		{
			Step[a] = 0;
			TMax[a] = Big;     
			TDelta[a] = Big;
		}
		else
		{
			Step[a] = (Da > 0.0) ? 1 : -1;
			const double Boundary = (Da > 0.0) ? static_cast<double>(Cell[a] + 1) : static_cast<double>(Cell[a]);
			TMax[a] = (Boundary - O[a]) / Da;
			TDelta[a] = 1.0 / FMath::Abs(Da);
		}
	}

	const int32 MaxCells = 3 + FMath::CeilToInt32(FMath::Abs(D.X) + FMath::Abs(D.Y) + FMath::Abs(D.Z));

	for (int32 i = 0; i < MaxCells; ++i)
	{
		const int32 Axis = (TMax[0] < TMax[1])
			? ((TMax[0] < TMax[2]) ? 0 : 2)
			: ((TMax[1] < TMax[2]) ? 1 : 2);

		const double T = TMax[Axis];
		if (T > 1.0)
		{
			break;   
		}

		Cell[Axis] += Step[Axis];
		TMax[Axis] += TDelta[Axis];

		const EVoxelOccupancy Occ = Sample(Cell);
		if (Occ == EVoxelOccupancy::Unknown)
		{
			Hit.bUnknown = true;
			return Hit;
		}
		if (Occ == EVoxelOccupancy::Solid)
		{
			Hit.bHit = true;
			Hit.Voxel = Cell;
			Hit.Point = FromCm + (ToCm - FromCm) * T;
			FVector N = FVector::ZeroVector;
			N[Axis] = -Step[Axis];
			Hit.Normal = N;
			return Hit;
		}
	}

	return Hit;
}

bool FVoxelQueryScope::FindNearestExit(const FVector& PosCm, FVector& OutPoint, FVector& OutNormal)
{
	static const FIntVector Neighbors[6] = { {-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1} };

	const double VS = static_cast<double>(ChunkManager.VoxelSize);
	const FIntVector Cell = WorldToVoxel(PosCm);

	double BestDist = TNumericLimits<double>::Max();
	bool bFound = false;
	for (const FIntVector& Dir : Neighbors)
	{
		if (Sample(Cell + Dir) != EVoxelOccupancy::Air)
		{
			continue;
		}

		const int32 Axis = (Dir.X != 0) ? 0 : (Dir.Y != 0) ? 1 : 2;
		const int32 Sign = Dir[Axis];
		const double FaceCoord = static_cast<double>((Sign > 0) ? Cell[Axis] + 1 : Cell[Axis]) * VS;
		const double Dist = FMath::Abs(FaceCoord - PosCm[Axis]);

		if (Dist < BestDist)
		{
			BestDist = Dist;
			OutPoint = PosCm;
			OutPoint[Axis] = FaceCoord;
			OutNormal = FVector(Dir);
			bFound = true;
		}
	}
	if (bFound)
	{
		return true;
	}

	for (int32 k = 1; k <= 8; ++k)
	{
		const FIntVector Up = Cell + FIntVector(0, 0, k);
		if (Sample(Up) == EVoxelOccupancy::Air)
		{
			OutPoint = PosCm;
			OutPoint.Z = static_cast<double>(Up.Z) * VS;
			OutNormal = FVector::UpVector;
			return true;
		}
	}
	return false;
}