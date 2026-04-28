// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeVoxelModule_init() {}
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");	static FPackageRegistrationInfo Z_Registration_Info_UPackage__Script_VoxelModule;
	FORCENOINLINE UPackage* Z_Construct_UPackage__Script_VoxelModule()
	{
		if (!Z_Registration_Info_UPackage__Script_VoxelModule.OuterSingleton)
		{
		static const UECodeGen_Private::FPackageParams PackageParams = {
			"/Script/VoxelModule",
			nullptr,
			0,
			PKG_CompiledIn | 0x00000000,
			0x482DACB8,
			0xF3CE26FF,
			METADATA_PARAMS(0, nullptr)
		};
		UECodeGen_Private::ConstructUPackage(Z_Registration_Info_UPackage__Script_VoxelModule.OuterSingleton, PackageParams);
	}
	return Z_Registration_Info_UPackage__Script_VoxelModule.OuterSingleton;
}
static FRegisterCompiledInInfo Z_CompiledInDeferPackage_UPackage__Script_VoxelModule(Z_Construct_UPackage__Script_VoxelModule, TEXT("/Script/VoxelModule"), Z_Registration_Info_UPackage__Script_VoxelModule, CONSTRUCT_RELOAD_VERSION_INFO(FPackageReloadVersionInfo, 0x482DACB8, 0xF3CE26FF));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
