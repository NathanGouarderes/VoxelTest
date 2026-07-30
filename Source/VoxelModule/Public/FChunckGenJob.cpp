#include "FChunckGenJob.h"
#include "ChunckManager.h"

FChunkGenJob::FChunkGenJob(FIntVector InCoord, EChunkVariant InVariant, AChunckManager* Manager, int32 InLOD, int32 InGenerationId) : Coord(InCoord), Variant(InVariant), LOD(InLOD),  GenerationId(InGenerationId){
    if (Manager)
    {
        ChunkSize = Manager->ChunkSize;
        SurfaceAmplitude = Manager->SurfaceAmplitude;
        BaseHeight = Manager->BaseHeight;
        CaveFrequency = Manager->CaveFrequency;
        CaveThreshold = Manager->CaveThreshold;
        SeaLevel = Manager->SeaLevel;
        SurfaceWavelength = Manager->SurfaceWavelength;


        // On copie la configuration du bruit
        SurfaceNoise = Manager->SurfaceNoise;
        CaveNoise = Manager->CaveNoise;
    }
}