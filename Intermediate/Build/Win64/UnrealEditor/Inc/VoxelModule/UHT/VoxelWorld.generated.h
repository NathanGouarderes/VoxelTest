// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "VoxelWorld.h"

#ifdef VOXELMODULE_VoxelWorld_generated_h
#error "VoxelWorld.generated.h already included, missing '#pragma once' in VoxelWorld.h"
#endif
#define VOXELMODULE_VoxelWorld_generated_h

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// ********** Begin Class AVoxelWorld **************************************************************
struct Z_Construct_UClass_AVoxelWorld_Statics;
VOXELMODULE_API UClass* Z_Construct_UClass_AVoxelWorld_NoRegister();

#define FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_VoxelWorld_h_16_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesAVoxelWorld(); \
	friend struct ::Z_Construct_UClass_AVoxelWorld_Statics; \
	static UClass* GetPrivateStaticClass(); \
	friend VOXELMODULE_API UClass* ::Z_Construct_UClass_AVoxelWorld_NoRegister(); \
public: \
	DECLARE_CLASS2(AVoxelWorld, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/VoxelModule"), Z_Construct_UClass_AVoxelWorld_NoRegister) \
	DECLARE_SERIALIZER(AVoxelWorld)


#define FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_VoxelWorld_h_16_ENHANCED_CONSTRUCTORS \
	/** Deleted move- and copy-constructors, should never be used */ \
	AVoxelWorld(AVoxelWorld&&) = delete; \
	AVoxelWorld(const AVoxelWorld&) = delete; \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, AVoxelWorld); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(AVoxelWorld); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(AVoxelWorld) \
	NO_API virtual ~AVoxelWorld();


#define FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_VoxelWorld_h_13_PROLOG
#define FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_VoxelWorld_h_16_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_VoxelWorld_h_16_INCLASS_NO_PURE_DECLS \
	FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_VoxelWorld_h_16_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


class AVoxelWorld;

// ********** End Class AVoxelWorld ****************************************************************

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_VoxelWorld_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
