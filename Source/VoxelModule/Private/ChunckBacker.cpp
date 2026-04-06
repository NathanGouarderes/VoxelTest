// Fill out your copyright notice in the Description page of Project Settings.


#include "ChunckBacker.h"

// Sets default values
AChunckBacker::AChunckBacker()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	ProceduralMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMeshComponent"));
	RootComponent = ProceduralMeshComponent;
	ProceduralMeshComponent->bUseAsyncCooking = true;

}

// Called when the game starts or when spawned
void AChunckBacker::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AChunckBacker::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);


}

