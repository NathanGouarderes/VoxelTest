#pragma once

#include "CoreMinimal.h"

// Catégorie de log dédiée (optionnel mais recommandé)
VOXELMODULE_API DECLARE_LOG_CATEGORY_EXTERN(LogNathanDebug, Log, All);

// Macro variadique : NathanDebug(TEXT("Valeur: %d"), x)
#define NathanDebug(Format, ...) \
    UE_LOG(LogNathanDebug, Log, TEXT("%s : %s --> ") Format, \
        *FDateTime::Now().ToString(TEXT("%H:%M:%S.%s")), \
        ANSI_TO_TCHAR(__FUNCTION__), \
        ##__VA_ARGS__)