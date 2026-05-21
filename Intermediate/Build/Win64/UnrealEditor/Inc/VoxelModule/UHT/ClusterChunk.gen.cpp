// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "ClusterChunk.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodeClusterChunk() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_AActor();
UPackage* Z_Construct_UPackage__Script_VoxelModule();
VOXELMODULE_API UClass* Z_Construct_UClass_AClusterChunk();
VOXELMODULE_API UClass* Z_Construct_UClass_AClusterChunk_NoRegister();
// ********** End Cross Module References **********************************************************

// ********** Begin Class AClusterChunk ************************************************************
FClassRegistrationInfo Z_Registration_Info_UClass_AClusterChunk;
UClass* AClusterChunk::GetPrivateStaticClass()
{
	using TClass = AClusterChunk;
	if (!Z_Registration_Info_UClass_AClusterChunk.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			TClass::StaticPackage(),
			TEXT("ClusterChunk"),
			Z_Registration_Info_UClass_AClusterChunk.InnerSingleton,
			StaticRegisterNativesAClusterChunk,
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
	return Z_Registration_Info_UClass_AClusterChunk.InnerSingleton;
}
UClass* Z_Construct_UClass_AClusterChunk_NoRegister()
{
	return AClusterChunk::GetPrivateStaticClass();
}
struct Z_Construct_UClass_AClusterChunk_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "IncludePath", "ClusterChunk.h" },
		{ "ModuleRelativePath", "Public/ClusterChunk.h" },
	};
#endif // WITH_METADATA

// ********** Begin Class AClusterChunk constinit property declarations ****************************
// ********** End Class AClusterChunk constinit property declarations ******************************
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<AClusterChunk>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct Z_Construct_UClass_AClusterChunk_Statics
UObject* (*const Z_Construct_UClass_AClusterChunk_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_VoxelModule,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AClusterChunk_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_AClusterChunk_Statics::ClassParams = {
	&AClusterChunk::StaticClass,
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
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_AClusterChunk_Statics::Class_MetaDataParams), Z_Construct_UClass_AClusterChunk_Statics::Class_MetaDataParams)
};
void AClusterChunk::StaticRegisterNativesAClusterChunk()
{
}
UClass* Z_Construct_UClass_AClusterChunk()
{
	if (!Z_Registration_Info_UClass_AClusterChunk.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_AClusterChunk.OuterSingleton, Z_Construct_UClass_AClusterChunk_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_AClusterChunk.OuterSingleton;
}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, AClusterChunk);
AClusterChunk::~AClusterChunk() {}
// ********** End Class AClusterChunk **************************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_ClusterChunk_h__Script_VoxelModule_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_AClusterChunk, AClusterChunk::StaticClass, TEXT("AClusterChunk"), &Z_Registration_Info_UClass_AClusterChunk, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(AClusterChunk), 4187701147U) },
	};
}; // Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_ClusterChunk_h__Script_VoxelModule_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_ClusterChunk_h__Script_VoxelModule_3878968644{
	TEXT("/Script/VoxelModule"),
	Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_ClusterChunk_h__Script_VoxelModule_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_ClusterChunk_h__Script_VoxelModule_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0,
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
