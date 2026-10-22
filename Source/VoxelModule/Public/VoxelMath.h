#pragma once

#include "CoreMinimal.h"

/**
 * Utilitaires mathematiques partages du module voxel.
 * Tout est FORCEINLINE et defini ici : aucun appel de fonction reel,
 * ces routines tournent dans des boucles de plusieurs millions d'iterations.
 */

// ---------------------------------------------------------------------
//  Division entiere
// ---------------------------------------------------------------------

/**
 * Division entiere arrondie vers -infini (et non vers zero comme '/').
 *   FloorDivInt(-5, 128) == -1     (alors que -5 / 128 == 0)
 *   FloorDivInt(130, 128) == 1
 *
 * INDISPENSABLE pour passer d'une coordonnee voxel a une coordonnee de
 * chunk : avec '/', tous les chunks negatifs seraient decales d'un cran.
 *
 * NOTE : inutile quand on divise un FLOAT (position monde) par une taille
 * de cellule -- FMath::FloorToInt arrondit deja vers -infini.
 */
FORCEINLINE int32 FloorDivInt(int32 A, int32 B)
{
    const int32 Q = A / B;
    return (A % B != 0 && ((A < 0) != (B < 0))) ? Q - 1 : Q;
}

// ---------------------------------------------------------------------
//  Hachage deterministe
// ---------------------------------------------------------------------
//
//  Un HASH n'est pas un BRUIT :
//    - bruit (FastNoiseLite) : f(x) et f(x+e) sont PROCHES  -> collines
//    - hash                  : H(3) et H(4) sont SANS RAPPORT -> decisions
//
//  Propriete recherchee : l'AVALANCHE. Changer 1 bit d'entree doit changer
//  ~50 % des bits de sortie. Sans elle, on voit des motifs alignes sur la
//  grille dans la foret.
//
//  Reference : Jarzynski & Olano, "Hash Functions for GPU Rendering",
//  JCGT vol.9 no.3 (2020) -- https://jcgt.org/published/0009/03/02/
// ---------------------------------------------------------------------

/**
 * Finaliseur fmix32 de MurmurHash3. DEUX tours de multiplication :
 * un seul laisse des bits d'entree sans influence sur certains bits
 * de sortie, ce qui produit des regularites visibles.
 */
FORCEINLINE uint32 HashMix(uint32 h)
{
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

/** Coordonnee de cellule 2D -> entier chaotique deterministe. */
FORCEINLINE uint32 HashCell2D(int32 cx, int32 cy, uint32 Seed)
{
    uint32 h = Seed;
    h ^= (uint32)cx * 0x9E3779B1u;   // nombre d'or 32 bits
    h ^= (uint32)cy * 0x85EBCA77u;
    return HashMix(h);
}

/** Coordonnee de cellule 3D -> entier chaotique deterministe. */
FORCEINLINE uint32 HashCell3D(int32 cx, int32 cy, int32 cz, uint32 Seed)
{
    uint32 h = Seed;
    h ^= (uint32)cx * 0x9E3779B1u;
    h ^= (uint32)cy * 0x85EBCA77u;
    h ^= (uint32)cz * 0xC2B2AE3Du;
    return HashMix(h);
}

/**
 * uint32 -> [0, 1[.
 *
 * Masque a 24 bits car la mantisse d'un float en fait 24 : diviser un
 * entier 32 bits par 2^32 produit des collisions non uniformes, donc un
 * biais invisible. Les 24 bits de poids faible ont deja recu toute
 * l'avalanche de HashMix, rien ne se perd.
 *
 * JAMAIS de modulo (H % N) : 2^32 n'est pas un multiple de N, les
 * premieres valeurs sortiraient plus souvent que les autres.
 */
FORCEINLINE float HashToFloat01(uint32 H)
{
    return (H & 0x00FFFFFFu) * (1.0f / 16777216.0f);
}

/** uint32 -> [Min, Max[. Pour les echelles, les angles, etc. */
FORCEINLINE float HashToRange(uint32 H, float Min, float Max)
{
    return Min + HashToFloat01(H) * (Max - Min);
}