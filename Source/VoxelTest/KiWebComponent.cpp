// Fill out your copyright notice in the Description page of Project Settings.

#include "KiWebComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "DebugMacro.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "FVoxelQueryScope.h"

UKiWebComponent::UKiWebComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Simulation APRES le CMC et l'animation : le poignet lu ici est celui de CETTE frame.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UKiWebComponent::BeginPlay()
{
	Super::BeginPlay();

	Webs.Reset();
	HeldWebIndex[0] = INDEX_NONE;
	HeldWebIndex[1] = INDEX_NONE;
	SimAccumulator = 0.f;

	Owner = GetOwner();
	if (!Owner)
	{
		NathanDebug(TEXT("Owner NULL"));
		return;
	}

	if (ACharacter* Character = Cast<ACharacter>(Owner))
	{
		OwnerMesh = Character->GetMesh();
	}

	// Sockets absents = avertissement, PAS un abandon : GetWristLocation se replie sur le centre de l'acteur.
	if (!OwnerMesh.IsValid())
	{
		NathanDebug(TEXT("Pas de SkeletalMesh : les toiles partiront du centre de l'acteur"));
	}
	else
	{
		if (!OwnerMesh->DoesSocketExist(HandSocketLeft))
		{
			NathanDebug(TEXT("Socket %s absent : toile gauche depuis le centre de l'acteur"), *HandSocketLeft.ToString());
		}
		if (!OwnerMesh->DoesSocketExist(HandSocketRight))
		{
			NathanDebug(TEXT("Socket %s absent : toile droite depuis le centre de l'acteur"), *HandSocketRight.ToString());
		}
	}

	GetChunkManager();
}

void UKiWebComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Webs.Num() == 0)
	{
		SimAccumulator = 0.f;
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Step = FMath::Max(FixedDt, 0.001f);
	const FVector Gravity(0.0, 0.0, World->GetGravityZ() * WebGravityScale);

	// Pas fixe : la raideur de la toile ne depend pas du framerate.
	// DeltaTime borne : un pic de lag ne devient pas un pas geant (explosion).
	SimAccumulator += FMath::Min(DeltaTime, 0.1f);

	int32 StepsDone = 0;
	while (SimAccumulator >= Step && StepsDone < MaxSimStepsPerFrame)
	{
		for (FKiWeb& Web : Webs)
		{
			StepWeb(Web, Step, Gravity);
		}
		SimAccumulator -= Step;
		++StepsDone;
	}

	// Budget de pas epuise : on jette le retard au lieu de l'accumuler (spirale de la mort).
	if (StepsDone >= MaxSimStepsPerFrame)
	{
		SimAccumulator = 0.f;
	}

	// Toiles qui ne tiennent plus a rien (ni poignet, ni ancre) : supprimees.
	// Parcours a l'envers : RemoveAt decale les elements SUIVANTS, pas ceux deja visites.
	for (int32 i = Webs.Num() - 1; i >= 0; --i)
	{
		if (Webs[i].WebState == EKiWebState::Loose && !Webs[i].IsHeld())
		{
			RemoveWebAt(i);
		}
	}

	//TArray<FKiWebTether, TInlineAllocator<2>> Tethers = GetActiveTethers();

#if ENABLE_DRAW_DEBUG
	if (bDrawTrace)
	{
		DrawWebsDebug();
	}
#endif
}

bool UKiWebComponent::TryGetHandSocketLocation(EKiArm Arm, FVector& OutLocation) const
{
	if (!OwnerMesh.IsValid())
	{
		return false;
	}

	const FName Socket = (Arm == EKiArm::Left) ? HandSocketLeft : HandSocketRight;
	if (!OwnerMesh->DoesSocketExist(Socket))
	{
		return false;
	}

	OutLocation = OwnerMesh->GetSocketLocation(Socket);
	return true;
}

FVector UKiWebComponent::GetWristLocation(EKiArm Arm) const
{
	FVector Hand;
	if (TryGetHandSocketLocation(Arm, Hand))
	{
		return Hand;
	}

	const AActor* OwnerActor = Owner ? Owner : GetOwner();
	return OwnerActor ? OwnerActor->GetActorLocation() : FVector::ZeroVector;
}

