// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Structs/FQuad.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodeFQuad() {}

// ********** Begin Cross Module References ********************************************************
UPackage* Z_Construct_UPackage__Script_VoxelModule();
VOXELMODULE_API UScriptStruct* Z_Construct_UScriptStruct_FQuad();
// ********** End Cross Module References **********************************************************

// ********** Begin ScriptStruct FQuad *************************************************************
struct Z_Construct_UScriptStruct_FQuad_Statics
{
	static inline consteval int32 GetStructSize() { return sizeof(FQuad); }
	static inline consteval int16 GetStructAlignment() { return alignof(FQuad); }
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Struct_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "ModuleRelativePath", "Public/Structs/FQuad.h" },
	};
#endif // WITH_METADATA

// ********** Begin ScriptStruct FQuad constinit property declarations *****************************
// ********** End ScriptStruct FQuad constinit property declarations *******************************
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FQuad>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
}; // struct Z_Construct_UScriptStruct_FQuad_Statics
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_FQuad;
class UScriptStruct* FQuad::StaticStruct()
{
	if (!Z_Registration_Info_UScriptStruct_FQuad.OuterSingleton)
	{
		Z_Registration_Info_UScriptStruct_FQuad.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FQuad, (UObject*)Z_Construct_UPackage__Script_VoxelModule(), TEXT("Quad"));
	}
	return Z_Registration_Info_UScriptStruct_FQuad.OuterSingleton;
	}
const UECodeGen_Private::FStructParams Z_Construct_UScriptStruct_FQuad_Statics::StructParams = {
	(UObject* (*)())Z_Construct_UPackage__Script_VoxelModule,
	nullptr,
	&NewStructOps,
	"Quad",
	nullptr,
	0,
	sizeof(FQuad),
	alignof(FQuad),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000001),
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FQuad_Statics::Struct_MetaDataParams), Z_Construct_UScriptStruct_FQuad_Statics::Struct_MetaDataParams)
};
UScriptStruct* Z_Construct_UScriptStruct_FQuad()
{
	if (!Z_Registration_Info_UScriptStruct_FQuad.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_FQuad.InnerSingleton, Z_Construct_UScriptStruct_FQuad_Statics::StructParams);
	}
	return CastChecked<UScriptStruct>(Z_Registration_Info_UScriptStruct_FQuad.InnerSingleton);
}
// ********** End ScriptStruct FQuad ***************************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_Structs_FQuad_h__Script_VoxelModule_Statics
{
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ FQuad::StaticStruct, Z_Construct_UScriptStruct_FQuad_Statics::NewStructOps, TEXT("Quad"),&Z_Registration_Info_UScriptStruct_FQuad, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FQuad), 1897128591U) },
	};
}; // Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_Structs_FQuad_h__Script_VoxelModule_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_Structs_FQuad_h__Script_VoxelModule_1612215611{
	TEXT("/Script/VoxelModule"),
	nullptr, 0,
	Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_Structs_FQuad_h__Script_VoxelModule_Statics::ScriptStructInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_Structs_FQuad_h__Script_VoxelModule_Statics::ScriptStructInfo),
	nullptr, 0,
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
