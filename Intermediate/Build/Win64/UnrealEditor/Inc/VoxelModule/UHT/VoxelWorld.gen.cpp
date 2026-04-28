// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "VoxelWorld.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodeVoxelWorld() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_AActor();
UPackage* Z_Construct_UPackage__Script_VoxelModule();
VOXELMODULE_API UClass* Z_Construct_UClass_AChunckManager_NoRegister();
VOXELMODULE_API UClass* Z_Construct_UClass_AVoxelWorld();
VOXELMODULE_API UClass* Z_Construct_UClass_AVoxelWorld_NoRegister();
// ********** End Cross Module References **********************************************************

// ********** Begin Class AVoxelWorld **************************************************************
FClassRegistrationInfo Z_Registration_Info_UClass_AVoxelWorld;
UClass* AVoxelWorld::GetPrivateStaticClass()
{
	using TClass = AVoxelWorld;
	if (!Z_Registration_Info_UClass_AVoxelWorld.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			TClass::StaticPackage(),
			TEXT("VoxelWorld"),
			Z_Registration_Info_UClass_AVoxelWorld.InnerSingleton,
			StaticRegisterNativesAVoxelWorld,
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
	return Z_Registration_Info_UClass_AVoxelWorld.InnerSingleton;
}
UClass* Z_Construct_UClass_AVoxelWorld_NoRegister()
{
	return AVoxelWorld::GetPrivateStaticClass();
}
struct Z_Construct_UClass_AVoxelWorld_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "IncludePath", "VoxelWorld.h" },
		{ "ModuleRelativePath", "Public/VoxelWorld.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ChunckManager_MetaData[] = {
		{ "Category", "Voxel" },
		{ "ModuleRelativePath", "Public/VoxelWorld.h" },
	};
#endif // WITH_METADATA

// ********** Begin Class AVoxelWorld constinit property declarations ******************************
	static const UECodeGen_Private::FObjectPropertyParams NewProp_ChunckManager;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class AVoxelWorld constinit property declarations ********************************
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<AVoxelWorld>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct Z_Construct_UClass_AVoxelWorld_Statics

// ********** Begin Class AVoxelWorld Property Definitions *****************************************
const UECodeGen_Private::FObjectPropertyParams Z_Construct_UClass_AVoxelWorld_Statics::NewProp_ChunckManager = { "ChunckManager", nullptr, (EPropertyFlags)0x0010000000020015, UECodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AVoxelWorld, ChunckManager), Z_Construct_UClass_AChunckManager_NoRegister, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ChunckManager_MetaData), NewProp_ChunckManager_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_AVoxelWorld_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AVoxelWorld_Statics::NewProp_ChunckManager,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AVoxelWorld_Statics::PropPointers) < 2048);
// ********** End Class AVoxelWorld Property Definitions *******************************************
UObject* (*const Z_Construct_UClass_AVoxelWorld_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_VoxelModule,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AVoxelWorld_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_AVoxelWorld_Statics::ClassParams = {
	&AVoxelWorld::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_AVoxelWorld_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_AVoxelWorld_Statics::PropPointers),
	0,
	0x009000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_AVoxelWorld_Statics::Class_MetaDataParams), Z_Construct_UClass_AVoxelWorld_Statics::Class_MetaDataParams)
};
void AVoxelWorld::StaticRegisterNativesAVoxelWorld()
{
}
UClass* Z_Construct_UClass_AVoxelWorld()
{
	if (!Z_Registration_Info_UClass_AVoxelWorld.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_AVoxelWorld.OuterSingleton, Z_Construct_UClass_AVoxelWorld_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_AVoxelWorld.OuterSingleton;
}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, AVoxelWorld);
AVoxelWorld::~AVoxelWorld() {}
// ********** End Class AVoxelWorld ****************************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_VoxelWorld_h__Script_VoxelModule_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_AVoxelWorld, AVoxelWorld::StaticClass, TEXT("AVoxelWorld"), &Z_Registration_Info_UClass_AVoxelWorld, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(AVoxelWorld), 2274847577U) },
	};
}; // Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_VoxelWorld_h__Script_VoxelModule_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_VoxelWorld_h__Script_VoxelModule_3140656523{
	TEXT("/Script/VoxelModule"),
	Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_VoxelWorld_h__Script_VoxelModule_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_VoxelWorld_h__Script_VoxelModule_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0,
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
