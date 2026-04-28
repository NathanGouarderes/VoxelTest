// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "Components/PlayerControllerComponent.h"

#ifdef VOXELTEST_PlayerControllerComponent_generated_h
#error "PlayerControllerComponent.generated.h already included, missing '#pragma once' in PlayerControllerComponent.h"
#endif
#define VOXELTEST_PlayerControllerComponent_generated_h

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// ********** Begin Class APlayerControllerComponent ***********************************************
struct Z_Construct_UClass_APlayerControllerComponent_Statics;
VOXELTEST_API UClass* Z_Construct_UClass_APlayerControllerComponent_NoRegister();

#define FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelTest_Components_PlayerControllerComponent_h_15_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesAPlayerControllerComponent(); \
	friend struct ::Z_Construct_UClass_APlayerControllerComponent_Statics; \
	static UClass* GetPrivateStaticClass(); \
	friend VOXELTEST_API UClass* ::Z_Construct_UClass_APlayerControllerComponent_NoRegister(); \
public: \
	DECLARE_CLASS2(APlayerControllerComponent, APlayerController, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/VoxelTest"), Z_Construct_UClass_APlayerControllerComponent_NoRegister) \
	DECLARE_SERIALIZER(APlayerControllerComponent)


#define FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelTest_Components_PlayerControllerComponent_h_15_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API APlayerControllerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	/** Deleted move- and copy-constructors, should never be used */ \
	APlayerControllerComponent(APlayerControllerComponent&&) = delete; \
	APlayerControllerComponent(const APlayerControllerComponent&) = delete; \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, APlayerControllerComponent); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(APlayerControllerComponent); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(APlayerControllerComponent) \
	NO_API virtual ~APlayerControllerComponent();


#define FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelTest_Components_PlayerControllerComponent_h_12_PROLOG
#define FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelTest_Components_PlayerControllerComponent_h_15_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelTest_Components_PlayerControllerComponent_h_15_INCLASS_NO_PURE_DECLS \
	FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelTest_Components_PlayerControllerComponent_h_15_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


class APlayerControllerComponent;

// ********** End Class APlayerControllerComponent *************************************************

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelTest_Components_PlayerControllerComponent_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
