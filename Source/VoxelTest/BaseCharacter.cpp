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
#include "../VoxelModule/Public/DebugMacro.h"
#include "InputActionValue.h"

// Sets default values
ABaseCharacter::ABaseCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer.SetDefaultSubobjectClass<UKiCharacterMovement>(
	ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
	GetMesh()->SetRelativeLocationAndRotation(FVector(0, 0, -96), FRotator(0, -90, 0));
	bUseControllerRotationYaw = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);

	WebComponent = CreateDefaultSubobject<UKiWebComponent>(TEXT("KiWebComponent"));
}

// Called when the game starts or when spawned
void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	GetCharacterMovement()->JumpZVelocity = 1500;
	GetCharacterMovement()->AirControl = 1;
	GetCharacterMovement()->GravityScale = 2.5;
	GetCharacterMovement()->Velocity *= 2;
	GetCharacterMovement()->MaxWalkSpeed = 1000;

	const APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("ABaseCharacter::BeginPlay() --> Pas de PlayerController"));
		return;
	}

	auto* SubSystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (!SubSystem || !DefaultMappingContext || !KiWebMappingContext)
	{
		UE_LOG(LogTemp, Error, TEXT("ABaseCharacter::BeginPlay() --> Subsystem ou IMC manquant"));
		return;
	}

	SubSystem->AddMappingContext(DefaultMappingContext, 0);
	SubSystem->AddMappingContext(KiWebMappingContext, 0);
}

// Called every frame
void ABaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// Les toiles sont simulees et dessinees par UKiWebComponent : rien a faire ici.
}

AChunckManager* ABaseCharacter::GetChunkManager()
{
	if (IsValid(ChunkManager))
	{
		return ChunkManager;
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AChunckManager::StaticClass(), Found);
	ChunkManager = (Found.Num() > 0) ? Cast<AChunckManager>(Found[0]) : nullptr;

	if (!ChunkManager)
	{
		NathanDebug(TEXT("Aucun AChunckManager trouve dans le monde"));
	}
	return ChunkManager;
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
	if (JumpAction)
	{
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ABaseCharacter::SetupPlayerInputComponent --> JumpAction non assignee dans le Blueprint"));
	}

	if (FireKiWebLeftArmAction)
	{
		EIC->BindAction(FireKiWebLeftArmAction, ETriggerEvent::Started, this, &ABaseCharacter::OnFireKiWebLeft);
		EIC->BindAction(FireKiWebLeftArmAction, ETriggerEvent::Completed, this, &ABaseCharacter::OnReleaseKiWebLeft);
		EIC->BindAction(FireKiWebLeftArmAction, ETriggerEvent::Canceled, this, &ABaseCharacter::OnReleaseKiWebLeft);
	}
	if (FireKiWebRightArmAction)
	{
		EIC->BindAction(FireKiWebRightArmAction, ETriggerEvent::Started, this, &ABaseCharacter::OnFireKiWebRight);
		EIC->BindAction(FireKiWebRightArmAction, ETriggerEvent::Completed, this, &ABaseCharacter::OnReleaseKiWebRight);
		EIC->BindAction(FireKiWebRightArmAction, ETriggerEvent::Canceled, this, &ABaseCharacter::OnReleaseKiWebRight);
	}
	if (ReelAction)
	{
		EIC->BindAction(ReelAction, ETriggerEvent::Triggered, this, &ABaseCharacter::OnReel);
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
	const FVector AimDir = ViewRot.Vector();   // deja normalise

	// On part du poignet, mais on vise la ou regarde la camera.
	const FVector Origin = WebComponent->GetWristLocation(Arm);

	if (!WebComponent->FireWeb(Arm, Origin, AimDir))
	{
		NathanDebug(TEXT("Tir de toile %d impossible"), static_cast<int32>(Arm));
	}
}

void ABaseCharacter::OnReleaseKiWeb(EKiArm Arm)
{
	if (WebComponent)
	{
		WebComponent->ReleaseWeb(Arm);
	}
}

void ABaseCharacter::Move(const FInputActionValue& Value)
{
	if (!Controller) return;

	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator YawOnly(0.f, Controller->GetControlRotation().Yaw, 0.f);

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
		WebComponent->ReleaseAll();
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

void ABaseCharacter::OnReel(const FInputActionValue& Value)
{
	if (!WebComponent)
	{
		return;
	}

	const float Notches = Value.Get<float>();   // +1 / -1 par cran
	NathanDebug(TEXT("Notche = %f"), Notches);
	if (FMath::IsNearlyZero(Notches))
	{
		return;
	}

	// Pour l'instant, la molette pilote les deux bras.
	// AdjustReelTarget ignore d'elle-meme un bras vide ou une toile non accrochee.
	WebComponent->AdjustReelTarget(EKiArm::Left, Notches);
	WebComponent->AdjustReelTarget(EKiArm::Right, Notches);
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

void ABaseCharacter::TreeDebugScatter(float Radius)
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AChunckManager::StaticClass(), Found);
	if (Found.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("TreeDebugScatter : aucun ChunckManager trouve"));
		return;
	}
	if (AChunckManager* Manager = Cast<AChunckManager>(Found[0]))
	{
		Manager->TreeDebugScatter(Radius);
	}
}

void ABaseCharacter::MeshTrees(float RadiusMeters)
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AChunckManager::StaticClass(), Found);
	if (Found.Num() == 0) return;
	Cast<AChunckManager>(Found[0])->MeshTrees(RadiusMeters);
}

void ABaseCharacter::CallMeasureCaveSlope(float RadiusMeters)
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AChunckManager::StaticClass(), Found);
	if (Found.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("ABaseCharacter::CallMeasureCaveSlope(float RadiusMeters) : aucun ChunckManager trouve"));
		return;
	}
	if (AChunckManager* Manager = Cast<AChunckManager>(Found[0]))
	{
		Manager->MeasureCaveSlope(RadiusMeters);
	}
}


void ABaseCharacter::GotoPeak()
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AChunckManager::StaticClass(), Found);
	if (Found.Num() > 0)
	{
		Cast<AChunckManager>(Found[0])->GotoPeak();
	}
}