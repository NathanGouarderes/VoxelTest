// Fill out your copyright notice in the Description page of Project Settings.

#include "BaseCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KiCharacterMovement.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

// Sets default values
ABaseCharacter::ABaseCharacter(const FObjectInitializer& ObjectInitializer) : Super (ObjectInitializer.SetDefaultSubobjectClass<UKiCharacterMovement>(
	ACharacter::CharacterMovementComponentName))
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
	GetMesh()->SetRelativeLocationAndRotation(FVector(0, 0, -96), FRotator(0, -90, 0));
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);
	WebComponent = CreateDefaultSubobject<UKiWebComponent>(TEXT("KiWeb"));
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

UKiCharacterMovement* ABaseCharacter::GetKiMovement() const
{
	return CastChecked<UKiCharacterMovement>(GetCharacterMovement());
}

// Called to bind functionality to input
void ABaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC) { UE_LOG(LogTemp, Error, TEXT(" ABaseCharacter::SetupPlayerInputComponent --> InputComponent pas Enhanced")); return; }

	if (MoveAction) EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABaseCharacter::Move);
	if (LookAction) EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABaseCharacter::Look);
	if (FireKiWebLeftArmAction)
	{
		EIC->BindAction(FireKiWebLeftArmAction, ETriggerEvent::Started, this, &ABaseCharacter::OnFireKiWebLeft);
		EIC->BindAction(FireKiWebLeftArmAction, ETriggerEvent::Completed, this, &ABaseCharacter::OnReleaseKiWebLeft);
		EIC->BindAction(FireKiWebLeftArmAction, ETriggerEvent::Canceled, this, &ABaseCharacter::OnReleaseKiWebLeft);
	}
}

void ABaseCharacter::OnFireKiWeb(EKiArm Arm)
{
	if (!WebComponent || !Controller)
	{
		return;
	}

	FVector ViewLoc;
	FRotator ViewRot;
	Controller->GetPlayerViewPoint(ViewLoc, ViewRot);

}

void ABaseCharacter::OnReleaseKiWeb(EKiArm Arm)
{
	UE_LOG(LogTemp, Warning, TEXT("ABaseCharacter::Move --> %d"), Arm);

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

UEnhancedInputLocalPlayerSubsystem* ABaseCharacter::GetInputSubsystem() const 
{
	const APlayerController* PC = Cast<APlayerController>(Controller);
	return PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
}

void ABaseCharacter::SetKiWebSkillActive(bool bActive)
{
	if (bIsKiWebSkillActive == bActive || !KiWebMappingContext)
	{
		return;
	}
	
	bIsKiWebSkillActive = bActive;
	if (auto* Subsystem = GetInputSubsystem())
	{
		if (bActive)
		{
			Subsystem->AddMappingContext(KiWebMappingContext, 1);
		}
		else
		{
			Subsystem->RemoveMappingContext(KiWebMappingContext);
		}
	}

	if (!bActive && WebComponent)
	{
		//WebComponent->ReleaseAll();
	}
}

void ABaseCharacter::OnFireKiWebLeft()
{
	OnFireKiWeb(EKiArm::Left);
}

void ABaseCharacter::OnFireKiWebRight()
{
	OnFireKiWeb(EKiArm::Right);
}

void ABaseCharacter::OnReleaseKiWebLeft()
{
	OnReleaseKiWeb(EKiArm::Left);
}

void ABaseCharacter::OnReleaseKiWebRight()
{
	OnReleaseKiWeb(EKiArm::Right);
}
FVector ABaseCharacter::GetCenterEyesLooking() const
{
	FVector  OutLocation;
	FRotator OutRotation;
	GetActorEyesViewPoint(OutLocation, OutRotation);
	return OutLocation + OutRotation.Vector() * 2000.0f;
}

void ABaseCharacter::CarveSphere(float Radius)
{
	const FVector Target = GetCenterEyesLooking();

	// Recuperation du manager (meme pattern que ResolveVoxelWorldIfNeeded).
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AChunckManager::StaticClass(), Found);
	if (Found.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("CarveSphere : aucun ChunckManager trouve"));
		return;
	}

	if (AChunckManager* Manager = Cast<AChunckManager>(Found[0]))
	{
		Manager->CarveSphereAt(Target, Radius);
	}
}

