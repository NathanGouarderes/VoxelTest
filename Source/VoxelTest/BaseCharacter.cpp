// Fill out your copyright notice in the Description page of Project Settings.

#include "BaseCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

// Sets default values
ABaseCharacter::ABaseCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
	GetMesh()->SetRelativeLocationAndRotation(FVector(0, 0, -96), FRotator(0, -90, 0));
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);
}

// Called when the game starts or when spawned
void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	const APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("ABaseCharacter::ABaseCharacter() --> Pas de PlayerController"));
		return;
	}

	auto* SubSystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (!SubSystem || !DefaultMappingContext)
	{
		UE_LOG(LogTemp, Error, TEXT("ABaseCharacter::ABaseCharacter() --> Subsystem ou IMC manquant"));
		return;
	}
	SubSystem->AddMappingContext(DefaultMappingContext, 0);
}

// Called every frame
void ABaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ABaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC) { UE_LOG(LogTemp, Error, TEXT(" ABaseCharacter::SetupPlayerInputComponent --> InputComponent pas Enhanced")); return; }

	if (MoveAction) EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABaseCharacter::Move);
	if (LookAction) EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABaseCharacter::Look);
}

void ABaseCharacter::Move(const FInputActionValue& Value)
{
	if (!Controller) return;


	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator YawOnly(0.f, Controller->GetControlRotation().Yaw, 0.f);
	UE_LOG(LogTemp, Warning, TEXT("ABaseCharacter::Move --> %s"), *Axis.ToString());


	AddMovementInput(YawOnly.RotateVector(FVector::ForwardVector), Axis.Y);
	AddMovementInput(YawOnly.RotateVector(FVector::RightVector), Axis.X);
}

void ABaseCharacter::Look(const FInputActionValue& Value)
{
	const FVector Axis = Value.Get<FVector>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

