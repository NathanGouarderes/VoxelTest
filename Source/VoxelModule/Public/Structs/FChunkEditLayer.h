#pragma once
#include "CoreMinimal.h"
#include "Math/IntVector.h"
#include "FVoxelDataStructure.h"


USTRUCT()
struct FChunkEditLayer
{
  TMap<int32, FVoxelDataStructure> Edits; // la clef est l'index local du voxel édité (de 0 à 127)
  uint32 Revision; 
};
