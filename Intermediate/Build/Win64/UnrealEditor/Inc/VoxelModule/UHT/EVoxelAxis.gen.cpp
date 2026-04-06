// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "EVoxelAxis.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodeEVoxelAxis() {}

// ********** Begin Cross Module References ********************************************************
UPackage* Z_Construct_UPackage__Script_VoxelModule();
VOXELMODULE_API UEnum* Z_Construct_UEnum_VoxelModule_EVoxelAxis();
// ********** End Cross Module References **********************************************************

// ********** Begin Enum EVoxelAxis ****************************************************************
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EVoxelAxis;
static UEnum* EVoxelAxis_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EVoxelAxis.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EVoxelAxis.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_VoxelModule_EVoxelAxis, (UObject*)Z_Construct_UPackage__Script_VoxelModule(), TEXT("EVoxelAxis"));
	}
	return Z_Registration_Info_UEnum_EVoxelAxis.OuterSingleton;
}
template<> VOXELMODULE_NON_ATTRIBUTED_API UEnum* StaticEnum<EVoxelAxis>()
{
	return EVoxelAxis_StaticEnum();
}
struct Z_Construct_UEnum_VoxelModule_EVoxelAxis_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "ModuleRelativePath", "Public/EVoxelAxis.h" },
		{ "X.Name", "EVoxelAxis::X" },
		{ "Y.Name", "EVoxelAxis::Y" },
		{ "Z.Name", "EVoxelAxis::Z" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EVoxelAxis::X", (int64)EVoxelAxis::X },
		{ "EVoxelAxis::Y", (int64)EVoxelAxis::Y },
		{ "EVoxelAxis::Z", (int64)EVoxelAxis::Z },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct Z_Construct_UEnum_VoxelModule_EVoxelAxis_Statics 
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_VoxelModule_EVoxelAxis_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_VoxelModule,
	nullptr,
	"EVoxelAxis",
	"EVoxelAxis",
	Z_Construct_UEnum_VoxelModule_EVoxelAxis_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_VoxelModule_EVoxelAxis_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_VoxelModule_EVoxelAxis_Statics::Enum_MetaDataParams), Z_Construct_UEnum_VoxelModule_EVoxelAxis_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_VoxelModule_EVoxelAxis()
{
	if (!Z_Registration_Info_UEnum_EVoxelAxis.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EVoxelAxis.InnerSingleton, Z_Construct_UEnum_VoxelModule_EVoxelAxis_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EVoxelAxis.InnerSingleton;
}
// ********** End Enum EVoxelAxis ******************************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_EVoxelAxis_h__Script_VoxelModule_Statics
{
	static constexpr FEnumRegisterCompiledInInfo EnumInfo[] = {
		{ EVoxelAxis_StaticEnum, TEXT("EVoxelAxis"), &Z_Registration_Info_UEnum_EVoxelAxis, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 2055141578U) },
	};
}; // Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_EVoxelAxis_h__Script_VoxelModule_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_EVoxelAxis_h__Script_VoxelModule_3384158548{
	TEXT("/Script/VoxelModule"),
	nullptr, 0,
	nullptr, 0,
	Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_EVoxelAxis_h__Script_VoxelModule_Statics::EnumInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_EVoxelAxis_h__Script_VoxelModule_Statics::EnumInfo),
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
