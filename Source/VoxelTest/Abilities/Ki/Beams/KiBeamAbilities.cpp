// Fill out your copyright notice in the Description page of Project Settings.


#include "../../VoxelTest/Abilities/Ki/Beams/KiBeamAbilities.h"

// Sets default values for this component's properties
UKiBeamAbilities::UKiBeamAbilities()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UKiBeamAbilities::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UKiBeamAbilities::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

