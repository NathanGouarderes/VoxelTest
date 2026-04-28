// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeVoxelTest_init() {}
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");	static FPackageRegistrationInfo Z_Registration_Info_UPackage__Script_VoxelTest;
	FORCENOINLINE UPackage* Z_Construct_UPackage__Script_VoxelTest()
	{
		if (!Z_Registration_Info_UPackage__Script_VoxelTest.OuterSingleton)
		{
		static const UECodeGen_Private::FPackageParams PackageParams = {
			"/Script/VoxelTest",
			nullptr,
			0,
			PKG_CompiledIn | 0x00000000,
			0x9ABD84E4,
			0xCAF45B25,
			METADATA_PARAMS(0, nullptr)
		};
		UECodeGen_Private::ConstructUPackage(Z_Registration_Info_UPackage__Script_VoxelTest.OuterSingleton, PackageParams);
	}
	return Z_Registration_Info_UPackage__Script_VoxelTest.OuterSingleton;
}
static FRegisterCompiledInInfo Z_CompiledInDeferPackage_UPackage__Script_VoxelTest(Z_Construct_UPackage__Script_VoxelTest, TEXT("/Script/VoxelTest"), Z_Registration_Info_UPackage__Script_VoxelTest, CONSTRUCT_RELOAD_VERSION_INFO(FPackageReloadVersionInfo, 0x9ABD84E4, 0xCAF45B25));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
