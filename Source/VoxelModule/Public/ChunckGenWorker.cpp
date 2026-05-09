// Fill out your copyright notice in the Description page of Project Settings.


#include "ChunckGenWorker.h"
#include "ChunckManager.h"
#include "FChunkGenResult.h"
#include "VoxelWorld.h"

ChunckGenWorker::ChunckGenWorker(AChunckManager* InManager, TQueue<FChunkGenJob, EQueueMode::Mpsc>& InQueue)
	: bStopRequest(false)
	, ChunckManager(InManager)
	, JobQueue(InQueue)
{
}

ChunckGenWorker::~ChunckGenWorker()
{
}

bool ChunckGenWorker::Init()
{
	return true;
}

uint32 ChunckGenWorker::Run()
{
	while (!bStopRequest)
	{
		FChunkGenJob Job;
		bool bGotJob = false;

		if(ChunckManager)
		{
			FScopeLock Lock(&ChunckManager->DequeueMutex);
			bGotJob = JobQueue.Dequeue(Job);
		}

		if (!bGotJob)
		{
			FPlatformProcess::Sleep(0.01f);
			continue;
		}

		const int32 ChunckSize = Job.ChunkSize;
		const int32 TotalSize = ChunckSize * ChunckSize * ChunckSize;

		TArray<FVoxelDataStructure> LocalVoxel;
		LocalVoxel.SetNumZeroed(TotalSize);

		for (int x = 0; x < ChunckSize; x++)
		{
			for (int y = 0; y < ChunckSize; y++)
			{
				float WorldX = (Job.Coord.X * ChunckSize + x) * Job.SurfaceFrequency;
				float WorldY = (Job.Coord.Y * ChunckSize + y) * Job.SurfaceFrequency;

				float SurfaceNoiseValue = Job.SurfaceNoise.GetNoise(WorldX, WorldY);
				int GlobalSurfaceHeigh = Job.BaseHeight + FMath::FloorToInt(SurfaceNoiseValue * Job.SurfaceAmplitude);

				for (int z = 0; z < ChunckSize; z++)
				{
					int Index = x + y * ChunckSize + z * ChunckSize * ChunckSize;
					int GlobalZ = Job.Coord.Z * ChunckSize + z;

					bool IsSolid = (GlobalZ < GlobalSurfaceHeigh);
					if (IsSolid)
					{
						float CaveNoiseValue = Job.CaveNoise.GetNoise((Job.Coord.X * ChunckSize + x) * Job.CaveFrequency, (Job.Coord.Y * ChunckSize + y) * Job.CaveFrequency, GlobalZ * Job.CaveFrequency);

						if (CaveNoiseValue > Job.CaveThreshold)
						{
							IsSolid = false;
						}
					}
					LocalVoxel[Index].Material.Id = IsSolid ? 1 : 0;
				}
			}
		}
		if (ChunckManager)
		{
			FChunkGenResult Result;
			Result.Coord = Job.Coord;
			Result.Voxels = MoveTemp(LocalVoxel);
			ChunckManager->ChunckGenerationResult.Enqueue(MoveTemp(Result));
		}
	}
	return 0;
}

void ChunckGenWorker::Stop()
{
	FScopeLock Lock(&StopMutex);
	bStopRequest = true;
}

