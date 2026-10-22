#include "../../Public/Structs/FTerrainGenerator.h"
#include "../../Public/DebugMacro.h"

// Liste des Id des minerais : 
 
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
    //DebugLogStats(500000, 1000);
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
        N.SetFractalType(F.Type == ETerrainLayerType::Ridged ? FastNoiseLite::FractalType_Ridged : FastNoiseLite::FractalType_FBm);
        N.SetFractalOctaves(FMath::Max(1, F.Octaves));
        N.SetFrequency(1.0f / FMath::Max(1.0f, F.Wavelength));
    }
    ProvinceNoise.SetSeed(C.Global.MasterSeed + C.Global.ProvinceSeedOffset);
    ProvinceNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    ProvinceNoise.SetFrequency(1.0f / FMath::Max(1.0f, C.Global.ProvinceWavelength));

    VeinNoise.SetSeed(C.Global.MasterSeed + C.Global.VeinSeedOffset);
    VeinNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    VeinNoise.SetFrequency(1.0f / FMath::Max(1.0f, C.Global.VeinWavelength));
    // --- Spline de base
    if (C.HeightSpline.IsValid() && !C.ControlFields.IsValidIndex(C.ContinentalFieldIndex))
    {
        UE_LOG(LogTemp, Error,
            TEXT("FTerrainGenerator : HeightSpline valide mais ContinentalFieldIndex %d invalide (%d champs) -> BaseHeight utilise"),
            C.ContinentalFieldIndex, C.ControlFields.Num());
    }

    // --- Couches ------------------------------------------------------------
    LayersNoise.SetNum(C.Layers.Num());
    LayerControlIndex.SetNum(C.Layers.Num());
    MaskLayersNoise.SetNum(C.Layers.Num());
    LayerGates.SetNum(C.Layers.Num());
    //NathanDebug(TEXT("LayersNoise Num : %d ControlNoise Num : %d"), LayersNoise.Num(), ControlNoise.Num());

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

        LayerGates[i].Reset();
        for (const FTerrainFieldGate& Gate : L.Gates)
        {
            FResolvedGate ResolvedGate;
            ResolvedGate.FieldIndex = INDEX_NONE;
            for (int32 j = 0; j < C.ControlFields.Num(); ++j)
            {
                if (C.ControlFields[j].Name == Gate.Field)
                {
                    ResolvedGate.FieldIndex = j;
                    break;
                }
            }
            ResolvedGate.Low = Gate.Low;
            ResolvedGate.High = Gate.High;
            ResolvedGate.bInvert = Gate.bInvert;

            if (ResolvedGate.FieldIndex == INDEX_NONE)
            {
                UE_LOG(LogTemp, Error,
                    TEXT("FTerrainGenerator : porte de la couche %d sur le champ '%s' introuvable -> couche desactivee"),
                    i, *Gate.Field.ToString());
            }
            if (Gate.Low >= Gate.High)
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("FTerrainGenerator : porte de la couche %d : Low (%.3f) >= High (%.3f), transition nulle"),
                    i, Gate.Low, Gate.High);
            }
            LayerGates[i].Add(ResolvedGate);
        }

        FastNoiseLite& Mask = MaskLayersNoise[i];
        Mask.SetSeed(C.Global.MasterSeed + L.MaskSeedOffset);
        Mask.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
        Mask.SetFractalType(FastNoiseLite::FractalType_FBm);
        Mask.SetFractalOctaves(2);
        Mask.SetFrequency(1.0f / FMath::Max(1.0f, L.MaskWavelength));

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
    CaveNoiseInst.SetFrequency(C.Global.CaveFrequency);

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
        Ctrl[i] = ControlNoise[i].GetNoise(SX, SY);
    }

    // 3) Couches.
    float Sum = 0.0f;
    const int32 NumLayers = FMath::Min(C.Layers.Num(), LayersNoise.Num());
    for (int32 i = 0; i < NumLayers; ++i)
    {
        const FTerrainLayer& TerrainLayer = C.Layers[i];
        float Mask = ComputeLayerPresence(i, SX, SY, Ctrl);
        if (Mask <= 0.0f)
        {
            continue;
        }

        float LayerNoiseValue = LayersNoise[i].GetNoise(SX, SY);

        if (TerrainLayer.Type == ETerrainLayerType::Billow)
        {
            LayerNoiseValue = FMath::Abs(LayerNoiseValue) * 2.0f - 1.0f;
        }
        else if (TerrainLayer.Type == ETerrainLayerType::Ridged)
        {
            const int32 K = FMath::Clamp(FMath::RoundToInt(TerrainLayer.RidgeStrength), 0, 6);
            if (K > 0)
            {
                const float R0 = FMath::Max(0.0f, LayerNoiseValue);
                float R = 1.0f;
                for (int32 k = 0; k < K; ++k)
                {
                    R *= R0;
                }
                LayerNoiseValue = R;
            }
        }

        float A = TerrainLayer.Amplitude * TerrainLayer.Weight * Mask;
        const int32 Ci = LayerControlIndex[i];
        if (Ci != INDEX_NONE && Ctrl.IsValidIndex(Ci))
        {
            const float V = Ctrl[Ci];
            A *= C.ControlFields[Ci].bRemapToUnit ? V * 0.5f + 0.5f : V;
        }
        Sum += LayerNoiseValue * A;
    }
    // 4) Altitude de base : spline de la valeur BRUTE du champ continental,
    //    ou BaseHeight. Les couches (Sum) s'ajoutent par-dessus.
    const float Base = GetBaseSpline()
        ? C.HeightSpline.Eval(Ctrl[C.ContinentalFieldIndex])
        : static_cast<float>(C.Global.BaseHeight);

    //NathanDebug(TEXT("Sum : %f"), Sum);
    return Base + Sum; //static_cast<float>(C.Global.BaseHeight) + Sum;
}

