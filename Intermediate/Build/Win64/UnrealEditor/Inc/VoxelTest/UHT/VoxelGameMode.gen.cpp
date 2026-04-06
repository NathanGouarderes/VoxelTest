// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "VoxelTest/VoxelGameMode.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodeVoxelGameMode() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_AGameModeBase();
UPackage* Z_Construct_UPackage__Script_VoxelTest();
VOXELTEST_API UClass* Z_Construct_UClass_AVoxelGameMode();
VOXELTEST_API UClass* Z_Construct_UClass_AVoxelGameMode_NoRegister();
// ********** End Cross Module References **********************************************************

// ********** Begin Class AVoxelGameMode ***********************************************************
FClassRegistrationInfo Z_Registration_Info_UClass_AVoxelGameMode;
UClass* AVoxelGameMode::GetPrivateStaticClass()
{
	using TClass = AVoxelGameMode;
	if (!Z_Registration_Info_UClass_AVoxelGameMode.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			TClass::StaticPackage(),
			TEXT("VoxelGameMode"),
			Z_Registration_Info_UClass_AVoxelGameMode.InnerSingleton,
			StaticRegisterNativesAVoxelGameMode,
			sizeof(TClass),
			alignof(TClass),
			TClass::StaticClassFlags,
			TClass::StaticClassCastFlags(),
			TClass::StaticConfigName(),
			(UClass::ClassConstructorType)InternalConstructor<TClass>,
			(UClass::ClassVTableHelperCtorCallerType)InternalVTableHelperCtorCaller<TClass>,
			UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS(TClass),
			&TClass::Super::StaticClass,
			&TClass::WithinClass::StaticClass
		);
	}
	return Z_Registration_Info_UClass_AVoxelGameMode.InnerSingleton;
}
UClass* Z_Construct_UClass_AVoxelGameMode_NoRegister()
{
	return AVoxelGameMode::GetPrivateStaticClass();
}
struct Z_Construct_UClass_AVoxelGameMode_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n * \n */" },
#endif
		{ "HideCategories", "Info Rendering MovementReplication Replication Actor Input Movement Collision Rendering HLOD WorldPartition DataLayers Transformation" },
		{ "IncludePath", "VoxelGameMode.h" },
		{ "ModuleRelativePath", "VoxelGameMode.h" },
		{ "ShowCategories", "Input|MouseInput Input|TouchInput" },
	};
#endif // WITH_METADATA

// ********** Begin Class AVoxelGameMode constinit property declarations ***************************
// ********** End Class AVoxelGameMode constinit property declarations *****************************
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<AVoxelGameMode>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct Z_Construct_UClass_AVoxelGameMode_Statics
UObject* (*const Z_Construct_UClass_AVoxelGameMode_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AGameModeBase,
	(UObject* (*)())Z_Construct_UPackage__Script_VoxelTest,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AVoxelGameMode_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_AVoxelGameMode_Statics::ClassParams = {
	&AVoxelGameMode::StaticClass,
	"Game",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	nullptr,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	0,
	0,
	0x009002ACu,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_AVoxelGameMode_Statics::Class_MetaDataParams), Z_Construct_UClass_AVoxelGameMode_Statics::Class_MetaDataParams)
};
void AVoxelGameMode::StaticRegisterNativesAVoxelGameMode()
{
}
UClass* Z_Construct_UClass_AVoxelGameMode()
{
	if (!Z_Registration_Info_UClass_AVoxelGameMode.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_AVoxelGameMode.OuterSingleton, Z_Construct_UClass_AVoxelGameMode_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_AVoxelGameMode.OuterSingleton;
}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, AVoxelGameMode);
AVoxelGameMode::~AVoxelGameMode() {}
// ********** End Class AVoxelGameMode *************************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelTest_VoxelGameMode_h__Script_VoxelTest_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_AVoxelGameMode, AVoxelGameMode::StaticClass, TEXT("AVoxelGameMode"), &Z_Registration_Info_UClass_AVoxelGameMode, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(AVoxelGameMode), 2842332393U) },
	};
}; // Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelTest_VoxelGameMode_h__Script_VoxelTest_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelTest_VoxelGameMode_h__Script_VoxelTest_328089448{
	TEXT("/Script/VoxelTest"),
	Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelTest_VoxelGameMode_h__Script_VoxelTest_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelTest_VoxelGameMode_h__Script_VoxelTest_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0,
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