AChunckManager* UKiWebComponent::GetChunkManager()
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

bool UKiWebComponent::FireWeb(EKiArm Arm, const FVector& Origin, const FVector& Direction)
{
	if (!Owner)
	{
		Owner = GetOwner();
		if (!Owner)
		{
			return false;
		}
	}

	const FVector Dir = Direction.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		NathanDebug(TEXT("FireWeb : direction nulle"));
		return false;
	}

	// Un bras ne tient qu'une toile : l'ancienne est lachee (elle reste dans le monde si ancree).
	ReleaseWeb(Arm);

	// Plafond : on retire la plus ancienne toile NON tenue.
	if (Webs.Num() >= FMath::Max(3, MaxWebs))
	{
		for (int32 i = 0; i < Webs.Num(); ++i)
		{
			if (!Webs[i].IsHeld())
			{
				RemoveWebAt(i);
				break;
			}
		}
	}

	FKiWeb NewWeb;
	NewWeb.Arm = Arm;
	NewWeb.WebState = EKiWebState::Traveling;
	NewWeb.ParticleRestLength = FMath::Max(5.0, static_cast<double>(ParticleSpacing));
	NewWeb.MaxParticleNumber = FMath::CeilToInt(MaxFireDistance / NewWeb.ParticleRestLength) + 2;

	const FVector Wrist = GetWristLocation(Arm);
	NewWeb.Start.Type = EKiWebBindingType::HandSocket;
	NewWeb.Start.BoneOrSocket = (Arm == EKiArm::Left) ? HandSocketLeft : HandSocketRight;
	NewWeb.Start.Location = Wrist;

	// Cible par defaut : portee max dans la direction visee (toile ratee).
	FVector Target = Origin + Dir * MaxFireDistance;
	NewWeb.End.Type = EKiWebBindingType::None;

	if (AChunckManager* Manager = GetChunkManager())
	{
		FVoxelRayHit Hit;
		Manager->VoxelRaycast(Origin, Dir, MaxFireDistance, Hit);

		// StartedInsideSolid exclu : main dans la roche = pas d'ancre exploitable.
		if (Hit.bHit && Hit.ExitReason == EVoxelRayHitExit::Hit)
		{
			Target = Hit.ImpactPointCm;   // sur la surface, pas au centre du voxel (dans la roche)
			NewWeb.End.Type = EKiWebBindingType::VoxelField;
			NewWeb.End.GlobalVoxelCoord = Hit.VoxelCoord;
			NewWeb.End.SurfaceNormal = Hit.Normal;
			NewWeb.End.SubVoxelOffset = Hit.ImpactPointCm - FVector(Hit.VoxelCoord) * Manager->VoxelSize;
		}

		NathanDebug(TEXT("Toile %d : %s"), static_cast<int32>(Arm), *Hit.ToString());
	}

	NewWeb.End.Location = Target;

	// La pointe vise la cible DEPUIS LE POIGNET : elle arrive exactement sur Target.
	const FVector ToTarget = Target - Wrist;
	const double Distance = ToTarget.Size();
	if (Distance < KINDA_SMALL_NUMBER)
	{
		return false;
	}
	NewWeb.TipVelocity = (ToTarget / Distance) * TipSpeed;

	// Reserve : aucune reallocation pendant le vol.
	NewWeb.WebParticles.Reserve(NewWeb.MaxParticleNumber);

	// Pointe : cinematique (InvMass = 0), les contraintes ne peuvent pas la freiner.
	// x_prev coherent avec sa vitesse : FillWebParticles lit sa vitesse implicite.
	FKiWebParticle Tip;
	Tip.CurrentLocationOfParticle = Wrist;
	Tip.PreviousLocationOfParticle = Wrist - NewWeb.TipVelocity * FixedDt;
	Tip.InvMass = 0.f;
	NewWeb.WebParticles.Add(Tip);

	const int32 NewIndex = Webs.Add(MoveTemp(NewWeb));
	HeldWebIndex[static_cast<int32>(Arm)] = NewIndex;
	return true;
}

