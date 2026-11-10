// Fill out your copyright notice in the Description page of Project Settings.


#include "MeleeComponent.h"
#include "GameFramework/Character.h"
#include "BaseCharacter.h"
#include "FVoxelQueryScope.h"


// Sets default values for this component's properties
UMeleeComponent::UMeleeComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;           // le composant PEUT tiquer
	PrimaryComponentTick.bStartWithTickEnabled = false; // mais seulement pendant un coup
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork; // après l'animation

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
	TickStrike();
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
	FVector HandPosition = Owner->GetMesh()->GetSocketLocation("hand_r");
	UE_LOG(LogTemp, Log, TEXT("Punch : duree = %.2f s"), Duration);
}

void UMeleeComponent::BeginStrike(const UAnimMontage* Montage, FName SocketName, float InWindowStart, float InWindowEnd)
{
	const ACharacter* Owner = Cast<ACharacter>(GetOwner());
	if (!Owner || !Montage)
	{
		UE_LOG(LogTemp, Error, TEXT("BeginStrike : proprietaire ou Montage nul"));
		return;
	}
	Trajectory = GetOrBakeTrajectory(Montage, SocketName);
	if (!Trajectory)
	{
		return;
	}
	StrikeMontage = Montage;
	WindowEnd = InWindowEnd;
	PrevTime = InWindowStart;   // on part du début EXACT de la fenêtre
	PrevMeshTransform = Owner->GetMesh()->GetComponentTransform();
	RemainingEnergy = StrikeEnergy;
	bIsStrikeActive = true;
	bTerrainHitThisStrike = false;
	bStrikeBlocked = false;
	SetComponentTickEnabled(true);
}

void UMeleeComponent::TickStrike()
{
	if (!bIsStrikeActive)
	{
		return;
	}
	const ACharacter* Owner = Cast<ACharacter>(GetOwner());
	const UAnimInstance* Anim = Owner ? Owner->GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim || !Anim->Montage_IsPlaying(StrikeMontage))
	{
		return;
	}
	const float Now = Anim->Montage_GetPosition(StrikeMontage);
	const FVector Baked = Owner->GetMesh()->GetComponentTransform().TransformPosition(Trajectory->SampleAt(Now));
	const FVector Real = Owner->GetMesh()->GetSocketLocation("hand_r");   // "hand_r"
	UE_LOG(LogTemp, Log, TEXT("Ecart bake/reel = %.2f cm"), FVector::Dist(Baked, Real));
	AdvanceStrike(FMath::Min(Anim->Montage_GetPosition(StrikeMontage), WindowEnd));
}

void UMeleeComponent::AdvanceStrike(float TargetTime)
{
	const ACharacter* Owner = Cast<ACharacter>(GetOwner());
	if (!Owner)
	{
		UE_LOG(LogTemp, Error, TEXT("UMeleeComponent::AdvanceStrike(float TargetTime) --> !Owner"));
		return;
	}
	if (!Trajectory)
	{
		UE_LOG(LogTemp, Error, TEXT("UMeleeComponent::AdvanceStrike(float TargetTime) --> !Trajectory"));
		return;
	}
	if (TargetTime <= PrevTime)
	{
		UE_LOG(LogTemp, Error, TEXT("UMeleeComponent::AdvanceStrike(float TargetTime) --> TargetTime <= PrevTime"));
		return;
	}
	const FTransform MeshNow = Owner->GetMesh()->GetComponentTransform();
	const float Span = TargetTime - PrevTime;
	const float Step = 1.f / Trajectory->SampleRate;

	FVector A = PrevMeshTransform.TransformPosition(Trajectory->SampleAt(PrevTime));
	for (float T = PrevTime + Step; ; T += Step)
	{
		const float Tc = FMath::Min(T, TargetTime);
		FTransform Mesh;
		Mesh.Blend(PrevMeshTransform, MeshNow, (Tc - PrevTime) / Span);   // le personnage a bougé entre les deux ticks
		const FVector B = Mesh.TransformPosition(Trajectory->SampleAt(Tc));
		PendingSegments.Add({A,B});
		A = B;
		if (Tc >= TargetTime)
		{
			break;
		}
	}
	PrevTime = TargetTime;
	PrevMeshTransform = MeshNow;
	ProcessSegments();
}

void UMeleeComponent::EndStrike()
{
	const ACharacter* Owner = Cast<ACharacter>(GetOwner());
	const UAnimInstance* Anim = Owner ? Owner->GetMesh()->GetAnimInstance() : nullptr;
	if (bIsStrikeActive && Anim && Anim->Montage_IsPlaying(StrikeMontage))
	{
		AdvanceStrike(WindowEnd);   // termine la fenêtre, même si le dernier tick est arrivé trop tôt
	}
	bIsStrikeActive = false;
	Trajectory.Reset();
	StrikeMontage = nullptr;
	SetComponentTickEnabled(false);
	UE_LOG(LogTemp, Log, TEXT("EndStrike"));
}

void UMeleeComponent::TestSegment(const FVector& A, const FVector& B)
{
	DrawDebugLine(GetWorld(), A, B, FColor::Red, false, 10.f, SDPG_Foreground, 20.f);
	// ensuite : Sweep du terrain, puis test des corps
}


