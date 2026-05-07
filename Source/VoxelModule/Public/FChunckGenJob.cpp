#include "FChunckGenJob.h"
#include "ChunckManager.h"

FChunkGenJob::FChunkGenJob(FIntVector InCoord, EChunkVariant InVariant, AChunckManager* Manager) : Coord(InCoord), Variant(InVariant) {
    if (Manager)
    {
        ChunkSize = Manager->ChunkSize;
        SurfaceFrequency = Manager->SurfaceFrequency;
        SurfaceAmplitude = Manager->SurfaceAmplitude;
        BaseHeight = Manager->BaseHeight;
        CaveFrequency = Manager->CaveFrequency;
        CaveThreshold = Manager->CaveThreshold;
        SeaLevel = Manager->SeaLevel;

        // On copie la configuration du bruit
        SurfaceNoise = Manager->SurfaceNoise;
        CaveNoise = Manager->CaveNoise;
    }
}