void UKiWebComponent::ReleaseWeb(EKiArm Arm)
{
	const int32 ArmIndex = static_cast<int32>(Arm);
	const int32 Index = HeldWebIndex[ArmIndex];
	HeldWebIndex[ArmIndex] = INDEX_NONE;

	if (!Webs.IsValidIndex(Index))
	{
		return;
	}

	// La toile n'est plus au poignet. Ancree : elle pend. En vol : elle finira son trajet.
	Webs[Index].Start.Type = EKiWebBindingType::None;

	// Ni poignet ni ancre : plus rien ne la tient.
	if (Webs[Index].WebState == EKiWebState::Loose)
	{
		RemoveWebAt(Index);
	}
}

void UKiWebComponent::ReleaseAll()
{
	ReleaseWeb(EKiArm::Left);
	ReleaseWeb(EKiArm::Right);
}

void UKiWebComponent::RemoveWebAt(int32 Index)
{
	if (!Webs.IsValidIndex(Index))
	{
		return;
	}

	Webs.RemoveAt(Index);

	// RemoveAt decale tout ce qui suit : les index tenus doivent suivre.
	for (int32& Held : HeldWebIndex)
	{
		if (Held == Index)
		{
			Held = INDEX_NONE;
		}
		else if (Held > Index)
		{
			--Held;
		}
	}
}

void UKiWebComponent::StepWeb(FKiWeb& Web, float Dt, const FVector& Gravity)
{
	if (Web.WebParticles.Num() == 0)
	{
		return;
	}

	// Ordre : pointe -> Verlet -> contraintes -> deroule.
	// Le deroule en DERNIER mesure l'ecart reel apres resolution.
	if (Web.WebState == EKiWebState::Traveling)
	{
		AdvanceTip(Web, Dt);
	}
	IntegrateParticles(Web, Dt, Gravity);
	AChunckManager* Manager = GetChunkManager();
	if (!Manager)
	{
		return;
	}
	{
		FVoxelQueryScope Query(*Manager);
		if (!Query.IsValid() || !DetectContacts(Web, Query))
		{
			//return;   //zone inconnue : toile figee pour ce pas
		}
	}//Verrou relaché
	ReelWebParticles(Web, Dt);
	SolveConstraints(Web, Dt);
	ApplyFriction(Web);
	FillWebParticles(Web, Dt);
}

bool UKiWebComponent::DetectContacts(FKiWeb& Web, FVoxelQueryScope& Query) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(KiWeb_DetectContacts);

	for (FKiWebParticle& P : Web.WebParticles)
	{
		P.bHasContact = false;
		if (P.InvMass <= 0.0f)
		{
			continue;
		}
		FVoxelSweepHit Hit = Query.Sweep(P.PreviousLocationOfParticle, P.CurrentLocationOfParticle);

		if (Hit.bUnknown)
		{
			for (FKiWebParticle& Particle : Web.WebParticles)
			{
				if (Particle.InvMass > 0.f)
				{
					Particle.CurrentLocationOfParticle = Particle.PreviousLocationOfParticle;
				}
				Particle.bHasContact = false;
			}
			return false;
		}

		if (!Hit.bHit)
		{
			continue;
		}

		if (Hit.bStartedInside)
		{
			FVector ExitPoint, ExitNormal;
			if (Query.FindNearestExit(P.PreviousLocationOfParticle, ExitPoint, ExitNormal))
			{
				P.bHasContact = true;
				P.ContactPoint = ExitPoint;
				P.ContactNormal = ExitNormal;
			}
			continue;
		}
		P.bHasContact = true;
		P.ContactPoint = Hit.Point;
		P.ContactNormal = Hit.Normal;
	}
	return true;
}

void UKiWebComponent::SolveContactConstraint(FKiWebParticle& P, double Radius)
{
	if (!P.bHasContact || P.InvMass <= 0)
	{
		return;
	}

	const double Constraint = FVector::DotProduct(P.ContactNormal, P.CurrentLocationOfParticle - P.ContactPoint) - Radius;
	if (Constraint < 0)
	{
		P.CurrentLocationOfParticle -= Constraint * P.ContactNormal;
	}

}


