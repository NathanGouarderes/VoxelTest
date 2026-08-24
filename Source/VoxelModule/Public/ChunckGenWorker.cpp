#include "ChunckGenWorker.h"
#include "ChunckManager.h"
#include "FChunkGenResult.h"
#include "DebugMacro.h"
#include "VoxelWorld.h"

static FORCEINLINE uint32 HashCell(int32 cx, int32 cy, int32 cz, uint32 Seed)
{
    uint32 h = Seed;
    h ^= (uint32)cx * 0x9E3779B1u;   // nombre d'or en 32 bits
    h ^= (uint32)cy * 0x85EBCA77u;
    h ^= (uint32)cz * 0xC2B2AE3Du;
    h ^= h >> 15;   // avalanche : diffuse les bits de poids fort
    h *= 0x2545F491u;
    h ^= h >> 13;
    return h;
}
ChunckGenWorker::ChunckGenWorker(AChunckManager* InManager, TQueue<FChunkGenJob, EQueueMode::Mpsc>& InQueue)
    : ChunckManager(InManager)
    , JobQueue(InQueue)
    , bStopRequest(false)
{
}

ChunckGenWorker::~ChunckGenWorker() {}

bool ChunckGenWorker::Init() { return true; }

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
        ChunckManager->TelemetryStamp(Job.Coord, &FChunkJobTelemetry::TDequeue, FPlatformTime::Seconds());

        Generator.SetConfig(Job.SharedFTerrainConfig);
        if (!Generator.IsReady())
        {
            continue;
        }

        const int32 ChunckSize = Job.ChunkSize;
        const int32 SubSize = ChunckSize >> Job.RenderLOD;
        const int32 Step = 1 << Job.RenderLOD;
        const int32 TotalSize = SubSize * SubSize * SubSize;

        TArray<FVoxelDataStructure> LocalVoxel;
        LocalVoxel.SetNumZeroed(TotalSize);

        const bool bHasBrush = Job.BrushOps.Num() > 0;
        const bool bHasEdits = Job.Edits.Num() > 0;

        const int32 OX = Job.Coord.X * ChunckSize;
        const int32 OY = Job.Coord.Y * ChunckSize;
        const int32 OZ = Job.Coord.Z * ChunckSize;

        // --- 1) Bruit. Hauteur calculee UNE fois par colonne. ---
        for (int32 x = 0; x < SubSize; ++x)
        {
            for (int32 y = 0; y < SubSize; ++y)
            {
                const int32 gx = OX + x * Step;
                const int32 gy = OY + y * Step;
                const float Height = Generator.ComputeHeight((float)gx, (float)gy);
                const float Province = Generator.ComputeProvince((float)gx, (float)gy);
                for (int32 z = 0; z < SubSize; ++z)
                {
                    const int32 gz = OZ + z * Step;
                    const int32 Index = x + y * SubSize + z * SubSize * SubSize;
                    LocalVoxel[Index].Material.Id = Generator.MaterialIdAt(Province, Height, gx, gy, gz);
                }
            }
        }

        // --- 2) Pinceaux, tries par Seq. ---
        if (bHasBrush)
        {
            Job.BrushOps.Sort([](const FVoxelBrushOp& A, const FVoxelBrushOp& B)
                {
                    return A.Seq < B.Seq;
                });
            for (const FVoxelBrushOp& Op : Job.BrushOps)
            {
                AChunckManager::ApplyBrushOp(LocalVoxel, Job.Coord, Job.RenderLOD, ChunckSize, Op);
            }
        }

        // --- 3) Editions ponctuelles. ---
        if (bHasEdits)
        {
            for (const TPair<int32, FVoxelDataStructure>& P : Job.Edits)
            {
                const int32 Idx0 = P.Key;
                const int32 lz = Idx0 / (ChunckSize * ChunckSize);
                const int32 ly = (Idx0 / ChunckSize) % ChunckSize;
                const int32 lx = Idx0 % ChunckSize;

                // Hors de la grille de CE LOD : aucune representation possible.
                if ((lx % Step) != 0 || (ly % Step) != 0 || (lz % Step) != 0)
                {
                    continue;
                }
                const int32 Index = (lx >> Job.RenderLOD)
                    + (ly >> Job.RenderLOD) * SubSize
                    + (lz >> Job.RenderLOD) * SubSize * SubSize;
                if (!LocalVoxel.IsValidIndex(Index))
                {
                    continue;
                }
                LocalVoxel[Index] = P.Value;
            }
        }
        ChunckManager->TelemetryStamp(Job.Coord, &FChunkJobTelemetry::TGenerated, FPlatformTime::Seconds());

        // --- 4) Drapeaux, APRES pinceaux et editions. ---
        bool bIsAllEmpty = true;
        bool bIsAllSolid = true;
        for (const FVoxelDataStructure& V : LocalVoxel)
        {
            if (V.Material.Id != 0) bIsAllEmpty = false;
            else                    bIsAllSolid = false;
            if (!bIsAllEmpty && !bIsAllSolid) break;
        }

        if (ChunckManager)
        {
            FChunkGenResult Result;
            Result.Coord = Job.Coord;
            Result.LOD = Job.RenderLOD;
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