bool FTerrainGenerator::GetBaseSpline() const
{
    const FTerrainConfig& C = *CachedRaw;
    return C.HeightSpline.IsValid() && C.ControlFields.IsValidIndex(C.ContinentalFieldIndex);
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
    const float CaveNoiseValue = CaveNoiseAt(gx, gy, gz);
    return CaveNoiseValue <= CachedRaw->Global.CaveThreshold;
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

    // 1) Base maximale : même choix que ComputeHeight (spline si valide, sinon BaseHeight).
    float Max = static_cast<float>(C.Global.BaseHeight);
    if (GetBaseSpline())
    {
        Max = C.HeightSpline.Points[0].Out;
        for (const FTerrainSplinePoint& P : C.HeightSpline.Points)
        {
            Max = FMath::Max(Max, P.Out);
        }
    }

    // 2) Chaque couche ajoute au plus |Amplitude * Weight| :
    //    bruit dans [-1, 1], masque et champ de contrôle remappé dans [0, 1].
    for (const FTerrainLayer& L : C.Layers)
    {
        Max += FMath::Abs(L.Amplitude * L.Weight);
    }
    return Max;
}
float FTerrainGenerator::ComputeProvince(float WorldX, float WorldY) const
{
    if (!CachedRaw) return 0.0f;
    return ProvinceNoise.GetNoise(WorldX, WorldY);
}

uint8 FTerrainGenerator::OreIdAt(uint32 h, int32 Depth, int32 gx, int32 gy, int32 gz) const
{
    // Bits de POIDS FORT : le seuil de cellule a consomme les bits faibles (h % 1000).
    // Reutiliser les memes bits correlerait l'espece au fait que la cellule soit retenue.
    const uint32 Species = (h >> 16) % 100u;

    uint8 Id;
    int32 MinDepth, MaxDepth;

    if (Species < 45u) { Id = 10; MinDepth = 20; MaxDepth = 300; }  // charbon
    else if (Species < 75u) { Id = 11; MinDepth = 50; MaxDepth = 600; }  // fer
    else if (Species < 93u) { Id = 12; MinDepth = 200; MaxDepth = 900; }  // cuivre
    else { Id = 13; MinDepth = 400; MaxDepth = 1500; }  // or

    // Hors de SA bande : la cellule ne produit rien. Ce rejet est frequent et
    // VOULU — c'est ce qui rend l'or rare en plus de son tirage a 7%.
    if (Depth < MinDepth || Depth > MaxDepth) return 0;

    // Seul appel de bruit 3D de toute la chaine minerai.
    if (VeinNoise.GetNoise((float)gx, (float)gy, (float)gz) < CachedRaw->Global.VeinThreshold)
        return 0;

    return Id;
}

