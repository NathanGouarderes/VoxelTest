// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FKiWeb.h"
#include "KiWebComponent.generated.h"

class AChunckManager;
class USkeletalMeshComponent;

UCLASS(ClassGroup = (Ki), meta = (BlueprintSpawnableComponent))
class VOXELTEST_API UKiWebComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKiWebComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Tire une toile depuis le poignet du bras donne, dans la direction donnee. */
	bool FireWeb(EKiArm Arm, const FVector& Origin, const FVector& Direction);

	/** Detache la toile tenue par ce bras. Elle reste dans le monde si elle est ancree. */
	void ReleaseWeb(EKiArm Arm);
	void ReleaseAll();

	/** true si le socket de main existe ; sinon OutLocation n'est pas modifie. */
	bool TryGetHandSocketLocation(EKiArm Arm, FVector& OutLocation) const;

	/** Socket de main si disponible, sinon centre de l'acteur. */
	FVector GetWristLocation(EKiArm Arm) const;

	AChunckManager* GetChunkManager();
	TArray<FKiWebTether, TInlineAllocator<2>> GetActiveTethers();
	void AdjustReelTarget(EKiArm Arm, const float Notches);


	/** Accès direct pour le CMC — pas de copie. */
	FORCEINLINE TArray<FKiWeb>& GetAllWebs() { return Webs; }
	FORCEINLINE const FKiResolveContext& GetResolveContext() const { return ResolveCtx; }


	// --- Tir -----------------------------------------------------------------
	UPROPERTY(EditAnywhere, Category = "Ki|Web") float MaxFireDistance = 6000.f;
	UPROPERTY(EditAnywhere, Category = "Ki|Web") float AimConeHalfAngle = 12.f;
	UPROPERTY(EditAnywhere, Category = "Ki|Web") FName HandSocketLeft = TEXT("hand_l");
	UPROPERTY(EditAnywhere, Category = "Ki|Web") FName HandSocketRight = TEXT("hand_r");
	UPROPERTY(EditAnywhere, Category = "Ki|Web") TEnumAsByte<ECollisionChannel> WebTraceChanel = ECC_Visibility; // inutilise pour l'instant (ancrage acteurs a venir)
	UPROPERTY(EditAnywhere, Category = "Ki|Web", meta = (ClampMin = "100")) float TipSpeed = 6000.f;
	UPROPERTY(EditAnywhere, Category = "Ki|Web", meta = (ClampMin = "3")) int32 MaxWebs = 30;

	// --- Simulation ------------------------------------------------------------
	UPROPERTY(EditAnywhere, Category = "Ki|Web|Simulation", meta = (ClampMin = "5")) float ParticleSpacing = 30.f;
	UPROPERTY(EditAnywhere, Category = "Ki|Web|Simulation", meta = (ClampMin = "0.001")) float FixedDt = 1.f / 120.f;
	UPROPERTY(EditAnywhere, Category = "Ki|Web|Simulation", meta = (ClampMin = "1")) int32 MaxSimStepsPerFrame = 8;
	UPROPERTY(EditAnywhere, Category = "Ki|Web|Simulation", meta = (ClampMin = "1")) int32 SolverIterations = 10;
	UPROPERTY(EditAnywhere, Category = "Ki|Web|Simulation", meta = (ClampMin = "0", ClampMax = "1")) float Damping = 0.99f;
	UPROPERTY(EditAnywhere, Category = "Ki|Web|Simulation") float WebGravityScale = 1.f;
	UPROPERTY(EditAnywhere, Category = "Ki|Web|Simulation", meta = (ClampMin = "0.1")) float WebRadius = 2.f;
	UPROPERTY(EditAnywhere, Category = "Ki|Web|Simulation", meta = (ClampMin = "0", ClampMax = "1")) float WebFriction = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Ki|Web|Simulation", meta = (ClampMin = "0")) float WebCompliance = 0.f;
	UPROPERTY(EditAnywhere, Category = "Ki|Web|Simulation", meta = (ClampMin = "0")) double DampingPerSecond = 0.5;
	UPROPERTY(EditAnywhere, Category = "Ki|Web|Reel", meta = (ClampMin = "0")) float ReelSpeed = 300.f;        // cm/s
	UPROPERTY(EditAnywhere, Category = "Ki|Web|Reel", meta = (ClampMin = "0")) float ReelStepPerNotch = 50.f;
	// --- Debug -----------------------------------------------------------------
	UPROPERTY(EditAnywhere, Category = "Ki|Web|Debug") bool bDrawTrace = true;


	UPROPERTY(Transient) TWeakObjectPtr<USkeletalMeshComponent> OwnerMesh;

protected:
	void StepWeb(FKiWeb& Web, float Dt, const FVector& Gravity);
	void AdvanceTip(FKiWeb& Web, float Dt);
	void IntegrateParticles(FKiWeb& Web, float Dt, const FVector& Gravity) const;
	void SolveConstraints(FKiWeb& Web, float Dt) const;
	void FillWebParticles(FKiWeb& KiWeb, float Dt);
	void ReelWebParticles(FKiWeb& KiWeb, float Dt);
	void DrawWebsDebug() const;
	void RemoveWebAt(int32 Index);
	bool DetectContacts(FKiWeb& Web, class FVoxelQueryScope& Query) const;
	static void SolveContactConstraint(FKiWebParticle& P, double Radius);
	void ApplyFriction(FKiWeb& Web) const;
	static void SolveDistanceConstraint(FKiWebParticle& A, FKiWebParticle& B, double RestLength, double AlphaTilde, double& Lambda);
	static void SolvePinConstraint(FKiWebParticle& P, const FVector& PinLocation, double RestLength);

	UPROPERTY()
	AActor* Owner = nullptr;

	// Toutes les toiles existantes (tenues, pieges, pendantes). Etat runtime : jamais serialise.
	UPROPERTY(Transient)
	TArray<FKiWeb> Webs;

	UPROPERTY(Transient)
	TObjectPtr<AChunckManager> ChunkManager;

	FKiResolveContext ResolveCtx;
	int32 ValidityFrameCounter = 0;

	// Index dans Webs de la toile tenue par chaque bras (EKiArm -> 0/1). INDEX_NONE = main vide.
	// Un INDEX, jamais une reference : un Add dans Webs peut reallouer le tableau.
	int32 HeldWebIndex[2] = { INDEX_NONE, INDEX_NONE };

	float SimAccumulator = 0.f;
};