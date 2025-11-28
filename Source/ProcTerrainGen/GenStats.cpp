// Fill out your copyright notice in the Description page of Project Settings.


#include "GenStats.h"

UGenStats::UGenStats()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UGenStats::BeginPlay()
{
	Super::BeginPlay();	
}

void UGenStats::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UGenStats::Save()
{
	//Nothing to save
	if (Counters.IsEmpty()) return;

	FString header;
	FString values;
	for (UStatCounter* counter : Counters)
	{
		FString counterName = counter->GetName();
		header += counterName + ",";
	}

#ifdef PLATFORM_DESKTOP
	IDesktopPlatform* desktopPlatform = FDesktopPlatformModule::Get();

	TArray<FString> fileNames;
	desktopPlatform->SaveFileDialog(nullptr, TEXT("PCG Stats"), TEXT(""), TEXT("pcgStats.csv"), TEXT("CSV (*.csv)|*.csv"), EFileDialogFlags::None, fileNames);

	if (fileNames.IsEmpty()) return;

	FFileHelper::SaveStringToFile(FStringView(TEXT("Test, Test2")), *fileNames[0]);
#endif
}

UStatCounter* UGenStats::AddCounter(const FName& name)
{
	auto newCounter = NewObject<UStatCounter>(this, name);

	Counters.Add(newCounter);

	return newCounter;
}

void UGenStats::ResetAllCounters()
{
	for (UStatCounter* counter : Counters)
	{
		counter->Reset();
	}
}