// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PlayerControllerComponent.generated.h"

/**
 * 
 */
UCLASS()
class VOXELTEST_API APlayerControllerComponent : public APlayerController
{
	GENERATED_BODY()
	
	virtual void SetupInputComponent() override;
	void OnDestroyInput();
	UFUNCTION(Exec)
	void VoxelCarve(int32 I, int32 J, int32 K, int32 Radius);
};
