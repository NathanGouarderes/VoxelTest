// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/UVoxelBodyComponent.h"
#include "DebugMacro.h"
#include "Components/InstancedStaticMeshComponent.h"


// Sets default values for this component's properties
UVoxelBodyComponent::UVoxelBodyComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}


// Called when the game starts
void UVoxelBodyComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!Body)
	{
		UE_LOG(LogTemp, Error, TEXT("UVoxelBodyComponent::BeginPlay() --> !Body"));
		return;
	}
	if (!MeshCube)
	{
		UE_LOG(LogTemp, Error, TEXT("UVoxelBodyComponent::BeginPlay() --> !MeshCube"));
		return;
	}
	AActor* Owner = GetOwner();
	TArray<USkeletalMeshComponent*> MeshComponents;
	Owner->GetComponents<USkeletalMeshComponent>(MeshComponents);
	UVoxelBodyAsset* VoxelBodyAsset = Body.Get();
	SlotToBoneIndex.Init(static_cast<int16>(INDEX_NONE), VoxelBodyAsset->BoneNames.Num());
	
	USkeletalMeshComponent* KeptComponent = nullptr;
	for (USkeletalMeshComponent* SkeletalMeshComponent : MeshComponents)
	{
		USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMeshAsset();
		if (!SkeletalMesh)
		{
			continue;
		}
		USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
		if (Skeleton == Body->Skeleton)
		{
			KeptComponent = SkeletalMeshComponent;
			break;
		}
	}
	if (!KeptComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("UVoxelBodyComponent::BeginPlay() --> !KeptComponent"));
		return;
	}
	for (int32 Slot = 0; Slot < VoxelBodyAsset->BoneNames.Num(); Slot++)
	{
		const FName BoneName = VoxelBodyAsset->BoneNames[Slot];
		const int32 BoneIndex = KeptComponent->GetBoneIndex(BoneName);

		SlotToBoneIndex[Slot] = BoneIndex;
		if (BoneIndex == INDEX_NONE)
		{
			NathanDebug(TEXT("warning VoxelBody : l'os '%s' (slot %d) n'existe pas dans ce mesh"),
				*BoneName.ToString(), Slot);
		}
		else
		{
			UInstancedStaticMeshComponent* MeshCubeComponent;
			MeshCubeComponent = NewObject<UInstancedStaticMeshComponent>(Owner, FName(TEXT("VoxelISM_") + BoneName.ToString()));
			MeshCubeComponent->SetStaticMesh(MeshCube);
			MeshCubeComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			MeshCubeComponent->SetMobility(EComponentMobility::Movable);
			MeshCubeComponent->SetupAttachment(KeptComponent, BoneName);
			MeshCubeComponent->RegisterComponent();
			MeshCubeComponents.Init(nullptr, VoxelBodyAsset->BoneNames.Num());

			MeshCubeComponents[Slot] = MeshCubeComponent;
			NathanDebug(TEXT("VoxelBody : slot %2d | %-20s -> os %d"),
				Slot, *BoneName.ToString(), BoneIndex);
		}
	}
}

