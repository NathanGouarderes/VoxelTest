// Fill out your copyright notice in the Description page of Project Settings.


#include "UStrikeWindowNotify.h"



void UStrikeWindowNotify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	UMeleeComponent* MeleeComponent = MeshComp->GetOwner()->FindComponentByClass<UMeleeComponent>();
	const FAnimNotifyEvent* Event = EventReference.GetNotify();
	const float Start = Event ? Event->GetTriggerTime() : 0.f;
	const float End = Event ? Event->GetEndTriggerTime() : TotalDuration;
	if (UMeleeComponent* Melee = FindMelee(MeshComp))
	{
		
		Melee->BeginStrike(Cast<UAnimMontage>(Animation), SocketName, Start, End);

	}

}

void UStrikeWindowNotify::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
}
void UStrikeWindowNotify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (UMeleeComponent* Melee = FindMelee(MeshComp))
	{
		Melee->EndStrike();
	}
}

UMeleeComponent* UStrikeWindowNotify::FindMelee(USkeletalMeshComponent* MeshComp)
{
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	return Owner ? Owner->FindComponentByClass<UMeleeComponent>() : nullptr;
}