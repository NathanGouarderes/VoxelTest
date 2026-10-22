// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WidgetDebug.generated.h"

/**
 * 
 */
UCLASS()
class VOXELTEST_API UWidgetDebug : public UUserWidget
{
	GENERATED_BODY()
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> TextBlock_Debug;
};
