// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../UVoxelBodyAsset.h"
#include "Engine/StaticMesh.h"
#include "UVoxelBodyComponent.generated.h"



UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VOXELMODULE_API UVoxelBodyComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UVoxelBodyComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UVoxelBodyAsset> Body;
	TArray<int32> SlotToBoneIndex;
	UPROPERTY(EditAnywhere)
	UStaticMesh* MeshCube;
	TArray<UInstancedStaticMeshComponent*> MeshCubeComponents;
		
};
