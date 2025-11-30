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

	std::ostringstream dataSStream;

	//Header
	for (UStatCounter* counter : Counters)
	{
		const FString counterName = counter->GetName();
		dataSStream << TCHAR_TO_UTF8(*counterName) << ',';
	}
	dataSStream << '\n';
	//First row of data
	for (UStatCounter* counter : Counters)
	{
		dataSStream << std::fixed << std::setprecision(12) << counter->GetSeconds() << ',';
	}
	dataSStream << '\n';

	//Save previous rows (if any)
	if (!SavedCounters.IsEmpty())
	{
		for (int32 i = 0; i < SavedCounters.Num(); i++)
		{
			if (i % Counters.Num() == 0 && i > 0) dataSStream << '\n';

			dataSStream << std::fixed << std::setprecision(12) << SavedCounters[SavedCounters.Num() - i - 1] << ',';
		}
	}

	std::string dataString = dataSStream.str();
	std::basic_string<TCHAR> dataTString(dataString.begin(), dataString.end());
	TCHAR* data = dataTString.data();

#ifdef PLATFORM_DESKTOP
	IDesktopPlatform* desktopPlatform = FDesktopPlatformModule::Get();

	TArray<FString> fileNames;
	desktopPlatform->SaveFileDialog(nullptr, TEXT("PCG Stats"), TEXT(""), TEXT("pcgStats.csv"), TEXT("CSV (*.csv)|*.csv"), EFileDialogFlags::None, fileNames);

	if (fileNames.IsEmpty()) return;

	FFileHelper::SaveStringToFile(data, *fileNames[0]);
#endif
}

UStatCounter* UGenStats::AddCounter(const FName& name)
{
	auto newCounter = NewObject<UStatCounter>(this, name);

	Counters.Add(newCounter);

	return newCounter;
}

void UGenStats::ResetAllCounters(bool clearSaved)
{
	for (UStatCounter* counter : Counters)
	{
		counter->Reset();
	}

	if (clearSaved) SavedCounters.Empty();
}

void UGenStats::AddNewRowToAllCounters()
{
	for (UStatCounter* counter : Counters)
	{
		SavedCounters.Add(counter->GetSeconds());
		counter->Reset();
	}
}
