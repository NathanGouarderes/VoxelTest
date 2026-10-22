// Fill out your copyright notice in the Description page of Project Settings.


#include "KiCharacterMovement.h"
#include "KiWebComponent.h"
#include "DebugMacro.h"

void UKiCharacterMovement::BeginPlay()
{
	Super::BeginPlay();
	WebComp = GetOwner()->FindComponentByClass<UKiWebComponent>();
	UE_LOG(LogTemp, Warning, TEXT("UKiCharacterMovement::BeginPlay() --> WebComp OK"));
	if (!WebComp.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("UKiCharacterMovement::BeginPlay() --> Pas de WebComp"));
	}
}

void UKiCharacterMovement::PerformMovement(float DeltaTime)
{
	Super::PerformMovement(DeltaTime);
	ApplyVoxelGroundGuard();
}


void UKiCharacterMovement::PhysCustom(float Deltatime, int32 Iterations)
{

	if (CustomMovementMode != static_cast<uint8>(EKiMovementMode::Swinging))
	{
		Super::PhysCustom(Deltatime, Iterations);
		return;
	}

	if (CustomMovementMode == static_cast<uint8>(EKiMovementMode::Swinging))
	{
		if (!WebComp.IsValid() || WebComp->GetActiveTethers().Num() <= 0)
		{
			SetMovementMode(MOVE_Falling);
			StartNewPhysics(Deltatime, Iterations);
			NathanDebug(TEXT("Passage en balancement"));
			return;
		}

		PhysSwing(Deltatime, Iterations);
	}
}

void UKiCharacterMovement::PhysSwing(float Dt, int32 Iterations)
{
	if (Dt < MIN_TICK_TIME || !WebComp.IsValid())
	{
		return;
	}
	++Iterations;

	const TArray<FKiWebTether, TInlineAllocator<2>> WebTethers = WebComp->GetActiveTethers();

	const int32 NumSubSteps = FMath::Clamp(FMath::CeilToInt(Dt * SwingSubstepHz), 1, 16);
	const float SubDt = Dt / NumSubSteps;
	const FVector Gravity(0.0, 0.0, GetGravityZ());

	const float DragFactor = FMath::Pow(FMath::Clamp(1.f - SwingDrag, 0.f, 1.f), SubDt);

	for (int32 Sub = 0; Sub < NumSubSteps; Sub++)
	{
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();

		Velocity += Gravity * SubDt;
		Velocity += GetFallingLateralAcceleration(SubDt) * SubDt;
		Velocity *= DragFactor;

		FVector Predicted = OldLocation + Velocity * SubDt;

		for (int32 It = 0; It < ConstraintIterations; It++)
		{
			for (const FKiWebTether& Tether : WebTethers)
			{
				const FVector EffectivePivot = Tether.Pivot - Tether.WristOffset;
				const double DistAtStart = FVector::Dist(OldLocation, EffectivePivot);

				const double AllowedRadius = FMath::Max(
					static_cast<double>(Tether.MaxDistance),
					DistAtStart - static_cast<double>(MaxCorrectionSpeed) * SubDt);

				const FVector FromPivot = Predicted - EffectivePivot;
				const double Dist = FromPivot.Size();
				if (Dist > AllowedRadius && Dist > KINDA_SMALL_NUMBER)
				{
					Predicted = EffectivePivot + FromPivot / Dist * AllowedRadius;
				}
			}
		}

		const FVector Delta = Predicted - OldLocation;
		FHitResult Hit(1.f);
		SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
		if (Hit.IsValidBlockingHit())
		{
			if (IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit))
			{
				const float RemainingTime = SubDt * (1.f - Hit.Time) + SubDt * (NumSubSteps - Sub - 1);
				SetMovementMode(MOVE_Falling);
				ProcessLanded(Hit, RemainingTime, Iterations);
				return;
			}
			HandleImpact(Hit, SubDt, Delta);
			SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
		}
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / SubDt;
	}
}

void UKiCharacterMovement::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	if (IsFalling() && WebComp.IsValid() && WebComp->GetActiveTethers().Num() > 0)
	{
		SetMovementMode(MOVE_Custom, static_cast<uint8>(EKiMovementMode::Swinging));
		NathanDebug(TEXT("Passage en balancement"));
	}
}

void UKiCharacterMovement::ApplyVoxelGroundGuard()
{
	if (!UpdatedComponent || !CharacterOwner)
	{
		return;
	}
	AChunckManager* Manager = GetChunkManager();
	if (!Manager)
	{
		return;
	}

	const float VoxelSizeCm = (float)Manager->VoxelSize;
	const float HalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Location = UpdatedComponent->GetComponentLocation();

	const FVector FeetCm(Location.X, Location.Y, Location.Z - HalfHeight + 1.0f);
	FIntVector Voxel = Manager->WorldToVoxelGlobal(FeetCm);

	uint8 MaterialId = 0;
	if (!Manager->IsSolidWorld(Voxel, MaterialId))
	{
		return;
	}
	for (int32 i = 0; i < MaxGroundRecoveryVoxels; ++i)
	{
		Voxel.Z += 1;
		if (!Manager->IsSolidWorld(Voxel, MaterialId))
		{
			const float GroundZCm = Voxel.Z * VoxelSizeCm;
			const FVector Corrected(Location.X, Location.Y, GroundZCm + HalfHeight + 0.5f);
			UpdatedComponent->SetWorldLocation(Corrected, false, nullptr, ETeleportType::TeleportPhysics);
			if (Velocity.Z < 0.0f)
			{
				Velocity.Z = 0.0f;
			}
			return;
		}
	}

}


AChunckManager* UKiCharacterMovement::GetChunkManager()
{
	if (!ChunkManager.IsValid() && GetWorld())
	{
		for (TActorIterator<AChunckManager> It(GetWorld()); It; ++It)
		{
			ChunkManager = *It;
			break;
		}
	}
	return ChunkManager.Get();
}
