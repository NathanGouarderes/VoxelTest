#include "../Public/VoxelAIModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogVoxelAI, Log, All);


void FVoxelAIModule::StartupModule()
{
	UE_LOG(LogTemp, Warning, TEXT("VoxelAIModule Module Startup"));

}

void FVoxelAIModule::ShutdownModule()
{
	UE_LOG(LogTemp, Warning, TEXT("VoxelAIModule Module Shutdown"));
}

#undef LOCTEXT_NAMESPACE
