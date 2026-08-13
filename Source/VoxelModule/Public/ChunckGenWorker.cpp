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
        if (ChunckManager)
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
        const int32 SubSize = ChunckSize >> Job.LOD;
        const int32 Step = 1 << Job.LOD;
        const int32 TotalSize = SubSize * SubSize * SubSize;
        TArray<FVoxelDataStructure> LocalVoxel;
        LocalVoxel.SetNumZeroed(TotalSize);
        bool bIsAllSolid = true;
        bool bIsAllEmpty = true;
        for (int x = 0; x < SubSize; x++)
        {
            for (int y = 0; y < SubSize; y++)
            {
                float WorldX = (Job.Coord.X * ChunckSize + x * Step);
                float WorldY = (Job.Coord.Y * ChunckSize + y * Step);
                float SurfaceNoiseValue = Job.SurfaceNoise.GetNoise(WorldX / Job.SurfaceWavelength, WorldY / Job.SurfaceWavelength);
                int GlobalSurfaceHeigh = Job.BaseHeight + FMath::FloorToInt(SurfaceNoiseValue * Job.SurfaceAmplitude);
                for (int z = 0; z < SubSize; z++)
                {
                    int Index = x + y * SubSize + z * SubSize * SubSize;
                    int GlobalZ = Job.Coord.Z * ChunckSize + z * Step;
                    bool IsSolid = (GlobalZ < GlobalSurfaceHeigh);
                    if (IsSolid)
                    {
                        float CaveNoiseValue = Job.CaveNoise.GetNoise((Job.Coord.X * ChunckSize + x * Step) * Job.CaveFrequency, (Job.Coord.Y * ChunckSize + y * Step) * Job.CaveFrequency, GlobalZ * Job.CaveFrequency);
                        if (CaveNoiseValue > Job.CaveThreshold)
                        {
                            IsSolid = false;
                        }
                    }
                    LocalVoxel[Index].Material.Id = IsSolid ? 1 : 0;
                    if (IsSolid)
                    {
                        bIsAllEmpty = false;
                    }
                    else
                    {
                        bIsAllSolid = false;
                    }
                }
            }
        }
        if (ChunckManager)
        {
            FChunkGenResult Result;
            Result.Coord = Job.Coord;
            Result.LOD = Job.LOD;
            Result.GenerationId = Job.GenerationId;
            Result.Voxels = MoveTemp(LocalVoxel);
            Result.bIsAllEmpty = bIsAllEmpty;
            Result.bIsAllSolid = bIsAllSolid;
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


int8 ChunckGenWorker::EvaluateNoiseSolid(int32 gx, int32 gy, int32 gz, FastNoiseLite& SurfaceNoise, FastNoiseLite& CaveNoise,
float SurfaceAmplitude, float SurfaceWavelength, float SurfaceAmplitude, int32 BaseHeight, float CaveFrequency, float CaveThreshold)
{
	float SurfaceNoiseValue = SurfaceNoise.GetNoise(gx / SurfaceWavelength, gy / SurfaceWavelength);
	int SurfaceHeight = BaseHeight + FMath::FloorToInt(SurfaceNoiseValue * SurfaceAmplitude);

	if(gz >= SurfaceHeight)
	{
		return false;
	}
	float CaveNoiseValue = CaveNoise.GetNoise(gx * CaveFrequency, gy * CaveFrequency, gz * CaveFrequency);
	if(CaveNoiseValue > CaveThreshold)
	{
		return false;
	}
	return true;
}
