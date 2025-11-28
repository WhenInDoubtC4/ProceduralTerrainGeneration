// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StatCounter.h"

#ifdef PLATFORM_DESKTOP
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Misc/FileHelper.h"
#endif

#include "GenStats.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROCTERRAINGEN_API UGenStats : public UActorComponent
{
	GENERATED_BODY()

public:	
	UGenStats();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable)
	void Save();

	UStatCounter* AddCounter(const FName& name);

	void ResetAllCounters();

private:
	TArray<UStatCounter*> Counters;
};
