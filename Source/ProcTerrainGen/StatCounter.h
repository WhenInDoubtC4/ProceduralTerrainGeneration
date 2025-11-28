// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StatCounter.generated.h"

/**
 * 
 */
UCLASS()
class PROCTERRAINGEN_API UStatCounter : public UObject
{
	GENERATED_BODY()
	
public:
	UStatCounter();

	void Start(bool reset = false);
	void Stop();
	void Reset();
	double GetSeconds() const { return Duration; };

private:
	double Duration = 0.;
	double StartTime = 0.;
};