// Diagnostic : percentiles des champs de contrôle et de la hauteur finale sur une grande zone.
// Exemple : DebugLogStats(500000, 1000) => 100 km x 100 km au pas de 100 m.
void FTerrainGenerator::DebugLogStats(int32 HalfExtentVoxels, int32 StepVoxels) const
{
    if (!CachedRaw || StepVoxels <= 0 || HalfExtentVoxels <= 0)
    {
        return;
    }
    const FTerrainConfig& C = *CachedRaw;

    const int32 N = 2 * HalfExtentVoxels / StepVoxels + 1;

    TArray<TArray<float>> CtrlSamples;
    CtrlSamples.SetNum(ControlNoise.Num());
    for (TArray<float>& Samples : CtrlSamples)
    {
        Samples.Reserve(N * N);
    }
    TArray<float> Heights;
    Heights.Reserve(N * N);

    const int32 NumLayers = FMath::Min(C.Layers.Num(), LayersNoise.Num());
    TArray<int32> LayerPresent;
    TArray<int32> LayerFull;
    LayerPresent.SetNumZeroed(NumLayers);
    LayerFull.SetNumZeroed(NumLayers);

    for (int32 iy = 0; iy < N; ++iy)
    {
        for (int32 ix = 0; ix < N; ++ix)
        {
            const float X = (float)(-HalfExtentVoxels + ix * StepVoxels);
            const float Y = (float)(-HalfExtentVoxels + iy * StepVoxels);

            // Même warp que ComputeHeight : on mesure ce qui est réellement échantillonné.
            float SX = X;
            float SY = Y;
            if (C.Global.WarpStrength > 0.0f)
            {
                SX = X + WarpNoiseX.GetNoise(X, Y) * C.Global.WarpStrength;
                SY = Y + WarpNoiseY.GetNoise(X, Y) * C.Global.WarpStrength;
            }
            TArray<float, TInlineAllocator<8>> Ctrl;
            Ctrl.SetNum(ControlNoise.Num());
            for (int32 i = 0; i < ControlNoise.Num(); ++i)
            {
                Ctrl[i] = ControlNoise[i].GetNoise(SX, SY);   // BRUT, comme ComputeHeight
                CtrlSamples[i].Add(Ctrl[i]);
            }
            for (int32 l = 0; l < NumLayers; ++l)
            {
                const float P = ComputeLayerPresence(l, SX, SY, Ctrl);
                if (P > 0.0f) { ++LayerPresent[l]; }
                if (P >= 1.0f) { ++LayerFull[l]; }
            }
            Heights.Add(ComputeHeight(X, Y));
        }
    }

    auto LogPercentiles = [](const FString& Label, TArray<float>& Samples)
        {
            if (Samples.Num() == 0)
            {
                return;
            }
            Samples.Sort();
            const int32 Last = Samples.Num() - 1;
            auto P = [&Samples, Last](float Q)
                {
                    return Samples[FMath::Clamp((int32)(Q * Last), 0, Last)];
                };
            UE_LOG(LogTemp, Warning,
                TEXT("%s : min=%.3f P5=%.3f P25=%.3f P50=%.3f P75=%.3f P95=%.3f max=%.3f"),
                *Label, P(0.0f), P(0.05f), P(0.25f), P(0.5f), P(0.75f), P(0.95f), P(1.0f));
        };

    const float Total = (float)(N * N);
    for (int32 l = 0; l < NumLayers; ++l)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Couche %d (amplitude %.0f vx) : presente sur %.1f%% du monde, pleine sur %.1f%%"),
            l, C.Layers[l].Amplitude, 100.0f * LayerPresent[l] / Total, 100.0f * LayerFull[l] / Total);
    }

    for (int32 i = 0; i < CtrlSamples.Num(); ++i)
    {
        LogPercentiles(FString::Printf(TEXT("Ctrl brut[%s]"), *C.ControlFields[i].Name.ToString()), CtrlSamples[i]);
    }
    LogPercentiles(TEXT("Hauteur (voxels)"), Heights);
}

