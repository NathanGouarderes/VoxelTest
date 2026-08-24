// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/Event.h"
#include "Containers/Queue.h"
#include "../../VoxelModule/Public/Structs/FTerrainConfig.h"
#include "Structs/FTerrainGenerator.h"
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

private:
	void Reconfigure();
	float ComputeHeight(float WorldX, float WorldY) const;

	FCriticalSection StopMutex;
	AChunckManager* ChunckManager;
	TQueue<FChunkGenJob, EQueueMode::Mpsc>& JobQueue;
	FTerrainGenerator Generator;
	bool bStopRequest;
	//FCriticalSection DequeueMutex;
};
