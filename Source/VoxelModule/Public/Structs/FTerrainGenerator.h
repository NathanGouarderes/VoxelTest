#pragma once

#include "CoreMinimal.h"
#include "FastNoiseLite.h"
#include "../../Public/VoxelMath.h"
#include "../../Public/Structs/FHeightMaxPyramid.h"
#include "FTerrainConfig.h"

/**
 * Generateur de terrain : objet pur, sans thread, sans UObject.
 * Possede une config immuable et SES propres instances de bruit.
 * Chaque proprietaire (worker, manager) en a une instance privee :
 * aucun partage, donc aucun verrou.
 */

 /** Porte resolue une fois pour toutes dans Reconfigure : FName -> index. */
struct FResolvedGate
{
    int32 FieldIndex = INDEX_NONE;
    float Low = 0.0f;
    float High = 0.0f;
    bool bInvert = false;
};

class VOXELMODULE_API FTerrainGenerator
{
public:
    FTerrainGenerator() = default;

    /** Memorise la config et reconfigure les bruits SI l'adresse a change.
     *  Appel gratuit en regime permanent. */
    void SetConfig(const TSharedPtr<const FTerrainConfig, ESPMode::ThreadSafe>& InConfig);

    bool IsReady() const { return Config.IsValid() && LayersNoise.Num() > 0; }

    const FTerrainConfig* GetConfig() const { return Config.Get(); }

    /** Altitude du sol en voxels-monde. Une evaluation par couche. */
    float ComputeHeight(float WorldX, float WorldY) const;

    float ComputeProvince(float WorldX, float WorldY) const;

    uint8 OreIdAt(uint32 h, int32 Depth, int32 gx, int32 gy, int32 gz) const;

    float ComputeLayerPresence(int32 LayerIndex, float SX, float SY, const TArray<float, TInlineAllocator<8>>& Ctrl) const;
    void DebugLogStats(int32 HalfExtentVoxels, int32 StepVoxels) const;

    /** Test plein/vide quand la hauteur est DEJA connue.
     *  Le worker passe par ici : il calcule la hauteur une fois par colonne. */
    bool IsSolidAtHeight(float Height, int32 gx, int32 gy, int32 gz) const;

    /** Test plein/vide pour un voxel isole. Calcule la hauteur lui-meme.
     *  Ne JAMAIS appeler dans une boucle Z : utiliser IsSolidAtHeight. */
    bool IsSolid(int32 gx, int32 gy, int32 gz) const;

    /** Borne haute de toute surface possible. Sert au culling vertical. */
    float GetMaxPossibleHeight() const;

    uint8 MaterialIdAt(const float Province, float Height, int32 gx, int32 gy, int32 gz, int32 LOD) const;
    float CaveNoiseAt(float gx, float gy, float gz) const;
    float GetCaveThreshold() const;

    bool GetBaseSpline() const;

private:
    void Reconfigure();


    TSharedPtr<const FTerrainConfig, ESPMode::ThreadSafe> Config;
    const FTerrainConfig* CachedRaw = nullptr;

    TArray<FastNoiseLite> LayersNoise;
    TArray<FastNoiseLite> ControlNoise;
    TArray<FastNoiseLite> MaskLayersNoise;
    TArray<int32>         LayerControlIndex;
    TArray<TArray<FResolvedGate, TInlineAllocator<2>>> LayerGates;
    FastNoiseLite         CaveNoiseInst;
    FastNoiseLite         WarpNoiseX;
    FastNoiseLite         WarpNoiseY;
    FastNoiseLite         ProvinceNoise;  
    FastNoiseLite         VeinNoise;
};