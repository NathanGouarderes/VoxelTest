#pragma once

#include "CoreMinimal.h"
#include "FTerrainConfig.generated.h"

UENUM()
enum class ETerrainLayerType : uint8
{
    Fractal   UMETA(DisplayName = "Fractal (FBM)"),
    Ridged    UMETA(DisplayName = "Ridged"),
    Billow    UMETA(DisplayName = "Billow")
};

/** Une couche de bruit. Chaque couche a SES propres parametres :
 *  c'est ce qui permet de superposer un fractal large et un ridged fin. */
USTRUCT()
struct FTerrainLayer
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    ETerrainLayerType Type = ETerrainLayerType::Fractal;

    /** En voxels. Grande valeur = formes larges. */
    UPROPERTY(EditAnywhere, meta = (ClampMin = "1.0"))
    float Wavelength = 2500.0f;

    /** En voxels. Contribution verticale de cette couche. */
    UPROPERTY(EditAnywhere)
    float Amplitude = 600.0f;

    UPROPERTY(EditAnywhere, meta = (ClampMin = "1", ClampMax = "8"))
    int32 Octaves = 5;

    /** Facteur de frequence entre octaves. 2.0 = standard. */
    UPROPERTY(EditAnywhere, meta = (ClampMin = "1.0"))
    float Lacunarity = 2.0f;

    /** Facteur d'amplitude entre octaves. 0.5 = standard. */
    UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Gain = 0.5f;

    /** DOIT differer entre couches, sinon elles se correlent. */
    UPROPERTY(EditAnywhere)
    int32 SeedOffset = 0;

    /** Multiplicateur final. 0 = couche desactivee sans la supprimer. */
    UPROPERTY(EditAnywhere)
    float Weight = 1.0f;

    // Masque régional. Wavelength = 0 -> couche globale, présente partout.
    UPROPERTY(EditAnywhere)
    float MaskWavelength = 0.0f;
    UPROPERTY(EditAnywhere)

    float MaskLow = 0.30f; //En dessous la couche est absente
    UPROPERTY(EditAnywhere)

    float MaskHigh = 0.55f; //Au dessus amplitude pleine
    UPROPERTY(EditAnywhere)
    int32 MaskSeedOffset = 0;
    UPROPERTY(EditAnywhere)
    float RidgeStrength = 2.0f;  /** Ridged uniquement. Arrondi a l'entier : 0 = bruit ridged brut (ancien comportement), *  1 = cretes positives seulement, 2 = cretes aigues, 3+ = tres acerees. */
    UPROPERTY(EditAnywhere)
    float TerraceStep = 0.0f; // 0 = pas de terrassement, sinon hauteur de marche (voxels)


    /** Si valide, l'amplitude est multipliee par ce champ de controle.
     *  Ex : "Erosion" -> ridged fort en montagne, nul en plaine. */
    UPROPERTY(EditAnywhere)
    FName ModulatedBy = NAME_None;

    TArray<FTerrainFieldGate> Gates;
};

/** Porte : la couche n'existe que la ou un champ de controle est dans la bonne plage.
 *  Presence = SmoothStep(Low, High, valeur BRUTE du champ), inversee si bInvert. */
USTRUCT()
struct FTerrainFieldGate
{
    GENERATED_BODY()

    /** Nom d'un FControlFieldConfig. Introuvable => porte FERMEE (couche absente) + erreur dans le log. */
    UPROPERTY(EditAnywhere)
    FName Field = NAME_None;

    /** Seuils sur la valeur BRUTE du champ. Mesure sa plage reelle avec DebugLogStats. */
    UPROPERTY(EditAnywhere)
    float Low = 0.0f;

    UPROPERTY(EditAnywhere)
    float High = 0.2f;

    /** Faux : presente quand le champ est HAUT. Vrai : presente quand il est BAS. */
    UPROPERTY(EditAnywhere)
    bool bInvert = false;
};

/** Description d'un champ de controle : un bruit tres basse frequence.
 *  Ne produit pas de relief, decrit le CARACTERE d'un endroit. */
USTRUCT()
struct FControlFieldConfig
{
    GENERATED_BODY()

    /** Cle de reference, utilisee par FTerrainLayer::ModulatedBy. */
    UPROPERTY(EditAnywhere)
    FName Name = NAME_None;

    UPROPERTY(EditAnywhere)
    ETerrainLayerType Type = ETerrainLayerType::Fractal;

    UPROPERTY(EditAnywhere, meta = (ClampMin = "1.0"))
    float Wavelength = 20000.0f;

    UPROPERTY(EditAnywhere, meta = (ClampMin = "1", ClampMax = "6"))
    int32 Octaves = 3;

