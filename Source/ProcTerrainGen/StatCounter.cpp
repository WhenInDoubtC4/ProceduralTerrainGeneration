// Fill out your copyright notice in the Description page of Project Settings.


#include "StatCounter.h"

UStatCounter::UStatCounter()
{

}

void UStatCounter::Start(bool reset)
{
	if (reset) Duration = 0.;

	StartTime = FPlatformTime::Seconds();
}

void UStatCounter::Stop()
{
	double endTime = FPlatformTime::Seconds();

	Duration += endTime - StartTime;
}

void UStatCounter::Reset()
{
	Duration = 0.;
}