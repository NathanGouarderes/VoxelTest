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
		{
			FScopeLock Lock(&StopMutex);
			if (bStopRequest)
			{
				break;
			}
		}

		FChunkGenJob Job;
		if (!JobQueue.Dequeue(Job))
		{
			FPlatformProcess::Sleep(0.002f);
			continue;
		}

		if (!ChunckManager)
		{
			continue;
		}

		const int32 ChunckSize = ChunckManager->ChunkSize;
		const int32 TotalSize = ChunckSize * ChunckSize * ChunckSize;

		TArray<FVoxelDataStructure> LocalVoxel;
		LocalVoxel.SetNumZeroed(TotalSize);

		for (int x = 0; x < ChunckSize; x++)
		{
			for (int y = 0; y < ChunckSize; y++)
			{
				float WorldX = (Job.Coord.X * ChunckSize + x) * ChunckManager->SurfaceFrequency;
				float WorldY = (Job.Coord.Y * ChunckSize + y) * ChunckManager->SurfaceFrequency;

				float SurfaceNoiseValue = ChunckManager->SurfaceNoise.GetNoise(WorldX, WorldY);
				int GlobalSurfaceHeigh = ChunckManager->BaseHeight + FMath::FloorToInt(SurfaceNoiseValue * ChunckManager->SurfaceAmplitude);

				for (int z = 0; z < ChunckSize; z++)
				{
					int Index = x + y * ChunckSize + z * ChunckSize * ChunckSize;
					int GlobalZ = Job.Coord.Z * ChunckSize + z;

					bool IsSolid = (GlobalZ < GlobalSurfaceHeigh);
					if (IsSolid)
					{
						float CaveNoiseValue = ChunckManager->CaveNoise.GetNoise((Job.Coord.X * ChunckSize + x) * ChunckManager->CaveFrequency, (Job.Coord.Y * ChunckSize + y) * ChunckManager->CaveFrequency, GlobalZ * ChunckManager->CaveFrequency);

						if (CaveNoiseValue > ChunckManager->CaveThreshold)
						{
							IsSolid = false;
						}
					}
					LocalVoxel[Index].Material.Id = IsSolid ? 1 : 0;
				}
			}
		}
		TWeakObjectPtr<AChunckManager> WeakManager = ChunckManager;
		FChunkGenResult Result;
		Result.Coord = Job.Coord;
		Result.Voxels = MoveTemp(LocalVoxel);
		ChunckManager->ChunckGenerationResult.Enqueue(MoveTemp(Result));
		//Retour dans le gamethread
		/*
		AsyncTask(ENamedThreads::GameThread, [WeakManager, Coord = Job.Coord, LocalVoxel = MoveTemp(LocalVoxel)]() mutable
			{
				AChunckManager* Manager = WeakManager.Get();
				if (!WeakManager->VoxelWorld)
				{
					UE_LOG(LogTemp, Error, TEXT(" ChunckGenWorker::Run() --> ChunckManager ou VoxelWorld NULL"));
					return;
				}
				if (!Manager || !Manager->VoxelWorld)
				{
					UE_LOG(LogTemp, Error, TEXT(" ChunckGenWorker::Run() --> Manager ou VoxelWorld NULL"));
					return;
				}

				FChunckDataStructure* ChunckData = WeakManager->VoxelWorld->Chuncks.Find(Coord);

				if (!ChunckData)
				{
					return;
				}

				
				//ChunckData->Voxels = MoveTemp(LocalVoxel);
				//ChunckData->bIsChunckGenerated = true;

				//WeakManager->DirtyChuncks.Add(Coord);
		});
		*/
		
		
	}
	return 0;
}

void ChunckGenWorker::Stop()
{
	FScopeLock Lock(&StopMutex);
	bStopRequest = true;
}

