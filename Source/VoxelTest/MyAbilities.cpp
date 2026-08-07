// Fill out your copyright notice in the Description page of Project Settings.


#include "MyAbilities.h"

// Sets default values
AMyAbilities::AMyAbilities()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AMyAbilities::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AMyAbilities::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

