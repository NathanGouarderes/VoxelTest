// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
struct FPackedQuad
{
    uint8 P[3];         // origine du quad (V1). Composante Axis dans [0..Size], autres dans [0..Size-1]
    uint8 Axis;         // 0=X 1=Y 2=Z
    int8  Normal;       // +1 / -1
    uint8 W;            // extent le long de (Axis+1)%3
    uint8 H;            // extent le long de (Axis+2)%3
    uint8 MaterialId;   // reserve : la fusion greedy ne discrimine pas encore le materiau
};
static_assert(sizeof(FPackedQuad) == 8, "FPackedQuad doit rester a 8 octets");

// Liste de quads d'une brique : IMMUABLE une fois construite.
// Le partage par refcount evite toute copie entre le cache (game thread) et le job (worker).
using FBrickQuadsRef = TSharedPtr<const TArray<FPackedQuad>, ESPMode::ThreadSafe>;