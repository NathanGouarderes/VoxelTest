#include "ChunckGenWorker.h"
#include "ChunckManager.h"
#include "FChunkGenResult.h"
#include "DebugMacro.h"
#include "VoxelWorld.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/CountersTrace.h"
#include <atomic>

// ============================================================================
//  Instrumentation des workers de generation
// ----------------------------------------------------------------------------
//  - Scopes Insights : Voxel_WorkerIdle (sommeil), Voxel_WorkerJob (un job
//    complet) et ses etapes ColumnHeights / Fill / Brush / Edits / Flags / Enqueue.
//  - Statistiques partagees par TOUS les workers (portee fichier), ecrites en
//    memory_order_relaxed : ce sont des mesures, rien ne se synchronise dessus.
//  - Un seul worker a la fois publie les compteurs Insights et ecrit le rapport
//    WORKERS dans le log (jeton atomique bPublishing).
// ============================================================================

static TAutoConsoleVariable<int32> CVarVoxelWorkerTelemetry(
	TEXT("voxel.Worker.Telemetry"),
	0,
	TEXT("1 = horodatages TDequeue/TGenerated par job. Prend TelemetryMutex : perturbe la mesure, laisser a 0 pendant les captures."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarVoxelWorkerLogInterval(
	TEXT("voxel.Worker.LogInterval"),
	10.0f,
	TEXT("Periode en secondes du rapport WORKERS dans le log. 0 = rapport desactive."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarVoxelWorkerCounterInterval(
	TEXT("voxel.Worker.CounterInterval"),
	0.25f,
	TEXT("Periode en secondes de publication des compteurs Insights des workers."),
	ECVF_Default);

TRACE_DECLARE_INT_COUNTER(VoxelWorker_JobsPerSecond, TEXT("Voxel/Worker/JobsPerSecond"));
TRACE_DECLARE_INT_COUNTER(VoxelWorker_BusyPermille, TEXT("Voxel/Worker/BusyPermille"));
TRACE_DECLARE_INT_COUNTER(VoxelWorker_AvgJobUs_LOD0, TEXT("Voxel/Worker/AvgJobUs_LOD0"));
TRACE_DECLARE_INT_COUNTER(VoxelWorker_AvgJobUs_LOD1, TEXT("Voxel/Worker/AvgJobUs_LOD1"));
TRACE_DECLARE_INT_COUNTER(VoxelWorker_AvgJobUs_LOD2, TEXT("Voxel/Worker/AvgJobUs_LOD2"));
TRACE_DECLARE_INT_COUNTER(VoxelWorker_AvgJobUs_LOD3, TEXT("Voxel/Worker/AvgJobUs_LOD3"));
TRACE_DECLARE_INT_COUNTER(VoxelWorker_DroppedJobs, TEXT("Voxel/Worker/DroppedJobs"));

namespace VoxelWorkerStats
{
	constexpr int32 NumLOD = 4;
	constexpr int32 NumPos = 3;   // 0 = Above, 1 = Crossing, 2 = Below
	constexpr int32 NumContent = 3;   // 0 = Empty, 1 = Solid,    2 = Mixed

	static const TCHAR* PosNames[NumPos] = { TEXT("Above"), TEXT("Crossing"), TEXT("Below") };

	static std::atomic<int64> JobCount[NumLOD];
	static std::atomic<int64> JobCycles[NumLOD];
	static std::atomic<int64> EditedJobs[NumLOD];
	static std::atomic<int64> BusyCycles;
	static std::atomic<int64> IdleCycles;
	static std::atomic<int64> Sleeps;
	static std::atomic<int64> DroppedJobs;
	static std::atomic<int64> ColumnCacheHits;
	static std::atomic<int64> ColumnCacheMisses;
	static std::atomic<int64> Matrix[NumLOD][NumPos][NumContent];

	static std::atomic<bool>   bPublishing{ false };
	static std::atomic<uint64> NextCounterCycle{ 0 };
	static std::atomic<uint64> NextLogCycle{ 0 };

	struct FSnapshot
	{
		uint64 Cycle = 0;
		int64  JobCount[NumLOD] = {};
		int64  JobCycles[NumLOD] = {};
		int64  Busy = 0;
		int64  Idle = 0;
		int64  Sleeps = 0;
		int64  Dropped = 0;
		int64  Hits = 0;
		int64  Misses = 0;
		int64  Matrix[NumLOD][NumPos][NumContent] = {};
		int64  Edited[NumLOD] = {};
	};

	// Lus et ecrits UNIQUEMENT par le detenteur du jeton bPublishing.
	static FSnapshot LastCounterSnap;
	static FSnapshot LastLogSnap;
	static bool      bSnapsInitialized = false;

	static FORCEINLINE void Add(std::atomic<int64>& Counter, int64 Value)
	{
		Counter.fetch_add(Value, std::memory_order_relaxed);
	}

	static void Take(FSnapshot& Out, uint64 NowCycle)
	{
		Out.Cycle = NowCycle;
		for (int32 L = 0; L < NumLOD; ++L)
		{
			Out.JobCount[L] = JobCount[L].load(std::memory_order_relaxed);
			Out.JobCycles[L] = JobCycles[L].load(std::memory_order_relaxed);
			Out.Edited[L] = EditedJobs[L].load(std::memory_order_relaxed);
			for (int32 P = 0; P < NumPos; ++P)
			{
				for (int32 C = 0; C < NumContent; ++C)
				{
					Out.Matrix[L][P][C] = Matrix[L][P][C].load(std::memory_order_relaxed);
				}
			}
		}
		Out.Busy = BusyCycles.load(std::memory_order_relaxed);
		Out.Idle = IdleCycles.load(std::memory_order_relaxed);
		Out.Sleeps = Sleeps.load(std::memory_order_relaxed);
		Out.Dropped = DroppedJobs.load(std::memory_order_relaxed);
		Out.Hits = ColumnCacheHits.load(std::memory_order_relaxed);
		Out.Misses = ColumnCacheMisses.load(std::memory_order_relaxed);
	}

	static void PublishCounters(const FSnapshot& Prev, const FSnapshot& Cur, double SecPerCycle)
	{
#if COUNTERSTRACE_ENABLED
		const double WindowSec = (double)(Cur.Cycle - Prev.Cycle) * SecPerCycle;
		if (WindowSec <= 0.0)
		{
			return;
		}

		int64 Jobs = 0;
		int64 AvgUs[NumLOD] = {};
		for (int32 L = 0; L < NumLOD; ++L)
		{
			const int64 DJobs = Cur.JobCount[L] - Prev.JobCount[L];
			Jobs += DJobs;
			AvgUs[L] = (DJobs > 0)
				? (int64)((double)(Cur.JobCycles[L] - Prev.JobCycles[L]) * SecPerCycle * 1.0e6 / (double)DJobs)
				: 0;
		}

		const int64 DBusy = Cur.Busy - Prev.Busy;
		const int64 DIdle = Cur.Idle - Prev.Idle;
		const int64 BusyPermille = (DBusy + DIdle > 0) ? (DBusy * 1000) / (DBusy + DIdle) : 0;

		TRACE_COUNTER_SET(VoxelWorker_JobsPerSecond, (int64)((double)Jobs / WindowSec));
		TRACE_COUNTER_SET(VoxelWorker_BusyPermille, BusyPermille);
		TRACE_COUNTER_SET(VoxelWorker_AvgJobUs_LOD0, AvgUs[0]);
		TRACE_COUNTER_SET(VoxelWorker_AvgJobUs_LOD1, AvgUs[1]);
		TRACE_COUNTER_SET(VoxelWorker_AvgJobUs_LOD2, AvgUs[2]);
		TRACE_COUNTER_SET(VoxelWorker_AvgJobUs_LOD3, AvgUs[3]);
		TRACE_COUNTER_SET(VoxelWorker_DroppedJobs, Cur.Dropped);
#endif
	}

	static void LogReport(const FSnapshot& Prev, const FSnapshot& Cur, double SecPerCycle)
	{
		const double WindowSec = (double)(Cur.Cycle - Prev.Cycle) * SecPerCycle;
		if (WindowSec <= 0.0)
		{
			return;
		}

		int64 Jobs = 0;
		for (int32 L = 0; L < NumLOD; ++L)
		{
			Jobs += Cur.JobCount[L] - Prev.JobCount[L];
		}

		const int64 DBusy = Cur.Busy - Prev.Busy;
		const int64 DIdle = Cur.Idle - Prev.Idle;
		const int64 DSleeps = Cur.Sleeps - Prev.Sleeps;
		const int64 DDropped = Cur.Dropped - Prev.Dropped;
		const int64 DHits = Cur.Hits - Prev.Hits;
		const int64 DMisses = Cur.Misses - Prev.Misses;

		const double BusyPct = (DBusy + DIdle > 0) ? 100.0 * (double)DBusy / (double)(DBusy + DIdle) : 0.0;
		const double HitPct = (DHits + DMisses > 0) ? 100.0 * (double)DHits / (double)(DHits + DMisses) : 0.0;

		// Ligne 1 : debit global. Occupation basse = workers affames (le goulot est en amont).
		UE_LOG(LogTemp, Display,
			TEXT("WORKERS | fenetre %.1fs | jobs=%lld (%.0f/s) | occupation=%.1f%% | sommeils=%lld | cache colonne=%.1f%% | abandonnes=%lld (total %lld)"),
			WindowSec, Jobs, (double)Jobs / WindowSec, BusyPct, DSleeps, HitPct, DDropped, Cur.Dropped);

		// Ligne 2 : cout moyen d'un job par LOD.
		FString PerLOD;
		for (int32 L = 0; L < NumLOD; ++L)
		{
			const int64 DJobs = Cur.JobCount[L] - Prev.JobCount[L];
			const double AvgUs = (DJobs > 0)
				? (double)(Cur.JobCycles[L] - Prev.JobCycles[L]) * SecPerCycle * 1.0e6 / (double)DJobs
				: 0.0;
			PerLOD += FString::Printf(TEXT(" | LOD%d n=%lld moy=%.0fus"), L, DJobs, AvgUs);
		}
		UE_LOG(LogTemp, Display, TEXT("WORKERS | temps par job%s"), *PerLOD);

		// Lignes 3+ : matrice position (par rapport a la surface) x contenu, par LOD.
		for (int32 L = 0; L < NumLOD; ++L)
		{
			int64 D[NumPos][NumContent] = {};
			int64 Total = 0;
			for (int32 P = 0; P < NumPos; ++P)
			{
				for (int32 C = 0; C < NumContent; ++C)
				{
					D[P][C] = Cur.Matrix[L][P][C] - Prev.Matrix[L][P][C];
					Total += D[P][C];
				}
			}
			if (Total == 0)
			{
				continue;
			}

			FString Line = FString::Printf(TEXT("WORKERS | LOD%d n=%lld"), L, Total);
			for (int32 P = 0; P < NumPos; ++P)
			{
				Line += FString::Printf(TEXT(" | %s E=%.1f%% S=%.1f%% M=%.1f%%"),
					PosNames[P],
					100.0 * (double)D[P][0] / (double)Total,
					100.0 * (double)D[P][1] / (double)Total,
					100.0 * (double)D[P][2] / (double)Total);
			}
			UE_LOG(LogTemp, Display, TEXT("%s"), *Line);

			// Garde-fou : la classification utilise EXACTEMENT les hauteurs du remplissage.
			// Un chunk Above non vide ne peut donc venir que de pinceaux/editions en altitude,
			// ou d'un bug. Dans les deux cas, on veut le voir.
			const int64 AboveNotEmpty = D[0][1] + D[0][2];
			if (AboveNotEmpty > 0)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("WORKERS | LOD%d : %lld chunk(s) Above NON vides (pinceaux/editions en altitude, ou bug de classification)."),
					L, AboveNotEmpty);
			}

			
		}
		FString EditedLine;
		for (int32 L = 0; L < NumLOD; ++L)
		{
			EditedLine += FString::Printf(TEXT(" | LOD%d n=%lld"), L, Cur.Edited[L] - Prev.Edited[L]);
		}
		UE_LOG(LogTemp, Display, TEXT("WORKERS | jobs edites (hors matrice)%s"), *EditedLine);
	}

	static void MaybePublish()
	{
		const uint64 Now = FPlatformTime::Cycles64();
		if (Now < NextCounterCycle.load(std::memory_order_relaxed) &&
			Now < NextLogCycle.load(std::memory_order_relaxed))
		{
			return;   // chemin rapide : deux lectures relaxed, rien d'autre
		}

		bool bExpected = false;
		if (!bPublishing.compare_exchange_strong(bExpected, true, std::memory_order_acquire))
		{
			return;   // un autre worker publie deja
		}

		const double SecPerCycle = FPlatformTime::GetSecondsPerCycle64();
		auto SecondsToCycles = [SecPerCycle](double Seconds) -> uint64
			{
				return (uint64)(FMath::Max(Seconds, 0.01) / SecPerCycle);
			};

		const float CounterInterval = CVarVoxelWorkerCounterInterval.GetValueOnAnyThread();
		const float LogInterval = CVarVoxelWorkerLogInterval.GetValueOnAnyThread();

		FSnapshot Cur;
		Take(Cur, Now);

		if (!bSnapsInitialized)
		{
			LastCounterSnap = Cur;
			LastLogSnap = Cur;
			bSnapsInitialized = true;
			NextCounterCycle.store(Now + SecondsToCycles(CounterInterval), std::memory_order_relaxed);
			NextLogCycle.store(Now + SecondsToCycles(LogInterval > 0.f ? LogInterval : 1.0), std::memory_order_relaxed);
			bPublishing.store(false, std::memory_order_release);
			return;
		}

		if (Now >= NextCounterCycle.load(std::memory_order_relaxed))
		{
			PublishCounters(LastCounterSnap, Cur, SecPerCycle);
			LastCounterSnap = Cur;
			NextCounterCycle.store(Now + SecondsToCycles(CounterInterval), std::memory_order_relaxed);
		}

		if (Now >= NextLogCycle.load(std::memory_order_relaxed))
		{
			if (LogInterval > 0.f)
			{
				LogReport(LastLogSnap, Cur, SecPerCycle);
			}
			LastLogSnap = Cur;
			NextLogCycle.store(Now + SecondsToCycles(LogInterval > 0.f ? LogInterval : 1.0), std::memory_order_relaxed);
		}

		bPublishing.store(false, std::memory_order_release);
	}
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
	using namespace VoxelWorkerStats;

	// Cache de colonne PROPRE A CE THREAD (variables locales de Run) : aucun partage,
	// aucun verrou. Clef complete = colonne XY + LOD + config : une config remplacee
	// invalide le cache au lieu de servir des hauteurs perimees.
	FIntPoint             ColCacheXY(MAX_int32, MAX_int32);
	int32                 ColCacheLOD = INDEX_NONE;
	const FTerrainConfig* ColCacheConfig = nullptr;
	TArray<float>         ColHeights;
	TArray<float>         ColProvinces;

	while (!bStopRequest)
	{
		MaybePublish();

		FChunkGenJob Job;
		bool bGotJob = false;

		if (ChunckManager)
		{
			FScopeLock Lock(&ChunckManager->DequeueMutex);
			bGotJob = JobQueue.Dequeue(Job);
		}

		if (!bGotJob)
		{
			// File vide : le worker n'a RIEN a faire. Ce temps mesure la famine, pas un frein.
			TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_WorkerIdle);
			const uint64 IdleStart = FPlatformTime::Cycles64();
			FPlatformProcess::Sleep(0.01f);
			Add(IdleCycles, (int64)(FPlatformTime::Cycles64() - IdleStart));
			Add(Sleeps, 1);
			continue;
		}

		const uint64 JobStart = FPlatformTime::Cycles64();
		const int32  StatLOD = FMath::Clamp(Job.RenderLOD, 0, NumLOD - 1);
		bool bProduced = false;

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_WorkerJob);

			const bool bTelemetry = CVarVoxelWorkerTelemetry.GetValueOnAnyThread() != 0;
			if (bTelemetry)
			{
				ChunckManager->TelemetryStamp(Job.Coord, &FChunkJobTelemetry::TDequeue, FPlatformTime::Seconds());
			}

			Generator.SetConfig(Job.SharedFTerrainConfig);
			if (!Generator.IsReady())
			{
				// Comportement inchange (job abandonne), mais plus jamais silencieux :
				// un job abandonne = un chunk qui ne sera JAMAIS genere.
				const int64 Dropped = DroppedJobs.fetch_add(1, std::memory_order_relaxed) + 1;
				if (Dropped <= 20)
				{
					UE_LOG(LogTemp, Error,
						TEXT("WORKER : job (%d,%d,%d) LOD%d gen%d ABANDONNE, generateur non pret (config %s). Ce chunk ne sera jamais genere."),
						Job.Coord.X, Job.Coord.Y, Job.Coord.Z, Job.RenderLOD, Job.GenerationId,
						Job.SharedFTerrainConfig.IsValid() ? TEXT("valide") : TEXT("NULLE"));
				}
			}
			else
			{
				const int32 ChunckSize = Job.ChunkSize;
				const int32 SubSize = ChunckSize >> Job.RenderLOD;
				const int32 Step = 1 << Job.RenderLOD;
				const int32 TotalSize = SubSize * SubSize * SubSize;

				const int32 OX = Job.Coord.X * ChunckSize;
				const int32 OY = Job.Coord.Y * ChunckSize;
				const int32 OZ = Job.Coord.Z * ChunckSize;

				TArray<FVoxelDataStructure> LocalVoxel;
				LocalVoxel.SetNumZeroed(TotalSize);

				// --- 0) Hauteurs et provinces de la colonne, mises en cache. ---
				const FIntPoint       ColumnXY(Job.Coord.X, Job.Coord.Y);
				const FTerrainConfig* ConfigRaw = Job.SharedFTerrainConfig.Get();
				const bool bHit = (ColumnXY == ColCacheXY)
					&& (Job.RenderLOD == ColCacheLOD)
					&& (ConfigRaw == ColCacheConfig)
					&& (ColHeights.Num() == SubSize * SubSize);

				Add(bHit ? ColumnCacheHits : ColumnCacheMisses, 1);

				if (!bHit)
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_WorkerColumnHeights);
					ColCacheXY = ColumnXY;
					ColCacheLOD = Job.RenderLOD;
					ColCacheConfig = ConfigRaw;
					ColHeights.SetNumUninitialized(SubSize * SubSize);
					ColProvinces.SetNumUninitialized(SubSize * SubSize);

					for (int32 x = 0; x < SubSize; ++x)
					{
						for (int32 y = 0; y < SubSize; ++y)
						{
							const int32 gx = OX + x * Step;
							const int32 gy = OY + y * Step;
							const int32 i = x + y * SubSize;
							ColHeights[i] = Generator.ComputeHeight((float)gx, (float)gy);
							ColProvinces[i] = Generator.ComputeProvince((float)gx, (float)gy);
						}
					}
				}

				// Bornes de la colonne : classent le chunk par rapport a la surface.
				float MinH = TNumericLimits<float>::Max();
				float MaxH = TNumericLimits<float>::Lowest();
				for (const float H : ColHeights)
				{
					MinH = FMath::Min(MinH, H);
					MaxH = FMath::Max(MaxH, H);
				}

				// --- 1) Remplissage. Une colonne entierement sous OZ est de l'air : on la saute. ---
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_WorkerFill);
					for (int32 x = 0; x < SubSize; ++x)
					{
						for (int32 y = 0; y < SubSize; ++y)
						{
							const int32 ci = x + y * SubSize;
							const float Height = ColHeights[ci];
							if (Height <= (float)OZ)
							{
								continue;
							}

							const int32 gx = OX + x * Step;
							const int32 gy = OY + y * Step;
							const float Province = ColProvinces[ci];

							for (int32 z = 0; z < SubSize; ++z)
							{
								const int32 gz = OZ + z * Step;
								const int32 Index = x + y * SubSize + z * SubSize * SubSize;
								LocalVoxel[Index].Material.Id =
									Generator.MaterialIdAt(Province, Height, gx, gy, gz, Job.RenderLOD);
							}
						}
					}
				}

				// --- 2) Pinceaux, tries par Seq (le dernier gagne). ---
				if (Job.BrushOps.Num() > 0)
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_WorkerBrush);
					Job.BrushOps.Sort([](const FVoxelBrushOp& A, const FVoxelBrushOp& B)
						{
							return A.Seq < B.Seq;
						});
					for (const FVoxelBrushOp& Op : Job.BrushOps)
					{
						AChunckManager::ApplyBrushOp(LocalVoxel, Job.Coord, Job.RenderLOD, ChunckSize, Op);
					}
				}

				// --- 3) Editions ponctuelles (priorite maximale). ---
				if (Job.Edits.Num() > 0)
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_WorkerEdits);
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

				if (bTelemetry)
				{
					ChunckManager->TelemetryStamp(Job.Coord, &FChunkJobTelemetry::TGenerated, FPlatformTime::Seconds());
				}

				// --- 4) Drapeaux, APRES pinceaux et editions. ---
				bool bIsAllEmpty = true;
				bool bIsAllSolid = true;
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_WorkerFlags);
					for (const FVoxelDataStructure& V : LocalVoxel)
					{
						if (V.Material.Id != 0) bIsAllEmpty = false;
						else                    bIsAllSolid = false;
						if (!bIsAllEmpty && !bIsAllSolid) break;
					}
				}

				// --- 5) Classification position x contenu (matrice du rapport WORKERS). ---
				//  Above    : tous les voxels gz >= OZ >= MaxH >= Height -> forcement de l'air.
				//  Below    : le voxel le plus haut (OZ + ChunckSize - Step) est sous MinH.
				//  Crossing : la surface traverse le chunk.
				int32 Pos = 1;
				if ((float)OZ >= MaxH)
				{
					Pos = 0;
				}
				else if ((float)(OZ + ChunckSize - Step) < MinH)
				{
					Pos = 2;
				}
				const int32 Content = bIsAllEmpty ? 0 : (bIsAllSolid ? 1 : 2);
				const bool bEdited = Job.BrushOps.Num() > 0 || Job.Edits.Num() > 0;
				if (bEdited) Add(EditedJobs[StatLOD], 1);
				else         Add(Matrix[StatLOD][Pos][Content], 1);

				// --- 6) Resultat vers le game thread. ---
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_WorkerEnqueue);
					FChunkGenResult Result;
					Result.Coord = Job.Coord;
					Result.LOD = Job.RenderLOD;
					Result.GenerationId = Job.GenerationId;
					Result.Voxels = MoveTemp(LocalVoxel);
					Result.bIsAllEmpty = bIsAllEmpty;
					Result.bIsAllSolid = bIsAllSolid;
					ChunckManager->ChunckGenerationResult.Enqueue(MoveTemp(Result));
				}

				bProduced = true;
			}
		}

		const int64 JobCyc = (int64)(FPlatformTime::Cycles64() - JobStart);
		Add(BusyCycles, JobCyc);
		if (bProduced)
		{
			Add(JobCycles[StatLOD], JobCyc);
			Add(JobCount[StatLOD], 1);
		}
	}
	return 0;
}