uint8 FTerrainGenerator::MaterialIdAt(float Province, float Height, int32 gx, int32 gy, int32 gz, int32 LOD) const
{
    if (!CachedRaw) return 0;

    // 1) Existence. Court-circuite tout sur ~60% des voxels d'un chunk de surface.
    if (!IsSolidAtHeight(Height, gx, gy, gz)) return 0;

    const FWorldGlobalSettings& WorldGlobalSettings = CachedRaw->Global;
    const int32 Depth = FMath::FloorToInt(Height) - gz;

    int32 Step = 1 << LOD;

    // 2) Couches superficielles. AVANT les filons : pas de minerai dans l'herbe.
    if (Depth < Step) return (gz <= WorldGlobalSettings.SeaLevel) ? 2 : 1;   // sable / herbe
    if (Depth < 4 * Step)  return 3;                            // terre
    if (Depth < 20 * Step) return 4;                            // roche alteree

    // 3) Filons. Le test de bande evite le hachage sur toute la roche hors zone minera1isee.
    if (Depth >= WorldGlobalSettings.VeinMinDepth && Depth < WorldGlobalSettings.VeinMaxDepth)
    {
        const int32 CS = WorldGlobalSettings.VeinCellSize;
        // FloorDivInt obligatoire : -5/16 vaut 0 en C++, pas -1.
        const uint32 h = HashCell3D(FloorDivInt(gx, CS),
            FloorDivInt(gy, CS),
            FloorDivInt(gz, CS),
            (uint32)WorldGlobalSettings.MasterSeed);

        if ((h % 1000u) < 20u)                       // ~2% des cellules
        {
            const uint8 Ore = OreIdAt(h, Depth, gx, gy, gz);
            if (Ore != 0) return Ore;                // sinon on retombe sur l'encaissante
        }
    }

    // 4) Roche encaissante. La province ne decide QUE de la roche profonde.
    if (Province < -0.35f) return 5;   // Andesite
    if (Province < -0.10f) return 6;   // gneiss
    if (Province < 0.25f) return 7;   // granite
    if (Province < 0.45f) return 8;   // andesite
    return 9;                          // calcaire
}

float FTerrainGenerator::CaveNoiseAt(float gx, float gy, float gz) const
{
    if (!CachedRaw)
    {
        return 0.0f;
    }
    const float CaveFrequency = CachedRaw->Global.CaveFrequency;
    return CaveNoiseInst.GetNoise(gx * CaveFrequency, gy * CaveFrequency, gz * CaveFrequency);
}

float FTerrainGenerator::GetCaveThreshold() const
{
    return CachedRaw ? CachedRaw->Global.CaveThreshold : 0.0f;
}

float FTerrainGenerator::ComputeLayerPresence(int32 LayerIndex, float SX, float SY,
    const TArray<float, TInlineAllocator<8>>& Ctrl) const
{
    const FTerrainLayer& L = CachedRaw->Layers[LayerIndex];

    // 1) Masque propre a la couche (mecanisme existant, inchange).
    float Presence = 1.0f;
    if (L.MaskWavelength > 0.0f && MaskLayersNoise.IsValidIndex(LayerIndex))
    {
        const float Ctl = MaskLayersNoise[LayerIndex].GetNoise(SX, SY);
        Presence = FMath::SmoothStep(L.MaskLow, L.MaskHigh, Ctl);
        if (Presence <= 0.0f)
        {
            return 0.0f;
        }
    }

    // 2) Portes sur les champs partages : produit des presences.
    for (const FResolvedGate& Gate : LayerGates[LayerIndex])
    {
        if (!Ctrl.IsValidIndex(Gate.FieldIndex))
        {
            return 0.0f;   // champ introuvable : porte FERMEE (erreur deja loggee dans Reconfigure)
        }
        float S = FMath::SmoothStep(Gate.Low, Gate.High, Ctrl[Gate.FieldIndex]);
        if (Gate.bInvert)
        {
            S = 1.0f - S;
        }
        Presence *= S;
        if (Presence <= 0.0f)
        {
            return 0.0f;
        }
    }
    return Presence;
}