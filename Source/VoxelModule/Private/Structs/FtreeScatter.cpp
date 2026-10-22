// Fill out your copyright notice in the Description page of Project Settings.


#include "../../Public/Structs/FtreeScatter.h"

#include "VoxelMath.h"

void FTreeScatterer::Reconfigure()
{
    ForestNoise.SetSeed(Params.WorldSeed + Params.ForestOffsetSeed);
    ForestNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    ForestNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    ForestNoise.SetFractalOctaves(3);   // bords de lisiere irreguliers
    ForestNoise.SetFrequency(1.0f / FMath::Max(1.0f, Params.ForestWaveLength));
}
void FTreeScatterer::SetParams(const FTreeScatterParams& InParams)
{ 
    Params = InParams;
    Reconfigure();
}

float FTreeScatterer::ComputeForestDensity(float WorldX, float WorldY) const
{
    const float ForestNoiseValue = ForestNoise.GetNoise(WorldX, WorldY);
    const float Unit = ForestNoiseValue * 0.5 + 0.5;

    const float Scaled = Unit * Params.GlobalDensityScale;
    return FMath::Clamp(FMath::Max(Scaled, Params.MinDensity), 0.0f, 1.0f);
}

void FTreeScatterer::EnumerateInBounds(const FBox& Bounds, TArray<FVector>& Out, const FTerrainGenerator& TerrainGenerator) const
{
    Out.Reset();

    if (Params.CellSize <= 0.0f)
    {
        UE_LOG(LogTemp, Error, TEXT("FTreeScatterer : CellSize <= 0"));
        return;
    }

    const float Cell = Params.CellSize;
    const float Ratio = FMath::Clamp(Params.JitterRatio, 0.0f, 1.0f);

    // JitterRatio centre la zone de pose dans la cellule.
    // Ratio=0.5, Cell=1000 -> Marge=250, jitter dans [250, 750[
    // -> deux arbres voisins sont a >= 500 unites l'un de l'autre.
    const float Marge = Cell * (1.0f - Ratio) * 0.5f;

    // Bornes monde -> plage de cellules.
    // FMath::FloorToInt sur une division FLOAT arrondit deja vers -infini :
    // pas besoin de FloorDivInt ici (qui sert aux divisions ENTIERES).
    const int32 CellMinX = FMath::FloorToInt(Bounds.Min.X / Cell);
    const int32 CellMaxX = FMath::FloorToInt(Bounds.Max.X / Cell);
    const int32 CellMinY = FMath::FloorToInt(Bounds.Min.Y / Cell);
    const int32 CellMaxY = FMath::FloorToInt(Bounds.Max.Y / Cell);

    // Garde-fou : des bornes aberrantes alloueraient des gigaoctets.
    const int64 CountX = (int64)CellMaxX - (int64)CellMinX + 1;
    const int64 CountY = (int64)CellMaxY - (int64)CellMinY + 1;
    const int64 Total = CountX * CountY;
    if (Total <= 0 || Total > 1000000)
    {
        UE_LOG(LogTemp, Error,
            TEXT("FTreeScatterer : %lld cellules demandees, bornes aberrantes"), Total);
        return;
    }

    Out.Reserve((int32)Total);

    const uint32 Seed = (uint32)Params.WorldSeed;

    // Bornes INCLUSES des deux cotes (<=, pas <) : avec '<' on perdrait
    // la derniere colonne, soit un lisere vide a l'est et au nord.
    for (int32 cy = CellMinY; cy <= CellMaxY; ++cy)
    {
        for (int32 cx = CellMinX; cx <= CellMaxX; ++cx)
        {
            // Chaine de maillons : chaque valeur tiree a le sien.
            // Reutiliser le meme maillon pour deux valeurs les correle,
            // ce qui se voit comme un motif regulier dans la foret.
            const uint32 H1 = HashCell2D(cx, cy, Seed);
            const uint32 H2 = HashMix(H1);
            const uint32 H3 = HashMix(H2);

            const float CornerX = (float)((double) cx * (double)Cell);
            const float CornerY = (float)((double)cy * (double)Cell);
            const float Density = ComputeForestDensity(CornerX, CornerY);

            if (HashToFloat01(H3) >= Density) continue;

            const float jx = Marge + HashToFloat01(H1) * Cell * Ratio;
            const float jy = Marge + HashToFloat01(H2) * Cell * Ratio;

            // double : a 50 km, un float perd la precision centimetrique.
            const double WorldX = (double)cx * (double)Cell + (double)jx;
            const double WorldY = (double)cy * (double)Cell + (double)jy;

            const double WorldZ = TerrainGenerator.ComputeHeight(WorldX / Params.VoxelSize, WorldY / Params.VoxelSize) * Params.VoxelSize;
            Out.Add(FVector(WorldX, WorldY, WorldZ));
        }
    }
}