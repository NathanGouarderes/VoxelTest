// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "EVoxelAxis.h"

#ifdef VOXELMODULE_EVoxelAxis_generated_h
#error "EVoxelAxis.generated.h already included, missing '#pragma once' in EVoxelAxis.h"
#endif
#define VOXELMODULE_EVoxelAxis_generated_h

#include "Templates/IsUEnumClass.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "Templates/NoDestroy.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_EVoxelAxis_h

// ********** Begin Enum EVoxelAxis ****************************************************************
#define FOREACH_ENUM_EVOXELAXIS(op) \
	op(EVoxelAxis::X) \
	op(EVoxelAxis::Y) \
	op(EVoxelAxis::Z) 

enum class EVoxelAxis : uint8;
template<> struct TIsUEnumClass<EVoxelAxis> { enum { Value = true }; };
template<> VOXELMODULE_NON_ATTRIBUTED_API UEnum* StaticEnum<EVoxelAxis>();
// ********** End Enum EVoxelAxis ******************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
