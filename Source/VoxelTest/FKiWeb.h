#pragma once
#include "CoreMinimal.h"
#include "ChunckManager.h"
#include "FKiWeb.generated.h"


UENUM(BlueprintType)
enum class EKiWebBindingType : uint8
{
    None,
    VoxelField,      // ancre dans le champ voxel — survit aux transitions LOD
    SceneComponent,  // os de squelette, plateforme mobile, dragon
    PhysicsBody,     // corps simulé (arbre déraciné, morceau de façade)
    HandSocket       // main du porteur
};

UENUM(BlueprintType)
enum class EKiArm : uint8 { Left, Right };

UENUM(BlueprintType)
enum class EKiWebState : uint8
{
    Inactive,
    Traveling,   // la pointe vole, la toile se deroule depuis le poignet
    Attached,    // la pointe est epinglee sur son ancre
    Brocke,
    Loose        // aucune ancre touchee : la pointe est libre et retombe
};

struct FKiResolveContext
{
    AChunckManager* ChunckManager = nullptr;
    double VoxelSize = 10.0;
    uint32 FieldSerial = 0;
};

USTRUCT()
struct FKiEndpoint
{
    GENERATED_BODY()

    UPROPERTY()
    EKiWebBindingType Type = EKiWebBindingType::None;
    FIntVector GlobalVoxelCoord = FIntVector::ZeroValue;
    FVector    SubVoxelOffset = FVector::ZeroVector;
    FVector    SurfaceNormal = FVector::UpVector;
    FVector Location = FVector::ZeroVector;
    UPROPERTY() TWeakObjectPtr<USceneComponent> Component;
    UPROPERTY() FName BoneOrSocket = NAME_None;
    FVector LocalOffset = FVector::ZeroVector;

    uint32 ValiditySerial = 0;

    /** Le SEUL accès du solveur. Il ignore totalement ce qu'il y a derrière. */
    bool ResolveWorld(const FKiResolveContext& Ctx, FVector& OutWorld) const;

    bool IsStillValid(const FKiResolveContext& Ctx) const;
};

struct FKiWebParticle
{
    FVector PreviousLocationOfParticle = FVector::ZeroVector;
    FVector CurrentLocationOfParticle = FVector::ZeroVector;
    float InvMass = 1.0f;   //0 = epinglee / cinematique (rien ne la deplace)

    bool bHasContact = false;
    FVector ContactPoint = FVector::ZeroVector;
    FVector ContactNormal = FVector::UpVector;

};

struct FKiWebTether
{
    FVector Pivot = FVector::ZeroVector;
    FVector WristOffset = FVector::ZeroVector;
    float MaxDistance = 600000.0f;
    EKiArm Arm;
};

USTRUCT()
struct FKiWeb
{
    GENERATED_BODY()

    UPROPERTY() FKiEndpoint Start;   // cote poignet
    UPROPERTY() FKiEndpoint End;     // cote ancre (Location = cible de la pointe)

    double RestLength = 0;
    double MinLength = 120;
    FVector LastTensionDir = FVector::ZeroVector;
    double LastTensionMag = 0;

    //L0 capture au tir : modifier ParticleSpacing en jeu ne deforme pas les toiles existantes.
    double ParticleRestLength = 30.0;

    //Longueur figee du dernier troncon (derniere particule -> poignet), fixee a la fin du vol.
    double WristRestLength = 0.0;
    
    //Longueur cible lorsque je déroule ou rembobine la toile
    double TargetLength = 0.0;

    //Vitesse de la pointe pendant le vol (cm/s).
    FVector TipVelocity = FVector::ZeroVector;

    int32 MaxParticleNumber = 500;
    TArray<FKiWebParticle> WebParticles;   // index 0 = pointe, dernier = cote poignet

    EKiArm Arm = EKiArm::Left;
    EKiWebState WebState = EKiWebState::Inactive;
    double TravelAlpha = 0;
    TArray<double> SegmentLambdas;

    FORCEINLINE bool IsAttached() const { return WebState == EKiWebState::Attached; }
    FORCEINLINE bool IsHeld() const { return Start.Type == EKiWebBindingType::HandSocket; }
    FORCEINLINE bool ConstrainsPawn() const { return IsAttached() && IsHeld(); }
};