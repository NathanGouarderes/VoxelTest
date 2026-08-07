#pragma once
#include "CoreMinimal.h"
#include "ChunckManager.h"
#include "FKiWeb.generated.h"


UENUM(BlueprintType)
enum class EKiBindingType : uint8
{
    None,
    VoxelField,      // ancre dans le champ voxel — survit aux transitions LOD
    SceneComponent,  // os de squelette, plateforme mobile, dragon
    PhysicsBody,     // corps simulé (arbre déraciné, morceau de façade)
    HandSocket       // main du porteur
};

UENUM(BlueprintType)
enum class EKiArm : uint8{Left, Right};

UENUM(BlueprintType)
enum class EKiWebState : uint8
{
    Inactive,
    Traveling,
    Attached,
    Brocke,
};

struct FKiResolveContext
{
    class AChunkManeger* ChunckManager = nullptr;
    double VoxelSize = 100.0;
    uint32 FieldSerial = 0;
};

USTRUCT()
struct FKiEndpoint
{
    GENERATED_BODY()

    UPROPERTY()
    EKiBindingType Type = EKiBindingType::None;
    FIntVector GlobalVoxelCoord = FIntVector::ZeroValue;
    FVector    SubVoxelOffset = FVector::ZeroVector;
    FVector    SurfaceNormal = FVector::UpVector;
    UPROPERTY() TWeakObjectPtr<USceneComponent> Component;
    UPROPERTY() FName BoneOrSocket = NAME_None;
    FVector LocalOffset = FVector::ZeroVector;

    uint32 ValiditySerial = 0;

    /** Le SEUL accès du solveur. Il ignore totalement ce qu'il y a derrière. */
    bool ResolveWorld(const FKiResolveContext& Ctx, FVector& OutWorld) const;

    /** Appelé périodiquement (pas chaque frame). */
    bool IsStillValid(const FKiResolveContext& Ctx) const;

};

USTRUCT()
struct FKiWeb
{
    GENERATED_BODY()

    UPROPERTY()
    FKiEndpoint Start;
    FKiEndpoint End;

    // --- état solveur : garde ce bloc POD et contigu ---

    double RestLength = 0;
    double MinLength = 120;
    FVector LastTensionDir = FVector::ZeroVector;
    double LastTensionMag = 0;


    // --- état gameplay ---
    EKiArm Arm = EKiArm::Left;

    EKiWebState WebState = EKiWebState::Inactive;
    double TravelAlpha = 0;
        
    FORCEINLINE bool IsAttached() const { return WebState == EKiWebState::Attached; }
    FORCEINLINE bool ConstrainsPawn() const
    {
        return IsAttached() && Start.Type == EKiBindingType::HandSocket;
    };
};