// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "VoxelChunck.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodeVoxelChunck() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_AActor();
UPackage* Z_Construct_UPackage__Script_VoxelModule();
VOXELMODULE_API UClass* Z_Construct_UClass_AVoxelChunck();
VOXELMODULE_API UClass* Z_Construct_UClass_AVoxelChunck_NoRegister();
// ********** End Cross Module References **********************************************************

// ********** Begin Class AVoxelChunck *************************************************************
FClassRegistrationInfo Z_Registration_Info_UClass_AVoxelChunck;
UClass* AVoxelChunck::GetPrivateStaticClass()
{
	using TClass = AVoxelChunck;
	if (!Z_Registration_Info_UClass_AVoxelChunck.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			TClass::StaticPackage(),
			TEXT("VoxelChunck"),
			Z_Registration_Info_UClass_AVoxelChunck.InnerSingleton,
			StaticRegisterNativesAVoxelChunck,
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
	return Z_Registration_Info_UClass_AVoxelChunck.InnerSingleton;
}
UClass* Z_Construct_UClass_AVoxelChunck_NoRegister()
{
	return AVoxelChunck::GetPrivateStaticClass();
}
struct Z_Construct_UClass_AVoxelChunck_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "IncludePath", "VoxelChunck.h" },
		{ "ModuleRelativePath", "Public/VoxelChunck.h" },
	};
#endif // WITH_METADATA

// ********** Begin Class AVoxelChunck constinit property declarations *****************************
// ********** End Class AVoxelChunck constinit property declarations *******************************
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<AVoxelChunck>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct Z_Construct_UClass_AVoxelChunck_Statics
UObject* (*const Z_Construct_UClass_AVoxelChunck_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_VoxelModule,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AVoxelChunck_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_AVoxelChunck_Statics::ClassParams = {
	&AVoxelChunck::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	nullptr,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	0,
	0,
	0x009000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_AVoxelChunck_Statics::Class_MetaDataParams), Z_Construct_UClass_AVoxelChunck_Statics::Class_MetaDataParams)
};
void AVoxelChunck::StaticRegisterNativesAVoxelChunck()
{
}
UClass* Z_Construct_UClass_AVoxelChunck()
{
	if (!Z_Registration_Info_UClass_AVoxelChunck.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_AVoxelChunck.OuterSingleton, Z_Construct_UClass_AVoxelChunck_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_AVoxelChunck.OuterSingleton;
}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, AVoxelChunck);
AVoxelChunck::~AVoxelChunck() {}
// ********** End Class AVoxelChunck ***************************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_VoxelChunck_h__Script_VoxelModule_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_AVoxelChunck, AVoxelChunck::StaticClass, TEXT("AVoxelChunck"), &Z_Registration_Info_UClass_AVoxelChunck, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(AVoxelChunck), 3558068017U) },
	};
}; // Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_VoxelChunck_h__Script_VoxelModule_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_VoxelChunck_h__Script_VoxelModule_1187500005{
	TEXT("/Script/VoxelModule"),
	Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_VoxelChunck_h__Script_VoxelModule_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_VoxelChunck_h__Script_VoxelModule_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0,
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
