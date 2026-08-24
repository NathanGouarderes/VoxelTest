#pragma once
#include "CoreMinimal.h"

struct FTreeSpeciesParams
{
	uint8   SpeciesIndex;         // index dans le TArray, jamais un pointeur
	float   Weight;               // poids du tirage quand plusieurs espèces conviennent

		// --- Tolérances climatiques (voir la courbe ci-dessous) ---
	float   TempOptMin, TempOptMax;       // plage optimale, °C
	float   TempTolMin, TempTolMax;       // plage de survie
	float   HumidOptMin, HumidOptMax;      // mm/an
	float   HumidTolMin, HumidTolMax;

		// --- Filtres géographiques ---
	float   AltitudeMin, AltitudeMax;      // unités monde
	float   MaxSlopeCos;       // cos, pour éviter un acos par test
	uint32  AllowedSurfaceMaterials;       // masque de bits sur tes Material.Id
	float   MinDistanceToWater;        // 0 = indifférent (saule, palmier)

		// --- Instanciation ---
	float   ScaleMin, ScaleMax;
	float   SuitabilitySizeBonus;         // 0..1 : plus grand là où il est heureux

		// --- Empreinte (alimente MaxInfluenceRadius) ---
	float   CanopyRadius;
	float TrunkRadius;
	float Height;

		// --- Physique (pour le déracinement, plus tard) ---
	float   MassAtScale1; 
	float AnchorMomentAtScale1; 
	float RuptureMomentAtScale1;
};
