#pragma once

#include "CoreMinimal.h"

enum class EChunkVariant
{
    Full,       // 100% solides
    HalfFull,   // 50% random solides
    Sparse,     // 20% solides avec clusters
    Empty,      // 0% (pour tests)
};