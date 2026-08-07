// Fill out your copyright notice in the Description page of Project Settings.


#include "KiWebComponent.h"
#include "DebugMacro.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

// Sets default values for this component's properties
UKiWebComponent::UKiWebComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	Webs.SetNum(2);
	Webs[0].Arm = EKiArm::Left;
	Webs[1].Arm = EKiArm::Right;


	// ...
}


// Called when the game starts
void UKiWebComponent::BeginPlay()
{
	Super::BeginPlay();
	Owner = GetOwner();
	if (!Owner)
	{
		NathanDebug(TEXT("Owner NULL"));
		return;
	}
	FCollisionQueryParams Params(FName("KiWebTrace"), true, Owner);
	TArray<AActor*> Attached;
	Owner->GetAttachedActors(Attached, /*bResetArray=*/true, /*bRecursivelyInclude=*/true);
	Params.AddIgnoredActors(Attached);
	

	UWorld* World = GetWorld();
	ACharacter* Character = Cast<ACharacter>(Owner);
	if (!World || !Owner)
	{
		NathanDebug(TEXT("!World || !Owner"));
		return;
	}
	if (Character == nullptr || !Character)
	{
		NathanDebug(TEXT("Character == nullptr || !Character"))
	}
	OwnerMesh = Character->GetMesh();
	if (OwnerMesh == nullptr)
	{
		NathanDebug(TEXT("OwnerMesh == nullptr"));

	}
	if (!OwnerMesh->DoesSocketExist(HandSocketRight))
	{
		NathanDebug(TEXT("!OwnerMesh->DoesSocketExist(HandSocketRight)"));
		return;
	}
	if (!OwnerMesh->DoesSocketExist(HandSocketLeft))
	{
		NathanDebug(TEXT("!OwnerMesh->DoesSocketExist(HandSocketLeft)"));
		return;
	}
	World->DebugDrawTraceTag = FName(TEXT("KiWebTrace"));
}


// Called every frame
void UKiWebComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

FVector UKiWebComponent::GetHandSocketLocation(EKiArm Arm) const
{
	if (!OwnerMesh.IsValid())
	{
		NathanDebug(TEXT("!OwnerMesh.IsValid()"));
		return FVector::ZeroVector;
	}

	const FName Socket = (Arm == EKiArm::Left) ? HandSocketLeft : HandSocketRight;
}

bool UKiWebComponent::FireWeb(EKiArm Arm, const FVector& Origin, const FVector& Direction)
{
	UWorld* World = GetWorld();
	if (!Owner || Owner == nullptr)
	{
		Owner = GetOwner();
	}

	const FVector Dir = Direction.GetSafeNormal();
	FVector End = Origin * Dir + MaxFireDistance;
	FVector HandLocation = GetHandSocketLocation(Arm);
	TArray<AActor*> AttachedActors;
	FHitResult Hit;
	bool bHit;

	if (!World || !Owner)
	{
		NathanDebug(TEXT("!World || !Owner"));
		return;
	}

	FCollisionQueryParams Params(FName("KiWebTrace"), true, Owner);
	Owner->GetAttachedActors(AttachedActors, /*bResetArray=*/true, /*bRecursivelyInclude=*/true);
	Params.AddIgnoredActors(AttachedActors);
	bHit = World->LineTraceSingleByChannel(Hit, Origin, End, WebTraceChanel, Params);
	if (!bHit || Hit.bStartPenetrating)
	{

	}
}