void UKiWebComponent::AdvanceTip(FKiWeb& Web, float Dt)
{
	FKiWebParticle& Tip = Web.WebParticles[0];
	const FVector Target = Web.End.Location;
	const double Remaining = FVector::Dist(Tip.CurrentLocationOfParticle, Target);
	const double StepLength = Web.TipVelocity.Size() * Dt;

	if (Remaining > StepLength)
	{
		Tip.PreviousLocationOfParticle = Tip.CurrentLocationOfParticle;
		Tip.CurrentLocationOfParticle += Web.TipVelocity * Dt;
		return;
	}

	// --- Arrivee ---
	Tip.PreviousLocationOfParticle = Tip.CurrentLocationOfParticle;
	Tip.CurrentLocationOfParticle = Target;

	if (Web.End.Type == EKiWebBindingType::VoxelField)
	{
		// Epinglee sur l'ancre : vitesse nulle, masse infinie.
		Web.WebState = EKiWebState::Attached;
		Tip.InvMass = 0.f;
		Tip.PreviousLocationOfParticle = Target;
	}
	else
	{
		// Ratee : la pointe devient libre et garde son elan (x_prev inchange).
		Web.WebState = EKiWebState::Loose;
		Tip.InvMass = 1.f;
	}

	// La longueur de la toile est figee MAINTENANT : c'est la longueur du balancement.
	if (Web.IsHeld())
	{
		Web.WristRestLength = FVector::Dist(
			Web.WebParticles.Last().CurrentLocationOfParticle, GetWristLocation(Web.Arm));
		Web.TargetLength = (Web.WebParticles.Num() - 1) * Web.ParticleRestLength + Web.WristRestLength;
	}
}

void UKiWebComponent::IntegrateParticles(FKiWeb& Web, float Dt, const FVector& Gravity) const
{
	const FVector GravityStep = Gravity * (static_cast<double>(Dt) * Dt);
	const double DampingFactor = pow(DampingPerSecond, Dt);//FMath::Clamp(static_cast<double>(Damping), 0.0, 1.0);

	for (FKiWebParticle& P : Web.WebParticles)
	{
		if (P.InvMass <= 0.f)
		{
			continue;   // epinglee / cinematique
		}

		// Verlet : x_new = x + (x - x_prev) * damping + a * dt²
		const FVector Velocity = (P.CurrentLocationOfParticle - P.PreviousLocationOfParticle) * DampingFactor;
		P.PreviousLocationOfParticle = P.CurrentLocationOfParticle;
		P.CurrentLocationOfParticle += Velocity + GravityStep;
	}
}

void UKiWebComponent::SolveConstraints(FKiWeb& Web, float Dt) const
{
	TArray<FKiWebParticle>& P = Web.WebParticles;
	const int32 Num = P.Num();
	const double Rest = Web.ParticleRestLength;

	// Pendant le vol, le troncon poignet -> derniere particule est de la toile qui SORT : non contraint.
	const bool bWristConstraint = Web.IsHeld() && Web.WebState != EKiWebState::Traveling;
	const FVector Wrist = bWristConstraint ? GetWristLocation(Web.Arm) : FVector::ZeroVector;

	const double AlphaTilde = static_cast<double>(WebCompliance) / (static_cast<double>(Dt) * Dt);
	Web.SegmentLambdas.Init(0.0, FMath::Max(0, Num - 1));
	const int32 Iterations = FMath::Max(1, SolverIterations);
	for (int32 It = 0; It < Iterations; ++It)
	{
		// Sens alterne : l'information se propage plus vite le long de la corde.
		if ((It & 1) == 0)
		{
			for (int32 i = 0; i < Num - 1; ++i)
			{
				SolveDistanceConstraint(P[i], P[i + 1], Rest, AlphaTilde, Web.SegmentLambdas[i]);
			}
			if (bWristConstraint)
			{
				SolvePinConstraint(P[Num - 1], Wrist, Web.WristRestLength);
			}
			for (FKiWebParticle& Particle : P)
			{
				SolveContactConstraint(Particle, WebRadius);
			}
		}
		else
		{
			if (bWristConstraint)
			{
				SolvePinConstraint(P[Num - 1], Wrist, Web.WristRestLength);
				for (FKiWebParticle& Particle : P)
				{
					SolveContactConstraint(Particle, WebRadius);
				}
			}
			for (int32 i = Num - 2; i >= 0; --i)
			{
				SolveDistanceConstraint(P[i], P[i + 1], Rest, AlphaTilde, Web.SegmentLambdas[i]);
			}
			for (FKiWebParticle& Particle : P)
			{
				SolveContactConstraint(Particle, WebRadius);
			}
		}
	}
}

