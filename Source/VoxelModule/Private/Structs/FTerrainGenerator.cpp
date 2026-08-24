#include "../../Public/Structs/FTerrainGenerator.h"
#include "../../Public/DebugMacro.h"

static FORCEINLINE int32 FloorDivInt(int32 A, int32 B)
{
    const int32 Q = A / B;
    return (A % B != 0 && ((A < 0) != (B < 0))) ? Q - 1 : Q;
}

void FTerrainGenerator::SetConfig(const TSharedPtr<const FTerrainConfig, ESPMode::ThreadSafe>& InConfig)
{
    if (!InConfig.IsValid())
    {
        return;
    }
    if (InConfig.Get() == CachedRaw)
    {
        return;
    }
    Config = InConfig;
    CachedRaw = InConfig.Get();
    Reconfigure();
}

void FTerrainGenerator::Reconfigure()
{
    const FTerrainConfig& C = *CachedRaw;

    // --- Champs de controle -------------------------------------------------
    ControlNoise.SetNum(C.ControlFields.Num());
    for (int32 i = 0; i < C.ControlFields.Num(); ++i)
    {
        const FControlFieldConfig& F = C.ControlFields[i];
        FastNoiseLite& N = ControlNoise[i];
        N.SetSeed(C.Global.MasterSeed + F.SeedOffset);
        N.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
        N.SetFractalType(FastNoiseLite::FractalType_FBm);
        N.SetFractalOctaves(FMath::Max(1, F.Octaves));
        N.SetFrequency(1.0f / FMath::Max(1.0f, F.Wavelength));
    }

    // --- Couches ------------------------------------------------------------
    LayersNoise.SetNum(C.Layers.Num());
    LayerControlIndex.SetNum(C.Layers.Num());
    NathanDebug(TEXT("LayersNoise Num : %d ControlNoise Num : %d"), LayersNoise.Num(), ControlNoise.Num());

    for (int32 i = 0; i < C.Layers.Num(); ++i)
    {
        const FTerrainLayer& L = C.Layers[i];

        // Resolution FName -> index, UNE fois. Pas par colonne.
        LayerControlIndex[i] = INDEX_NONE;
        if (L.ModulatedBy != NAME_None)
        {
            for (int32 j = 0; j < C.ControlFields.Num(); ++j)
            {
                if (C.ControlFields[j].Name == L.ModulatedBy)
                {
                    LayerControlIndex[i] = j;
                    break;
                }
            }
            if (LayerControlIndex[i] == INDEX_NONE)
            {
                UE_LOG(LogTemp, Error,
                    TEXT("FTerrainGenerator : couche %d reference le champ '%s' qui n'existe pas"),
                    i, *L.ModulatedBy.ToString());
            }
        }

        FastNoiseLite& N = LayersNoise[i];
        N.SetSeed(C.Global.MasterSeed + L.SeedOffset);
        N.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
        N.SetFractalType(L.Type == ETerrainLayerType::Ridged
            ? FastNoiseLite::FractalType_Ridged
            : FastNoiseLite::FractalType_FBm);
        N.SetFractalOctaves(FMath::Max(1, L.Octaves));
        N.SetFractalLacunarity(L.Lacunarity);
        N.SetFractalGain(L.Gain);
        N.SetFrequency(1.0f / FMath::Max(1.0f, L.Wavelength));
    }

    // --- Grottes ------------------------------------------------------------
    CaveNoiseInst.SetSeed(C.Global.MasterSeed);
    CaveNoiseInst.SetNoiseType(FastNoiseLite::NoiseType_Perlin);

    // --- Warp : deux instances, graines DIFFERENTES.
    //     Meme graine => dx == dy => translation diagonale, pas de deformation.
    const float WarpFreq = 1.0f / FMath::Max(1.0f, C.Global.WarpWavelength);
    WarpNoiseX.SetSeed(C.Global.MasterSeed + 7717);
    WarpNoiseX.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    WarpNoiseX.SetFrequency(WarpFreq);
    WarpNoiseY.SetSeed(C.Global.MasterSeed + 3391);
    WarpNoiseY.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    WarpNoiseY.SetFrequency(WarpFreq);
}

