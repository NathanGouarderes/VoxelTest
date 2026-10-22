#include "../../Public/Structs/FDesiredSetBuilder.h"
#include "Async/ParallelFor.h"
#include "../../Public/ChunckManager.h"


void FDesiredSetBuilder::Compute(const FDesiredRequest& Req, FDesiredResult& Out)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_DesiredCompute);
    const double Start = FPlatformTime::Seconds();

    Generator.SetConfig(Req.Config);
    Out.bValid = Generator.IsReady();
    if (!Out.bValid) return;   // JAMAIS d'ensemble vide applique : il dechargerait tout le monde
    TSet<FIntPoint> MissingSet;
    const int32 HR = FMath::Max(0, Req.HR);
    const int32 VR = FMath::Max(0, Req.VR);
    const int32 HRSq = HR * HR;
    for (const FDesiredPlayer& P : Req.Players)
        for (int32 dx = -HR; dx <= HR; ++dx)
            for (int32 dy = -HR; dy <= HR; ++dy)
            {
                if (dx * dx + dy * dy > HRSq) continue;
                const FIntPoint Column(P.Center.X + dx, P.Center.Y + dy);
                if (!ColumnCache.Contains(Column)) MissingSet.Add(Column);
            }
    TArray<FIntPoint> Missing = MissingSet.Array();

    // 2) Calcule-les en parallele. Chaque iteration ecrit dans SA case : aucune ecriture partagee.
    TArray<FColumnBounds> Computed;
    Computed.SetNum(Missing.Num());
    ParallelFor(Missing.Num(), [&](int32 i)
        {
            Computed[i] = AChunckManager::ComputeColumnBounds(Generator, Missing[i], Req.ChunkSize);
        });

    // 3) Range les resultats dans le cache (sequentiel de nouveau).
    for (int32 i = 0; i < Missing.Num(); ++i)
    {
        ColumnCache.Add(Missing[i], Computed[i]);
        Out.NewColumns.Add(TPair<FIntPoint, FColumnBounds>(Missing[i], Computed[i]));
    }

    for (const FDesiredPlayer& P : Req.Players)
    {
        for (int32 dx = -HR; dx <= HR; ++dx)
            for (int32 dy = -HR; dy <= HR; ++dy)
            {
                if (dx * dx + dy * dy > HRSq) continue;   // disque horizontal

                const FIntPoint Column(P.Center.X + dx, P.Center.Y + dy);
                FColumnBounds Bounds;
                if (const FColumnBounds* Found = ColumnCache.Find(Column))
                {
                    Bounds = *Found;
                }
                else
                {
                    Bounds = AChunckManager::ComputeColumnBounds(Generator, Column, Req.ChunkSize);
                    ColumnCache.Add(Column, Bounds);
                    Out.NewColumns.Add(TPair<FIntPoint, FColumnBounds>(Column, Bounds));
                }

                for (int32 dz = -VR; dz <= VR; ++dz)
                {
                    const FIntVector Coord = P.Center + FIntVector(dx, dy, dz);
                    if (Coord.Z * Req.ChunkSize >= Req.MaxSurfaceVoxel) continue;          // IsChunkGuaranteedEmpty

                    if (!Req.Edited.Contains(Coord) && (float)(Coord.Z * Req.ChunkSize) >= Bounds.MaxH)
                    {
                        ++Out.CulledAir;
                        continue;
                    }

                    const int32 LOD = AChunckManager::ComputeLODForChunk(Coord, P.Pos, Req.ChunkSize,
                        Req.VoxelSize, Req.LODDistances, Req.MaxLOD);
                    if (int32* Cur = Out.Desired.Find(Coord)) *Cur = FMath::Min(*Cur, LOD);
                    else Out.Desired.Add(Coord, LOD);
                }
            }
    }

    // Tri par distance : le game thread spawnera dans cet ordre, sans trier lui-meme.
    Out.Desired.GenerateKeyArray(Out.SortedCoords);
    const FIntVector C = Req.SortCenter;
    Out.SortedCoords.Sort([C](const FIntVector& A, const FIntVector& B)
        {
            const FIntVector DA = A - C;
            const FIntVector DB = B - C;
            return (DA.X * DA.X + DA.Y * DA.Y + DA.Z * DA.Z) < (DB.X * DB.X + DB.Y * DB.Y + DB.Z * DB.Z);
        });

    Out.ColumnCacheSize = ColumnCache.Num();
    Out.ComputeMs = (FPlatformTime::Seconds() - Start) * 1000.0;
}
