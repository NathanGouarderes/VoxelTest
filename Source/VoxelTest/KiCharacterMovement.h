// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KiCharacterMovement.generated.h"

/**
 * 
 */
class UKiWebComponent;

UENUM()
enum class EKiMovementMode : uint8 {None = 0, Swinging = 1};


UCLASS()
class VOXELTEST_API UKiCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()
public:

	//virtual void PhysCustom(float Deltatime, int32 Iterations) override;
	//virtual float GetMaxSpeed() const override;
    //virtual void OnMovementModeChanged(EMovementMode PrevMode, uint8 PrevCustomMode) override;

	//void EnterSwinging();
	//void ExitSwinging();

	UPROPERTY(EditAnywhere, Category = "Ki|Swing") float SwingSubstepHz = 120.f;
	UPROPERTY(EditAnywhere, Category = "Ki|Swing") int32 ConstraintIterations = 6;
	UPROPERTY(EditAnywhere, Category = "Ki|Swing") float SwingDrag = 0.02f;

protected:
	//void PhysSwing(float dt, int32 Iterations);
	//void SolveTether(float dt, int32 SubstepIndex);

	UPROPERTY(Transient) TWeakObjectPtr<UKiWebComponent> WebComp;
	
};
