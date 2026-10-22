#pragma once

#include "CoreMinimal.h"

/**
 * Reglages du SEMIS d'arbres. Un seul jeu pour tout le monde.
 *
 * A ne pas confondre avec FTreeSpeciesParams (un jeu PAR ESSENCE, plus tard)
 * ni avec les parametres climatiques (etape 3).
 *
 * POD pur : aucun UObject, aucun TObjectPtr. Cette struct sera copiee par
 * valeur dans des jobs worker, ou le GC peut collecter a tout moment.
 */
struct FTreeScatterParams
{
    /** Identique client ET serveur, sinon deux forets differentes. */
    int32 WorldSeed = 1337;

    int8 VoxelSize = 10;

    /**
     * Cote d'une cellule, en unites monde. Une cellule = AU PLUS un arbre.
     * CellSize = sqrt(surface moyenne par arbre).
     *   futaie mature (100 m2/arbre) -> 1000
     *   foret jeune   (20 m2/arbre)  -> 450
     *   savane        (1000 m2/arbre)-> 3200
     * Fixe la densite MAXIMALE ; le masque de densite (etape 3) n'en retire.
     */
    float CellSize = 10000.0f;

    /**
     * Fraction centrale de la cellule ou l'arbre peut se poser.
     *   0.5 -> espacement minimal garanti de 0.5 * CellSize
     *   1.0 -> plus naturel, mais deux arbres peuvent se superposer
     */
    float JitterRatio = 0.7f;

    /** Levier global d'eclaircissement. Utilise a l'etape 3. */
    float GlobalDensityScale = 1.0f;

    float ForestWaveLength = 200000.0f;
    int32 ForestOffsetSeed = 4001;
    float MinDensity = 0.0f;

    /**
     * De combien un chunk doit regarder AU-DELA de ses bornes pour
     * attraper les arbres voisins qui debordent chez lui.
     * DOIT etre le MAXIMUM sur TOUTES les essences : un sequoia ajoute
     * plus tard sans mise a jour produirait des demi-arbres aux bordures.
     * Utilise a l'etape 5.
     */
    float MaxInfluenceRadiusXY = 1000.0f;
    float MaxInfluenceHeightZ  = 2500.0f;
};