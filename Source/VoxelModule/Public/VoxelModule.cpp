// Fill out your copyright notice in the Description page of Project Settings.


#include "VoxelModule.h"
#define LOCTEXT_NAMESPACE "FVoxelModule"

void FVoxelModule::StartupModule()
{
	UE_LOG(LogTemp, Warning, TEXT("Voxel Module Started"));
}

void FVoxelModule::ShutdownModule()
{
	UE_LOG(LogTemp, Warning, TEXT("Voxel Module Shutdown"));
}

#undef LOCTEXT_NAMESPACE
