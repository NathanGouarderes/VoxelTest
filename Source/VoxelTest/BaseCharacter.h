// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "InputMappingContext.h"
#include "EnhancedInputSubsystems.h"
#include "KiWebComponent.h"
#include "InputAction.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "BaseCharacter.generated.h"

class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

UCLASS()
class VOXELTEST_API ABaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABaseCharacter(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintCallable, Category = "Ki")
	UKiCharacterMovement* GetKiMovement() const;

	UEnhancedInputLocalPlayerSubsystem* GetInputSubsystem() const;

	void SetKiWebSkillActive(bool Active);
	void OnFireKiWeb(EKiArm Arm);
	void OnReleaseKiWeb(EKiArm Arm);
	void OnFireKiWebRight();
	void OnFireKiWebLeft();
	void OnReleaseKiWebLeft();
	void OnReleaseKiWebRight();





protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ki")
	TObjectPtr<UKiWebComponent> WebComponent;
	UPROPERTY(EditDefaultsOnly, Category = "Input|Ki")
	TObjectPtr<UInputAction> FireKiWebLeftArm;
	UPROPERTY(EditDefaultsOnly, Category = "Input|Ki")
	TObjectPtr<UInputAction> FireKiWebRightArm;
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> KiWebMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> FireKiWebLeftArmAction;

	bool bIsKiWebSkillActive = false;

	

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
};
