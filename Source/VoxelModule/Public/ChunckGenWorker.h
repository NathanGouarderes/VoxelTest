// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/Event.h"
#include "FastNoiseLite.h"
#include "Containers/Queue.h"
#include "FChunckGenJob.h"
#include "EChunkVariant.h"

class AChunckManager;

/**
 * 
 */
class VOXELMODULE_API ChunckGenWorker : public FRunnable
{
public:
	ChunckGenWorker(AChunckManager* InManager, TQueue<FChunkGenJob, EQueueMode::Mpsc>& InQueue);
	virtual ~ChunckGenWorker();

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	static bool EvaluateNoiseSolid(int32 gx, int32 gy, int32 gz, FastNoiseLite SurfaceNoise, FastNoiseLite CaveNoise,
float SurfaceAmplitude, float SurfaceWavelength, int32 BaseHeight, float CaveFrequency, float CaveThreshold);

private:
	FCriticalSection StopMutex;
	AChunckManager* ChunckManager;
	TQueue<FChunkGenJob, EQueueMode::Mpsc>& JobQueue;
	bool bStopRequest;
	//FCriticalSection DequeueMutex;
};
