// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "VoxelTest/Components/PlayerControllerComponent.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodePlayerControllerComponent() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_APlayerController();
UPackage* Z_Construct_UPackage__Script_VoxelTest();
VOXELTEST_API UClass* Z_Construct_UClass_APlayerControllerComponent();
VOXELTEST_API UClass* Z_Construct_UClass_APlayerControllerComponent_NoRegister();
// ********** End Cross Module References **********************************************************

// ********** Begin Class APlayerControllerComponent ***********************************************
FClassRegistrationInfo Z_Registration_Info_UClass_APlayerControllerComponent;
UClass* APlayerControllerComponent::GetPrivateStaticClass()
{
	using TClass = APlayerControllerComponent;
	if (!Z_Registration_Info_UClass_APlayerControllerComponent.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			TClass::StaticPackage(),
			TEXT("PlayerControllerComponent"),
			Z_Registration_Info_UClass_APlayerControllerComponent.InnerSingleton,
			StaticRegisterNativesAPlayerControllerComponent,
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
	return Z_Registration_Info_UClass_APlayerControllerComponent.InnerSingleton;
}
UClass* Z_Construct_UClass_APlayerControllerComponent_NoRegister()
{
	return APlayerControllerComponent::GetPrivateStaticClass();
}
struct Z_Construct_UClass_APlayerControllerComponent_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n * \n */" },
#endif
		{ "HideCategories", "Collision Rendering Transformation" },
		{ "IncludePath", "Components/PlayerControllerComponent.h" },
		{ "ModuleRelativePath", "Components/PlayerControllerComponent.h" },
	};
#endif // WITH_METADATA

// ********** Begin Class APlayerControllerComponent constinit property declarations ***************
// ********** End Class APlayerControllerComponent constinit property declarations *****************
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<APlayerControllerComponent>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct Z_Construct_UClass_APlayerControllerComponent_Statics
UObject* (*const Z_Construct_UClass_APlayerControllerComponent_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_APlayerController,
	(UObject* (*)())Z_Construct_UPackage__Script_VoxelTest,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_APlayerControllerComponent_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_APlayerControllerComponent_Statics::ClassParams = {
	&APlayerControllerComponent::StaticClass,
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
	0x009002A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_APlayerControllerComponent_Statics::Class_MetaDataParams), Z_Construct_UClass_APlayerControllerComponent_Statics::Class_MetaDataParams)
};
void APlayerControllerComponent::StaticRegisterNativesAPlayerControllerComponent()
{
}
UClass* Z_Construct_UClass_APlayerControllerComponent()
{
	if (!Z_Registration_Info_UClass_APlayerControllerComponent.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_APlayerControllerComponent.OuterSingleton, Z_Construct_UClass_APlayerControllerComponent_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_APlayerControllerComponent.OuterSingleton;
}
APlayerControllerComponent::APlayerControllerComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, APlayerControllerComponent);
APlayerControllerComponent::~APlayerControllerComponent() {}
// ********** End Class APlayerControllerComponent *************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelTest_Components_PlayerControllerComponent_h__Script_VoxelTest_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_APlayerControllerComponent, APlayerControllerComponent::StaticClass, TEXT("APlayerControllerComponent"), &Z_Registration_Info_UClass_APlayerControllerComponent, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(APlayerControllerComponent), 1728878389U) },
	};
}; // Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelTest_Components_PlayerControllerComponent_h__Script_VoxelTest_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelTest_Components_PlayerControllerComponent_h__Script_VoxelTest_2731972513{
	TEXT("/Script/VoxelTest"),
	Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelTest_Components_PlayerControllerComponent_h__Script_VoxelTest_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Users_natha_Documents_Unreal_Projects_VoxelTest_Source_VoxelTest_Components_PlayerControllerComponent_h__Script_VoxelTest_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0,
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
