// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "ChunckManager.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodeChunckManager() {}

// ********** Begin Cross Module References ********************************************************
COREUOBJECT_API UClass* Z_Construct_UClass_UClass_NoRegister();
ENGINE_API UClass* Z_Construct_UClass_AActor();
UPackage* Z_Construct_UPackage__Script_VoxelModule();
VOXELMODULE_API UClass* Z_Construct_UClass_AChunckManager();
VOXELMODULE_API UClass* Z_Construct_UClass_AChunckManager_NoRegister();
VOXELMODULE_API UClass* Z_Construct_UClass_AVoxelChunck_NoRegister();
VOXELMODULE_API UClass* Z_Construct_UClass_AVoxelWorld_NoRegister();
// ********** End Cross Module References **********************************************************

// ********** Begin Class AChunckManager ***********************************************************
FClassRegistrationInfo Z_Registration_Info_UClass_AChunckManager;
UClass* AChunckManager::GetPrivateStaticClass()
{
	using TClass = AChunckManager;
	if (!Z_Registration_Info_UClass_AChunckManager.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			TClass::StaticPackage(),
			TEXT("ChunckManager"),
			Z_Registration_Info_UClass_AChunckManager.InnerSingleton,
			StaticRegisterNativesAChunckManager,
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
	return Z_Registration_Info_UClass_AChunckManager.InnerSingleton;
}
UClass* Z_Construct_UClass_AChunckManager_NoRegister()
{
	return AChunckManager::GetPrivateStaticClass();
}
struct Z_Construct_UClass_AChunckManager_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "IncludePath", "ChunckManager.h" },
		{ "ModuleRelativePath", "Public/ChunckManager.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_VoxelChunckClass_MetaData[] = {
		{ "Category", "Voxel" },
		{ "ModuleRelativePath", "Public/ChunckManager.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_VoxelWorld_MetaData[] = {
		{ "ModuleRelativePath", "Public/ChunckManager.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_VoxelSize_MetaData[] = {
		{ "Category", "ChunckManager" },
		{ "ModuleRelativePath", "Public/ChunckManager.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ChunkSize_MetaData[] = {
		{ "Category", "ChunckManager" },
		{ "ModuleRelativePath", "Public/ChunckManager.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_LODDistances_MetaData[] = {
		{ "Category", "Voxel | LOD" },
		{ "ModuleRelativePath", "Public/ChunckManager.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_MaxLOD_MetaData[] = {
		{ "Category", "Voxel | LOD" },
		{ "ModuleRelativePath", "Public/ChunckManager.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_PlayerSpawnHeight_MetaData[] = {
		{ "Category", "Voxel | Spawn" },
		{ "ModuleRelativePath", "Public/ChunckManager.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_MaxSpawnPerFrame_MetaData[] = {
		{ "Category", "Voxel | Performance" },
		{ "ModuleRelativePath", "Public/ChunckManager.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_MaxRebuildPerFrame_MetaData[] = {
		{ "Category", "Voxel | Performance" },
		{ "ModuleRelativePath", "Public/ChunckManager.h" },
	};
#endif // WITH_METADATA

// ********** Begin Class AChunckManager constinit property declarations ***************************
	static const UECodeGen_Private::FClassPropertyParams NewProp_VoxelChunckClass;
	static const UECodeGen_Private::FObjectPropertyParams NewProp_VoxelWorld;
	static const UECodeGen_Private::FIntPropertyParams NewProp_VoxelSize;
	static const UECodeGen_Private::FIntPropertyParams NewProp_ChunkSize;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_LODDistances_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_LODDistances;
	static const UECodeGen_Private::FIntPropertyParams NewProp_MaxLOD;
	static const UECodeGen_Private::FFloatPropertyParams NewProp_PlayerSpawnHeight;
	static const UECodeGen_Private::FIntPropertyParams NewProp_MaxSpawnPerFrame;
	static const UECodeGen_Private::FIntPropertyParams NewProp_MaxRebuildPerFrame;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class AChunckManager constinit property declarations *****************************
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<AChunckManager>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct Z_Construct_UClass_AChunckManager_Statics

// ********** Begin Class AChunckManager Property Definitions **************************************
const UECodeGen_Private::FClassPropertyParams Z_Construct_UClass_AChunckManager_Statics::NewProp_VoxelChunckClass = { "VoxelChunckClass", nullptr, (EPropertyFlags)0x0014000000000001, UECodeGen_Private::EPropertyGenFlags::Class, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AChunckManager, VoxelChunckClass), Z_Construct_UClass_UClass_NoRegister, Z_Construct_UClass_AVoxelChunck_NoRegister, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_VoxelChunckClass_MetaData), NewProp_VoxelChunckClass_MetaData) };
const UECodeGen_Private::FObjectPropertyParams Z_Construct_UClass_AChunckManager_Statics::NewProp_VoxelWorld = { "VoxelWorld", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AChunckManager, VoxelWorld), Z_Construct_UClass_AVoxelWorld_NoRegister, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_VoxelWorld_MetaData), NewProp_VoxelWorld_MetaData) };
const UECodeGen_Private::FIntPropertyParams Z_Construct_UClass_AChunckManager_Statics::NewProp_VoxelSize = { "VoxelSize", nullptr, (EPropertyFlags)0x0010000000000001, UECodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AChunckManager, VoxelSize), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_VoxelSize_MetaData), NewProp_VoxelSize_MetaData) };
const UECodeGen_Private::FIntPropertyParams Z_Construct_UClass_AChunckManager_Statics::NewProp_ChunkSize = { "ChunkSize", nullptr, (EPropertyFlags)0x0010000000000001, UECodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AChunckManager, ChunkSize), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ChunkSize_MetaData), NewProp_ChunkSize_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AChunckManager_Statics::NewProp_LODDistances_Inner = { "LODDistances", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UClass_AChunckManager_Statics::NewProp_LODDistances = { "LODDistances", nullptr, (EPropertyFlags)0x0010000000000001, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AChunckManager, LODDistances), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_LODDistances_MetaData), NewProp_LODDistances_MetaData) };
const UECodeGen_Private::FIntPropertyParams Z_Construct_UClass_AChunckManager_Statics::NewProp_MaxLOD = { "MaxLOD", nullptr, (EPropertyFlags)0x0010000000000001, UECodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AChunckManager, MaxLOD), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_MaxLOD_MetaData), NewProp_MaxLOD_MetaData) };
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AChunckManager_Statics::NewProp_PlayerSpawnHeight = { "PlayerSpawnHeight", nullptr, (EPropertyFlags)0x0010000000000001, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AChunckManager, PlayerSpawnHeight), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_PlayerSpawnHeight_MetaData), NewProp_PlayerSpawnHeight_MetaData) };
const UECodeGen_Private::FIntPropertyParams Z_Construct_UClass_AChunckManager_Statics::NewProp_MaxSpawnPerFrame = { "MaxSpawnPerFrame", nullptr, (EPropertyFlags)0x0010000000000001, UECodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AChunckManager, MaxSpawnPerFrame), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_MaxSpawnPerFrame_MetaData), NewProp_MaxSpawnPerFrame_MetaData) };
const UECodeGen_Private::FIntPropertyParams Z_Construct_UClass_AChunckManager_Statics::NewProp_MaxRebuildPerFrame = { "MaxRebuildPerFrame", nullptr, (EPropertyFlags)0x0010000000000001, UECodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AChunckManager, MaxRebuildPerFrame), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_MaxRebuildPerFrame_MetaData), NewProp_MaxRebuildPerFrame_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_AChunckManager_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AChunckManager_Statics::NewProp_VoxelChunckClass,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AChunckManager_Statics::NewProp_VoxelWorld,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AChunckManager_Statics::NewProp_VoxelSize,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AChunckManager_Statics::NewProp_ChunkSize,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AChunckManager_Statics::NewProp_LODDistances_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AChunckManager_Statics::NewProp_LODDistances,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AChunckManager_Statics::NewProp_MaxLOD,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AChunckManager_Statics::NewProp_PlayerSpawnHeight,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AChunckManager_Statics::NewProp_MaxSpawnPerFrame,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AChunckManager_Statics::NewProp_MaxRebuildPerFrame,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AChunckManager_Statics::PropPointers) < 2048);
// ********** End Class AChunckManager Property Definitions ****************************************
UObject* (*const Z_Construct_UClass_AChunckManager_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_VoxelModule,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AChunckManager_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_AChunckManager_Statics::ClassParams = {
	&AChunckManager::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_AChunckManager_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_AChunckManager_Statics::PropPointers),
	0,
	0x009000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_AChunckManager_Statics::Class_MetaDataParams), Z_Construct_UClass_AChunckManager_Statics::Class_MetaDataParams)
};
void AChunckManager::StaticRegisterNativesAChunckManager()
{
}
UClass* Z_Construct_UClass_AChunckManager()
{
	if (!Z_Registration_Info_UClass_AChunckManager.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_AChunckManager.OuterSingleton, Z_Construct_UClass_AChunckManager_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_AChunckManager.OuterSingleton;
}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, AChunckManager);
AChunckManager::~AChunckManager() {}
// ********** End Class AChunckManager *************************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_ChunckManager_h__Script_VoxelModule_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_AChunckManager, AChunckManager::StaticClass, TEXT("AChunckManager"), &Z_Registration_Info_UClass_AChunckManager, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(AChunckManager), 2042151809U) },
	};
}; // Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_ChunckManager_h__Script_VoxelModule_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_ChunckManager_h__Script_VoxelModule_2633342243{
	TEXT("/Script/VoxelModule"),
	Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_ChunckManager_h__Script_VoxelModule_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelModule_Public_ChunckManager_h__Script_VoxelModule_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0,
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