void UKiWebComponent::ApplyFriction(FKiWeb& Web) const
{
	for (FKiWebParticle& P : Web.WebParticles)
	{
		if (P.bHasContact && P.InvMass > 0)
		{
			FVector Distance = P.CurrentLocationOfParticle - P.PreviousLocationOfParticle;
			FVector Distance_t = Distance - (FVector::DotProduct(Distance, P.ContactNormal) * P.ContactNormal);
			P.CurrentLocationOfParticle -= Distance_t * WebFriction;
		}
	}
}



void UKiWebComponent::SolveDistanceConstraint(FKiWebParticle& A, FKiWebParticle& B, double RestLength, double AlphaTilde, double& Lambda)
{
	const double WSum = static_cast<double>(A.InvMass) + B.InvMass;
	if (WSum + AlphaTilde <= 0.0)
	{
		return;
	}

	const FVector Delta = A.CurrentLocationOfParticle - B.CurrentLocationOfParticle;
	const double Length = Delta.Size();
	if (Length < KINDA_SMALL_NUMBER)
	{
		return;
	}
	const double Error = Length - RestLength;
	const FVector Normal = Delta / Length;
	
	const double DeltaLambda = (-Error - AlphaTilde * Lambda) / (WSum + AlphaTilde);
	Lambda += DeltaLambda;

	A.CurrentLocationOfParticle += Normal * (A.InvMass * DeltaLambda);
	B.CurrentLocationOfParticle -= Normal * (B.InvMass * DeltaLambda);
}

void UKiWebComponent::SolvePinConstraint(FKiWebParticle& P, const FVector& PinLocation, double RestLength)
{
	if (P.InvMass <= 0.f)
	{
		return;
	}

	const FVector Delta = PinLocation - P.CurrentLocationOfParticle;
	const double Length = Delta.Size();

	// UNILATERALE : une toile retient, elle ne pousse pas.
	// Si le joueur se rapproche de l'ancre, la toile devient molle au lieu d'etre repoussee.
	if (Length <= RestLength || Length < KINDA_SMALL_NUMBER)
	{
		return;
	}

	P.CurrentLocationOfParticle += (Delta / Length) * (Length - RestLength);
}