void ChunckGenWorker::Stop()
{
	FScopeLock Lock(&StopMutex);
	bStopRequest = true;
}

void ChunckGenWorker::ResetStats()
{
	using namespace VoxelWorkerStats;

	for (int32 L = 0; L < NumLOD; ++L)
	{
		JobCount[L].store(0, std::memory_order_relaxed);
		JobCycles[L].store(0, std::memory_order_relaxed);
		EditedJobs[L].store(0, std::memory_order_relaxed);
		for (int32 P = 0; P < NumPos; ++P)
		{
			for (int32 C = 0; C < NumContent; ++C)
			{
				Matrix[L][P][C].store(0, std::memory_order_relaxed);
			}
		}
	}
	BusyCycles.store(0, std::memory_order_relaxed);
	IdleCycles.store(0, std::memory_order_relaxed);
	Sleeps.store(0, std::memory_order_relaxed);
	DroppedJobs.store(0, std::memory_order_relaxed);
	ColumnCacheHits.store(0, std::memory_order_relaxed);
	ColumnCacheMisses.store(0, std::memory_order_relaxed);

	NextCounterCycle.store(0, std::memory_order_relaxed);
	NextLogCycle.store(0, std::memory_order_relaxed);
	bPublishing.store(false, std::memory_order_relaxed);
	bSnapsInitialized = false;
	LastCounterSnap = FSnapshot();
	LastLogSnap = FSnapshot();

	UE_LOG(LogTemp, Display, TEXT("WORKERS | statistiques remises a zero"));
}