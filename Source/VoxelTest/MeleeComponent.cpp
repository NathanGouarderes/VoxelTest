// Fill out your copyright notice in the Description page of Project Settings.


#include "MeleeComponent.h"
#include "GameFramework/Character.h"


// Sets default values for this component's properties
UMeleeComponent::UMeleeComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UMeleeComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UMeleeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UMeleeComponent::PunchRight()
{
	UE_LOG(LogTemp, Log, TEXT("UMeleeComponent::PunchRight()"));
	ACharacter* Owner = Cast<ACharacter>(GetOwner());
	if (!Owner || !PunchRightMontage)
	{
		UE_LOG(LogTemp, Error, TEXT("UMeleeComponent::PunchRight() --> !Actor || PunchRightMontage"));
		return;
	}
	UAnimInstance* AnimInstance = Owner->GetMesh()->GetAnimInstance();
	if (AnimInstance && AnimInstance->Montage_IsPlaying(PunchRightMontage))
	{
		return;
	}
	const float Duration = Owner->PlayAnimMontage(PunchRightMontage);
	UE_LOG(LogTemp, Log, TEXT("Punch : duree = %.2f s"), Duration);
}