void UKiWebComponent::FillWebParticles(FKiWeb& KiWeb, float Dt)
{
	// Le deroule n'existe que pendant le vol ET tant que le poignet tient la toile.
	if (KiWeb.WebState != EKiWebState::Traveling || !KiWeb.IsHeld())
	{
		return;
	}

	AActor* ShooterActor = GetOwner();
	if (!ShooterActor || Dt <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// La pointe (index 0) est creee au moment du tir. Sans elle, rien a derouler.
	if (KiWeb.WebParticles.Num() == 0)
	{
		return;
	}

	const double Spacing = KiWeb.ParticleRestLength;
	const FVector WristPosition = GetWristLocation(KiWeb.Arm);
	const FVector WristVelocity = ShooterActor->GetVelocity();

	// COPIE volontaire, pas une reference : Add() peut reallouer le tableau
	// et une reference vers Last() deviendrait invalide pendant la boucle.
	const FKiWebParticle LastParticle = KiWeb.WebParticles.Last();
	const FVector LastPosition = LastParticle.CurrentLocationOfParticle;

	// Vitesse implicite de Verlet : (x - x_prev) / dt
	const FVector LastVelocity =
		(LastParticle.CurrentLocationOfParticle - LastParticle.PreviousLocationOfParticle) / Dt;

	const FVector ToWrist = WristPosition - LastPosition;
	const double InitialGap = ToWrist.Size();
	if (InitialGap <= Spacing)
	{
		return;   // pas encore assez de toile sortie pour une nouvelle particule
	}

	// Toutes les nouvelles particules sont sur le segment droit Derniere -> Poignet.
	const FVector Direction = ToWrist / InitialGap;

	double Gap = InitialGap;
	int32 EmittedCount = 0;

	// WHILE et pas IF : a 60 m/s et 120 Hz, ~1,7 particule doit sortir par pas.
	while (Gap > Spacing && KiWeb.WebParticles.Num() < KiWeb.MaxParticleNumber)
	{
		++EmittedCount;

		// Placee a k * L0 de la derniere particule, JAMAIS sur le poignet.
		// Calcul depuis LastPosition (et non en chainant) : pas d'accumulation d'erreur.
		const double DistanceFromLast = EmittedCount * Spacing;
		const FVector NewPosition = LastPosition + Direction * DistanceFromLast;

		// Vitesse du flux de toile : une particule nee immobile freinerait toute la corde.
		const double T = DistanceFromLast / InitialGap;
		const FVector NewVelocity = FMath::Lerp(LastVelocity, WristVelocity, T);

		FKiWebParticle NewParticle;
		NewParticle.CurrentLocationOfParticle = NewPosition;
		NewParticle.PreviousLocationOfParticle = NewPosition - NewVelocity * Dt;
		NewParticle.InvMass = 1.0f;

		KiWeb.WebParticles.Add(NewParticle);

		Gap -= Spacing;   // la condition du while progresse a chaque tour
	}
}

void UKiWebComponent::ReelWebParticles(FKiWeb& KiWeb, float Dt)
{
	if (!KiWeb.IsAttached() || !KiWeb.IsHeld())
	{
		return;
	}

	AActor* ShooterActor = GetOwner();
	if (!ShooterActor || Dt <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	//La pointe (index 0) est creee au moment du tir. Sans elle, rien a derouler.
	if (KiWeb.WebParticles.Num() == 0)
	{
		return;
	}

	const double L0 = KiWeb.ParticleRestLength;

	//Rapprocher la longueur de la cible, d'au plus ReelSpeed * Dt ---
	const double CurrentLength = (KiWeb.WebParticles.Num() - 1) * L0 + KiWeb.WristRestLength;
	const double Gap = KiWeb.TargetLength - CurrentLength;      // + = derouler, - = rembobiner
	const double MaxStep = static_cast<double>(ReelSpeed) * Dt;
	KiWeb.WristRestLength += FMath::Clamp(Gap, -MaxStep, MaxStep);

	//Derouler : le dernier troncon depasse un segment -> on insere une particule ---
	const FVector WristPosition = GetWristLocation(KiWeb.Arm);
	const FVector WristVelocity = ShooterActor->GetVelocity();

	while (KiWeb.WristRestLength > L0 && KiWeb.WebParticles.Num() < KiWeb.MaxParticleNumber)
	{
		const FVector LastPosition = KiWeb.WebParticles.Last().CurrentLocationOfParticle;

		const FVector ToWrist = WristPosition - LastPosition;
		const double DistToWrist = ToWrist.Size();

		// Poignet colle a la derniere particule : direction de secours vers le bas.
		// Deux particules confondues ne se separeraient jamais (SolveDistanceConstraint les ignore).
		const FVector Direction = (DistToWrist > 1.0) ? ToWrist / DistToWrist : FVector::DownVector;

		//A L0 de la derniere, sans depasser le poignet.
		const FVector NewPosition = LastPosition + Direction * FMath::Min(L0, FMath::Max(DistToWrist, 1.0));

		FKiWebParticle NewParticle;
		NewParticle.CurrentLocationOfParticle = NewPosition;
		NewParticle.PreviousLocationOfParticle = NewPosition - WristVelocity * Dt;   // part a la vitesse du poignet
		NewParticle.InvMass = 1.0f;
		KiWeb.WebParticles.Add(NewParticle);

		KiWeb.WristRestLength -= L0;
	}

	//Rembobiner : le dernier troncon est negatif -> on retire la derniere particule ---
	// Jamais l'index 0 : c'est l'ancre.
	while (KiWeb.WristRestLength < 0.0 && KiWeb.WebParticles.Num() > 1)
	{
		KiWeb.WebParticles.Pop(EAllowShrinking::No);   // pas de reallocation : on re-deroulera surement
		KiWeb.WristRestLength += L0;
	}

	//Il ne reste que la pointe : le troncon ne peut pas etre negatif.
	KiWeb.WristRestLength = FMath::Max(KiWeb.WristRestLength, 0.0);

}

void UKiWebComponent::AdjustReelTarget(EKiArm Arm, const float Notches)
{
	const int32 WebIndex = HeldWebIndex[static_cast<int32>(Arm)];
	if (!Webs.IsValidIndex(WebIndex))
	{
		return;
	}

	FKiWeb& Web = Webs[WebIndex];
	if (!Web.IsAttached())
	{
		return;
	}

	// Bornes appliquees ICI : le cran ne peut pas demander l'impossible.
	const double MaxLength = Web.MaxParticleNumber * Web.ParticleRestLength;
	Web.TargetLength = FMath::Clamp(Web.TargetLength + Notches * ReelStepPerNotch, Web.MinLength, MaxLength);
}

TArray<FKiWebTether, TInlineAllocator<2>> UKiWebComponent::GetActiveTethers()
{
	TArray<FKiWebTether, TInlineAllocator<2>> Tethers;
	for (int32 ArmIndex = 0; ArmIndex < 2; ArmIndex++)
	{
		const int32 WebIndex = HeldWebIndex[ArmIndex];
		if (!Webs.IsValidIndex(WebIndex))
		{
			continue;
		}
		const FKiWeb& Web = Webs[WebIndex];
		if (!Web.IsAttached() || Web.WebParticles.Num() == 0)
		{
			continue;
		}
		FKiWebTether WebTether;
		WebTether.Arm = static_cast<EKiArm>(ArmIndex);
		WebTether.Pivot = Web.End.Location;
		WebTether.MaxDistance = (Web.WebParticles.Num() - 1) * Web.ParticleRestLength + Web.WristRestLength;
		WebTether.WristOffset = GetWristLocation(WebTether.Arm) - GetOwner()->GetActorLocation();
		Tethers.Add(WebTether);
	}
	return Tethers;
}

void UKiWebComponent::DrawWebsDebug() const
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (const FKiWeb& Web : Webs)
	{
		const TArray<FKiWebParticle>& P = Web.WebParticles;
		const int32 Num = P.Num();
		if (Num == 0)
		{
			continue;
		}

		FColor Color = FColor::Red;
		switch (Web.WebState)
		{
		case EKiWebState::Traveling: Color = FColor::Yellow; break;
		case EKiWebState::Attached:  Color = FColor::Cyan;   break;
		case EKiWebState::Loose:     Color = FColor::Orange; break;
		default: break;
		}

		double CurrentLength = 0.0;
		for (int32 i = 0; i < Num - 1; ++i)
		{
			DrawDebugLine(World, P[i].CurrentLocationOfParticle, P[i + 1].CurrentLocationOfParticle,
				Color, false, -1.f, 0, 2.f);
			CurrentLength += FVector::Dist(P[i].CurrentLocationOfParticle, P[i + 1].CurrentLocationOfParticle);
		}

		if (Web.IsHeld())
		{
			const FVector Wrist = GetWristLocation(Web.Arm);
			DrawDebugLine(World, P[Num - 1].CurrentLocationOfParticle, Wrist, Color, false, -1.f, 0, 2.f);
			CurrentLength += FVector::Dist(P[Num - 1].CurrentLocationOfParticle, Wrist);
		}

		DrawDebugPoint(World, P[0].CurrentLocationOfParticle, 10.f, FColor::Red, false, -1.f);

		// Mesure d'etirement : longueur actuelle / longueur au repos (toiles tenues, apres le vol).
		if (GEngine && Web.IsHeld() && Web.WebState != EKiWebState::Traveling)
		{
			const double RestTotal = (Num - 1) * Web.ParticleRestLength + Web.WristRestLength;
			if (RestTotal > KINDA_SMALL_NUMBER)
			{
				GEngine->AddOnScreenDebugMessage(-1, 0.f, Color,
					FString::Printf(TEXT("Toile %s : %d particules | etirement x%.2f"),
						Web.Arm == EKiArm::Left ? TEXT("G") : TEXT("D"), Num, CurrentLength / RestTotal));
			}
		}
	}
#endif
}