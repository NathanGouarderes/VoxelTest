// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class VOXELAIMODULE_API FVoxelAIModule : public  IModuleInterface
{
	public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
IMPLEMENT_MODULE(FVoxelAIModule, VoxelAIModule);
