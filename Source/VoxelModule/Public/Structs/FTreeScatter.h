#pragma once

#include "CoreMinimal.h"
#include "FastNoiseLite.h"
#include "../../Public/Structs/FTerrainGenerator.h"
#include "Structs/FTreeScatterParams.h"

/**
 * Semeur d'arbres : objet pur, sans thread, sans UObject.
 * Meme contrat que FTerrainGenerator : const, thread-safe, appelable
 * depuis un worker. Possede sa config, ne prend que des coordonnees.
 *
 * NE GENERE AUCUNE GEOMETRIE. Il repond a une seule question :
 * "combien d'arbres, et a quelles coordonnees XY ?".
 * La FORME de l'arbre, c'est le UStaticMesh importe de Blender.
 */
class VOXELMODULE_API FTreeScatterer
{
public:
    FTreeScatterer() = default;

    void SetParams(const FTreeScatterParams& InParams);
    const FTreeScatterParams& GetParams() const { return Params; }

    float ComputeForestDensity(float WorldX, float WorldY) const;

    /**
     * Enumere les positions candidates dont la cellule intersecte Bounds.
     *
     * ETAPE 2 : aucun filtre. Toutes les cellules produisent un candidat,
     * Z est laisse a 0 (la requete de sol arrive a l'etape 4).
     *
     * Out est vide au debut : l'appelant n'a rien a preparer.
     */
    void EnumerateInBounds(const FBox& Bounds, TArray<FVector>& Out, const FTerrainGenerator& TerrainGenerator) const;

private:
    void Reconfigure();
    FTreeScatterParams Params;
    FastNoiseLite ForestNoise;
};