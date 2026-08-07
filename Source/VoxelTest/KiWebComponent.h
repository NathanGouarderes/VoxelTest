// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "Components/ActorComponent.h"
#include "FKiWeb.h"
#include "KiWebComponent.generated.h"


UCLASS( ClassGroup=(Ki), meta=(BlueprintSpawnableComponent) )
class VOXELTEST_API UKiWebComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UKiWebComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool FireWeb(EKiArm Arm, const FVector& Origin, const FVector& Direction);
	void ReleaseWeb(EKiArm Arm);
	void ReleaseAll();
	FVector GetHandSocketLocation(EKiArm Arm) const;

	/** Accès direct pour le CMC — pas de copie. */
	FORCEINLINE TArray<FKiWeb>& GetAllWebs() { return Webs; }
	FORCEINLINE const FKiResolveContext& GetResolveContext() const { return ResolveCtx; }

	UPROPERTY(EditAnywhere, Category = "Ki|Web") float MaxFireDistance = 6000.f;
	UPROPERTY(EditAnywhere, Category = "Ki|Web") float AimConeHalfAngle = 12.f;
	UPROPERTY(EditAnywhere, Category = "Ki|Web") FName HandSocketLeft = TEXT("hand_l");
	UPROPERTY(EditAnywhere, Category = "Ki|Web") FName HandSocketRight = TEXT("hand_r");
	UPROPERTY(EditAnywhere, Category = "Ki|Web") TEnumAsByte<ECollisionChannel> WebTraceChanel = ECC_Visibility;
	UPROPERTY(EditAnywhere, Category = "Ki|Web|Debug") bool bDrawTrace = true;
	UPROPERTY(Transient) TWeakObjectPtr<USkeletalMeshComponent> OwnerMesh;


protected:
	void RefreshValidity();   // toutes les N frames, pas chaque tick
	//bool FindBestAnchor(const FVector& Origin, const FVector& Dir, FKiEndpoint& Out) const;
	UPROPERTY()
	AActor* Owner;
	UPROPERTY()

	TArray<FKiWeb> Webs;
	FKiResolveContext ResolveCtx;
	int32 ValidityFrameCounter = 0;

		
};
