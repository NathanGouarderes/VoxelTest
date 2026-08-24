#pragma once
#include "CoreMinimal.h"
#include "FVoxelMaterialPhysics.generated.h"

USTRUCT()
struct FVoxelMaterialPhysics
{
    GENERATED_BODY()
    float Density;
    float TensileStrength;
    float ThermalConductivity;
    float MeltingPoint;
    float ElectricalConductivity;
    float Hardness;
    float ExcavationResistance;
};