TSharedPtr<const FStrikeTrajectory> UMeleeComponent::GetOrBakeTrajectory(const UAnimMontage* Montage, FName BoneName)
{
	const TPair<const UAnimMontage*, FName> Key(Montage, BoneName);
	if (const TSharedPtr<const FStrikeTrajectory>* Found = TrajectoryCache.Find(Key))
	{
		return *Found;
	}

	// Version 1 : un Montage = une seule animation, lue depuis 0, à vitesse 1
	if (!Montage || Montage->SlotAnimTracks.Num() != 1 || Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() != 1)
	{
		UE_LOG(LogTemp, Error, TEXT("Bake : le Montage doit contenir exactement un segment"));
		return nullptr;
	}
	const FAnimSegment& Segment = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0];
	const UAnimSequence* Sequence = Cast<UAnimSequence>(Segment.GetAnimReference());
	if (!Sequence || Segment.StartPos != 0.f || Segment.AnimStartTime != 0.f || Segment.AnimPlayRate != 1.f)
	{
		UE_LOG(LogTemp, Error, TEXT("Bake : segment non supporte (decalage ou vitesse differente de 1)"));
		return nullptr;
	}

	const FReferenceSkeleton& Ref = Sequence->GetSkeleton()->GetReferenceSkeleton();
	const int32 Bone = Ref.FindBoneIndex(BoneName);
	if (Bone == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("Bake : os %s introuvable"), *BoneName.ToString());
		return nullptr;
	}

	TArray<int32> Chain;   // de la main vers la racine
	for (int32 B = Bone; B != INDEX_NONE; B = Ref.GetParentIndex(B))
	{
		Chain.Add(B);
	}

	TSharedPtr<FStrikeTrajectory> Out = MakeShared<FStrikeTrajectory>();
	Out->Duration = Sequence->GetPlayLength();
	const int32 Num = FMath::CeilToInt(Out->Duration * Out->SampleRate) + 1;
	Out->Points.SetNum(Num);

	for (int32 i = 0; i < Num; ++i)
	{
		const double Time = FMath::Min(i / static_cast<double>(Out->SampleRate), static_cast<double>(Out->Duration));
		FTransform ComponentSpace = FTransform::Identity;
		for (const int32 B : Chain)
		{
			FTransform Local;
			Sequence->GetBoneTransform(Local, FSkeletonPoseBoneIndex(B), Time, false);
			ComponentSpace = ComponentSpace * Local;   // A * B : A appliqué d'abord, puis B
		}
		Out->Points[i] = ComponentSpace.GetLocation();
	}

	UE_LOG(LogTemp, Log, TEXT("Bake : %s / %s -> %d echantillons sur %.2f s"),
		*Montage->GetName(), *BoneName.ToString(), Num, Out->Duration);
	TrajectoryCache.Add(Key, Out);
	return Out;
}

void UMeleeComponent::ProcessSegments()
{
	if (PendingSegments.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("UMeleeComponent::ProcessSegments() --> PendingSegments.IsEmpty()"));
		return;
	}

	// Passe 1 : terrain
	ABaseCharacter* Character = Cast<ABaseCharacter>(GetOwner());
	AChunckManager* Manager = Character ? Character->GetChunkManager() : nullptr;
	FIntVector HittedVoxel;
	bool bCarve = false;
	if (Manager && !bTerrainHitThisStrike)
	{
		FVoxelQueryScope Query(*Manager);   // verrou pris une seule fois pour tous les segments
		for (const FStrikeSegment& S : PendingSegments)
		{
			const FVector Dir = (S.B - S.A).GetSafeNormal();
			const FVoxelSweepHit Hit = Query.Sweep(S.A, S.B + Dir * FistRadius);
			DrawDebugLine(GetWorld(), S.A, S.B, FColor::Red, false, 1.f, SDPG_Foreground, 2.f);

			if (Hit.bHit)
			{
				bTerrainHitThisStrike = true;
				HittedVoxel = Hit.Voxel;
				bCarve = true;
				UE_LOG(LogTemp, Log, TEXT("Terrain touche : point=%s normale=%s voxel=%s dedans=%d"),
					*Hit.Point.ToString(), *Hit.Normal.ToString(), *Hit.Voxel.ToString(), Hit.bStartedInside);
				DrawDebugPoint(GetWorld(), Hit.Point, 14.f, FColor::Green, false, 3.f, SDPG_Foreground);

				break;   // un seul impact terrain par coup, pour l'instant
			}
		}
	}   // ← le verrou est relâché ici
	if (bCarve)
	{
		Manager->CarveVoxelsAt(HittedVoxel, FistRadius);
	}

	// Passe 2 : corps 

	if (bStrikeBlocked || RemainingEnergy <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("UMeleeComponent::ProcessSegments() --> bStrikeBlocked || RemainingEnergy <= 0"));
		PendingSegments.Reset();
		return;
	}
	for (TObjectIterator<UVoxelBodyComponent> It; It; ++It)
	{
		UVoxelBodyComponent* Target = *It;
		if (Target->GetWorld() != GetWorld() || Target->GetOwner() == GetOwner())
		{
			continue;   // autre monde (CDO, éditeur), ou mon propre corps
		}
		int32 Removed = 0;
		for (const FStrikeSegment& StrikeSegment : PendingSegments)
		{
			Removed += Target->CarveCapsuleWorld(StrikeSegment.A, StrikeSegment.B, FistRadius, StrikePressure, RemainingEnergy, bStrikeBlocked);
			UE_LOG(LogTemp, Warning, TEXT("UMeleeComponent::ProcessSegments() --> StrikePressure : %d, RemainingEnergy : %d, bStrikeBlocked %d"), StrikePressure, RemainingEnergy, bStrikeBlocked);
			if (bStrikeBlocked || RemainingEnergy <= 0)
			{
				break;
			}
		}
		if (Removed > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("UMeleeComponent::ProcessSegments() --> Corps %s : %d cellules retirees"), *GetNameSafe(Target->GetOwner()), Removed);
		}
	}

	PendingSegments.Reset();   // vide le tableau sans libérer sa mémoire
}