    UPROPERTY(EditAnywhere)
    int32 SeedOffset = 100;

    /** Remappe [-1,1] vers [0,1] UNIQUEMENT quand le champ sert de multiplicateur (ModulatedBy).
    *  La spline lit toujours la valeur brute. */
    UPROPERTY(EditAnywhere)
    bool bRemapToUnit = true;  

};

USTRUCT()
struct FTerrainSplinePoint
{
    GENERATED_BODY()

    /** Valeur de bruit en entree, typiquement dans [-1, 1]. */
    UPROPERTY(EditAnywhere)
    float In = 0.0f;

    /** Hauteur en voxels-monde. */
    UPROPERTY(EditAnywhere)
    float Out = 0.0f;
};

/** Courbe de transfert. C'est elle qui produit plateaux, falaises et plaines. */
USTRUCT()
struct FTerrainSpline
{
    GENERATED_BODY()

    /** DOIT etre trie par In croissant. */
    UPROPERTY(EditAnywhere)
    TArray<FTerrainSplinePoint> Points;

    float Eval(float X) const
    {
        const int32 N = Points.Num();
        if (N == 0) return 0.0f;
        if (N == 1 || X <= Points[0].In) return Points[0].Out;
        if (X >= Points[N - 1].In)       return Points[N - 1].Out;

        for (int32 i = 1; i < N; ++i)
        {
            if (X <= Points[i].In)
            {
                const float A = Points[i - 1].In;
                const float B = Points[i].In;
                const float D = B - A;
                if (D <= KINDA_SMALL_NUMBER) return Points[i].Out;
                const float T = (X - A) / D;
                return FMath::Lerp(Points[i - 1].Out, Points[i].Out, T);
            }
        }
        return Points[N - 1].Out;
    }

    bool IsValid() const { return Points.Num() >= 2; }
};

/** Reglages qui ne dependent d'aucune couche. */
USTRUCT()
struct FWorldGlobalSettings
{
    GENERATED_BODY()

    /** Altitude de reference, en voxels. */
    UPROPERTY(EditAnywhere)
    int32 BaseHeight = 408;

    UPROPERTY(EditAnywhere)
    int32 SeaLevel = 24.0f;

    UPROPERTY(EditAnywhere)
    float CaveFrequency = 0.038f;

    UPROPERTY(EditAnywhere)
    float CaveThreshold = 0.42f;

    /** Graine maitresse. Chaque couche ajoute son SeedOffset. */
    UPROPERTY(EditAnywhere)
    int32 MasterSeed = 1337;


    /** Force du domain warping. 0 = desactive. */
    UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
    float WarpStrength = 0.0f;

    UPROPERTY(EditAnywhere, meta = (ClampMin = "1.0"))
    float WarpWavelength = 5000.0f;

    UPROPERTY(EditAnywhere, meta = (ClampMin = "1.0"))
    int32 VeinMinDepth = 20;
    UPROPERTY(EditAnywhere, meta = (ClampMin = "1.0"))
    int32 VeinMaxDepth = 1500;
    UPROPERTY(EditAnywhere, meta = (ClampMin = "1.0"))
    float VeinThreshold = 0.3f;
    UPROPERTY(EditAnywhere, meta = (ClampMin = "1.0"))
    float ProvinceWavelength = 400000.0f;   // 40 km
    UPROPERTY(EditAnywhere, meta = (ClampMin = "1.0"))
    int32 ProvinceSeedOffset = 300;
    UPROPERTY(EditAnywhere, meta = (ClampMin = "1.0"))
    float VeinWavelength = 80.0f;           
    UPROPERTY(EditAnywhere, meta = (ClampMin = "1.0"))
    int32 VeinSeedOffset = 400;
    UPROPERTY(EditAnywhere, meta = (ClampMin = "1"))
    int32 VeinCellSize = 16;

};

USTRUCT()
struct FTerrainConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FWorldGlobalSettings Global;

    UPROPERTY(EditAnywhere)
    TArray<FControlFieldConfig> ControlFields;

    UPROPERTY(EditAnywhere)
    TArray<FTerrainLayer> Layers;

    /** Altitude de BASE = HeightSpline(valeur brute du champ ContinentalFieldIndex).
         *  Les couches s'ajoutent ensuite. Si invalide : Global.BaseHeight. */
    UPROPERTY(EditAnywhere)
    FTerrainSpline HeightSpline;

    UPROPERTY(EditAnywhere)
    int32 ContinentalFieldIndex = 0;
    UPROPERTY(EditAnywhere)
    int32 ErosionFieldIndex = 1;
};