float FTerrainGenerator::ComputeHeight(float WorldX, float WorldY) const
{
    if (!CachedRaw)
    {
        return 0.0f;
    }
    const FTerrainConfig& C = *CachedRaw;

    // 1) Warp : deforme l'espace AVANT tout echantillonnage.
    float SX = WorldX;
    float SY = WorldY;
    if (C.Global.WarpStrength > 0.0f)
    {
        SX = WorldX + WarpNoiseX.GetNoise(WorldX, WorldY) * C.Global.WarpStrength;
        SY = WorldY + WarpNoiseY.GetNoise(WorldX, WorldY) * C.Global.WarpStrength;
    }

    // 2) Champs de controle. TInlineAllocator : zero allocation heap par colonne.
    TArray<float, TInlineAllocator<8>> Ctrl;
    Ctrl.SetNum(ControlNoise.Num());
    for (int32 i = 0; i < ControlNoise.Num(); ++i)
    {
        float V = ControlNoise[i].GetNoise(SX, SY);
        if (C.ControlFields[i].bRemapToUnit)
        {
            V = V * 0.5f + 0.5f;
        }
        Ctrl[i] = V;
    }

    // 3) Couches.
    float Sum = 0.0f;
    const int32 NumLayers = FMath::Min(C.Layers.Num(), LayersNoise.Num());
    for (int32 i = 0; i < NumLayers; ++i)
    {
        const FTerrainLayer& L = C.Layers[i];
        float N = LayersNoise[i].GetNoise(SX, SY);

        if (L.Type == ETerrainLayerType::Billow)
        {
            N = FMath::Abs(N) * 2.0f - 1.0f;
        }

        float A = L.Amplitude * L.Weight;
        const int32 Ci = LayerControlIndex[i];
        if (Ci != INDEX_NONE && Ctrl.IsValidIndex(Ci))
        {
            A *= Ctrl[Ci];
        }
        Sum += N * A;
    }

    // 4) Spline : si presente, elle produit la hauteur ABSOLUE.
    //    BaseHeight n'est alors PAS ajoute.
    if (C.HeightSpline.IsValid())
    {
        return C.HeightSpline.Eval(Sum);
    }
    return static_cast<float>(C.Global.BaseHeight) + Sum;
}

bool FTerrainGenerator::IsSolidAtHeight(float Height, int32 gx, int32 gy, int32 gz) const
{
    if (!CachedRaw)
    {
        return false;
    }
    if (static_cast<float>(gz) >= Height)
    {
        return false;
    }
    const FTerrainConfig& C = *CachedRaw;
    const float F = C.Global.CaveFrequency;
    const float Cave = CaveNoiseInst.GetNoise(gx * F, gy * F, gz * F);
    return Cave <= C.Global.CaveThreshold;
}

bool FTerrainGenerator::IsSolid(int32 gx, int32 gy, int32 gz) const
{
    const float H = ComputeHeight(static_cast<float>(gx), static_cast<float>(gy));
    return IsSolidAtHeight(H, gx, gy, gz);
}

float FTerrainGenerator::GetMaxPossibleHeight() const
{
    if (!CachedRaw)
    {
        return 0.0f;
    }
    const FTerrainConfig& C = *CachedRaw;

    if (C.HeightSpline.IsValid())
    {
        float MaxOut = C.HeightSpline.Points[0].Out;
        for (const FTerrainSplinePoint& P : C.HeightSpline.Points)
        {
            MaxOut = FMath::Max(MaxOut, P.Out);
        }
        return MaxOut;
    }

    float Total = 0.0f;
    for (const FTerrainLayer& L : C.Layers)
    {
        Total += FMath::Abs(L.Amplitude * L.Weight);
    }
    return static_cast<float>(C.Global.BaseHeight) + Total;
}

uint8 FTerrainGenerator::MaterialIdAt(const float GeologicalProvinceValue, float Height, int32 gx, int32 gy, int32 gz) const
{
    if (!IsSolidAtHeight(Height, gx, gy, gz))
    {
        return 0;

    }
    else
    {
        const int32 Depth = FMath::FloorToInt(Height) - gz;
        if (!CachedRaw) return 0;
        if (Depth == 0 && gz > CachedRaw->Global.SeaLevel)
        {
            return 1;
        }
        if (Depth <= 0 && gz <= CachedRaw->Global.SeaLevel)
        {
            return 2;
        }
        if (Depth < 4)
        {
            return 3;
        }
        if (Depth < 20)
        {
            return 4;
        }
        if (Depth >= CachedRaw->Global.VeinMinDepth && Depth > CachedRaw->Global.VeinMaxDepth)
        {
            int32 cx = FloorDivInt(gx, 16);
            int32 cy = FloorDivInt(gy, 16);
            int32 cz = FloorDivInt(gz, 16);
        }
        else
        {
            return 5;
        }
    }
}