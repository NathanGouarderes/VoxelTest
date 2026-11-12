// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
//#include "../VoxelTest/Notify/UStrikeWindowNotify.h"
#include "MeleeComponent.generated.h"

class UAnimMontage;


struct FStrikeTrajectory
{
	float SampleRate = 120.0f;
	float Duration = 0.0f;
	TArray<FVector> Points;
	FVector SampleAt(float Time) const
	{
		const float F = FMath::Clamp(Time, 0.f, Duration) * SampleRate;
		const int32 I0 = FMath::Clamp(FMath::FloorToInt(F), 0, Points.Num() - 1);
		const int32 I1 = FMath::Min(I0 + 1, Points.Num() - 1);
		return FMath::Lerp(Points[I0], Points[I1], F - I0);
	}
};

struct FStrikeSegment
{
	FVector A;
	FVector B;
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VOXELTEST_API UMeleeComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UMeleeComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void PunchRight();
	void BeginStrike(const UAnimMontage* Montage, FName SocketName, float InWindowStart, float InWindowEnd);
	void TickStrike();
	void EndStrike();
	void TestSegment(const FVector& A, const FVector& B);
	void AdvanceStrike(float TargetTime);
	void ProcessSegments();
	void FreezeAim();
	FVector SampleAimed(float Time) const;
	TSharedPtr<const FStrikeTrajectory> GetOrBakeTrajectory(const UAnimMontage* Montage, FName BoneName);

	UPROPERTY(EditDefaultsOnly, Category = "Melee")
	TObjectPtr<UAnimMontage> PunchRightMontage;
	bool bTerrainHitThisStrike = false;
	TArray<FStrikeSegment> PendingSegments;
	UPROPERTY(EditAnywhere, Category = "Melee")
	int32 StrikePressure = 5000;
	UPROPERTY(EditAnywhere, Category = "Melee")
	int64 StrikeEnergy = 2000;
	int32 RemainingEnergy;
	bool bStrikeBlocked;
	UPROPERTY(EditAnywhere, Category = "Melee")
	float FistRadius = 6.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Melee|Aim")
	FName AimPivotBone = TEXT("spine_03");
	UPROPERTY(EditDefaultsOnly, Category = "Melee|Aim")
	float AimPitchMin = -60.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Melee|Aim")
	float AimPitchMax = 60.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Melee|Aim")
	FRotator AimRotationCS;
	UPROPERTY(EditDefaultsOnly, Category = "Melee|Aim")
	FQuat AimQuatCS = FQuat::Identity;
	float StrikePitch;
	TSharedPtr<const FStrikeTrajectory> PivotTrajectory;
	FVector HandCur = FVector::ZeroVector;
	FVector HandPrev = FVector::ZeroVector;
	bool bIsStrikeActive = false;
	FName StrikeSocket = TEXT("hand_r");
	TMap<TPair<const UAnimMontage*, FName>, TSharedPtr<const FStrikeTrajectory>> TrajectoryCache;
	TSharedPtr<const FStrikeTrajectory> Trajectory;
	const UAnimMontage* StrikeMontage = nullptr;   // gardé en vie par PunchRightMontage
	float WindowEnd = 0.f;
	float PrevTime = 0.f;
	FTransform PrevMeshTransform;
	
};
