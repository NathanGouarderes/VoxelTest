#pragma once

#include "CoreMinimal.h"
#include "FastNoiseLite.h"
#include "FTerrainConfig.h"

/**
 * Generateur de terrain : objet pur, sans thread, sans UObject.
 * Possede une config immuable et SES propres instances de bruit.
 * Chaque proprietaire (worker, manager) en a une instance privee :
 * aucun partage, donc aucun verrou.
 */
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

    /** Test plein/vide quand la hauteur est DEJA connue.
     *  Le worker passe par ici : il calcule la hauteur une fois par colonne. */
    bool IsSolidAtHeight(float Height, int32 gx, int32 gy, int32 gz) const;

    /** Test plein/vide pour un voxel isole. Calcule la hauteur lui-meme.
     *  Ne JAMAIS appeler dans une boucle Z : utiliser IsSolidAtHeight. */
    bool IsSolid(int32 gx, int32 gy, int32 gz) const;

    /** Borne haute de toute surface possible. Sert au culling vertical. */
    float GetMaxPossibleHeight() const;

    uint8 MaterialIdAt(const float Province, float Height, int32 gx, int32 gy, int32 gz) const;

private:
    void Reconfigure();

    TSharedPtr<const FTerrainConfig, ESPMode::ThreadSafe> Config;
    const FTerrainConfig* CachedRaw = nullptr;

    TArray<FastNoiseLite> LayersNoise;
    TArray<FastNoiseLite> ControlNoise;
    TArray<int32>         LayerControlIndex;
    FastNoiseLite         CaveNoiseInst;
    FastNoiseLite         WarpNoiseX;
    FastNoiseLite         WarpNoiseY;
    FastNoiseLite         ProvinceNoise;     
    FastNoiseLite         VeinNoise;
};