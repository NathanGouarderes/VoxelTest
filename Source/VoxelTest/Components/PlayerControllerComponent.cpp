// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerControllerComponent.h"
#include "VoxelChunck.h" 

void APlayerControllerComponent::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindAction("RemoveVoxel", IE_Pressed, this, &APlayerControllerComponent::OnDestroyInput);
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
}

void APlayerControllerComponent::OnDestroyInput()
{
/*
    float MouseX, MouseY;
    GetMousePosition(MouseX, MouseY);

    UE_LOG(LogTemp, Warning, TEXT("Mouse: %f %f"), MouseX, MouseY);
*/
    // Récupérer la position de la souris dans le monde

    FVector Start;
    FRotator Rotation;

    GetPlayerViewPoint(Start, Rotation);

    FVector End = Start + Rotation.Vector() * 5000.0f;

    FHitResult Hit;
    GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility);

    if (Hit.bBlockingHit)
    {
        AVoxelChunck* Chunck = Cast<AVoxelChunck>(Hit.GetActor());
        if (Chunck)
        {
            FVector HitLocation = Hit.ImpactPoint - Hit.ImpactNormal * 1.0f;

            FVector LocalPos = Chunck->GetActorTransform().InverseTransformPosition(HitLocation);
            float VoxelSize = Chunck->GetVoxelSize();
            int X = FMath::FloorToInt(LocalPos.X / VoxelSize);
            int Y = FMath::FloorToInt(LocalPos.Y / VoxelSize);
            int Z = FMath::FloorToInt(LocalPos.Z / VoxelSize);

            Chunck->RemoveVoxel(X, Y, Z);
        }
    }
}
