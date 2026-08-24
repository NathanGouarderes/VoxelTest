// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

enum class ECollisionState : uint8
{
    None,       // hors anneau, aucune collision
    Queued,     // job en file
    Building,   // worker en cours ou cook Chaos en cours
    Ready,      // collision LOD0 valide et enregistrée
    Stale       // une édition a invalidé la collision actuelle
};