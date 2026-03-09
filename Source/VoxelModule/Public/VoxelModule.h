// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * 
 */

class VOXELMODULE_API FVoxelModule : public  IModuleInterface
{
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
IMPLEMENT_MODULE(FVoxelModule, VoxelModule);


