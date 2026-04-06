// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "ChunckBacker.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodeChunckBacker() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_AActor();
UPackage* Z_Construct_UPackage__Script_VoxelModule();
VOXELMODULE_API UClass* Z_Construct_UClass_AChunckBacker();
VOXELMODULE_API UClass* Z_Construct_UClass_AChunckBacker_NoRegister();
// ********** End Cross Module References **********************************************************

// ********** Begin Class AChunckBacker ************************************************************
FClassRegistrationInfo Z_Registration_Info_UClass_AChunckBacker;
UClass* AChunckBacker::GetPrivateStaticClass()
{
	using TClass = AChunckBacker;
	if (!Z_Registration_Info_UClass_AChunckBacker.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			TClass::StaticPackage(),
			TEXT("ChunckBacker"),
			Z_Registration_Info_UClass_AChunckBacker.InnerSingleton,
			StaticRegisterNativesAChunckBacker,
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
	return Z_Registration_Info_UClass_AChunckBacker.InnerSingleton;
}
UClass* Z_Construct_UClass_AChunckBacker_NoRegister()
{
	return AChunckBacker::GetPrivateStaticClass();
}
struct Z_Construct_UClass_AChunckBacker_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "IncludePath", "ChunckBacker.h" },
		{ "ModuleRelativePath", "Public/ChunckBacker.h" },
	};
#endif // WITH_METADATA

// ********** Begin Class AChunckBacker constinit property declarations ****************************
// ********** End Class AChunckBacker constinit property declarations ******************************
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<AChunckBacker>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct Z_Construct_UClass_AChunckBacker_Statics
UObject* (*const Z_Construct_UClass_AChunckBacker_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_VoxelModule,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AChunckBacker_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_AChunckBacker_Statics::ClassParams = {
	&AChunckBacker::StaticClass,
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
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_AChunckBacker_Statics::Class_MetaDataParams), Z_Construct_UClass_AChunckBacker_Statics::Class_MetaDataParams)
};
void AChunckBacker::StaticRegisterNativesAChunckBacker()
{
}
UClass* Z_Construct_UClass_AChunckBacker()
{
	if (!Z_Registration_Info_UClass_AChunckBacker.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_AChunckBacker.OuterSingleton, Z_Construct_UClass_AChunckBacker_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_AChunckBacker.OuterSingleton;
}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, AChunckBacker);
AChunckBacker::~AChunckBacker() {}
// ********** End Class AChunckBacker **************************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_ChunckBacker_h__Script_VoxelModule_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_AChunckBacker, AChunckBacker::StaticClass, TEXT("AChunckBacker"), &Z_Registration_Info_UClass_AChunckBacker, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(AChunckBacker), 2568492750U) },
	};
}; // Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_ChunckBacker_h__Script_VoxelModule_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_ChunckBacker_h__Script_VoxelModule_2567609278{
	TEXT("/Script/VoxelModule"),
	Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_ChunckBacker_h__Script_VoxelModule_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_ChunckBacker_h__Script_VoxelModule_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0,
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
