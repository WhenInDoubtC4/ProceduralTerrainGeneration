// Fill out your copyright notice in the Description page of Project Settings.


#include "GenFoliage.h"

UGenFoliage::UGenFoliage()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
}

void UGenFoliage::UpdateBounds(const FVector& position, const FVector& extent)
{
	ProceduralFoliageVolume->SetActorLocation(position);
	ProceduralFoliageVolume->SetActorScale3D(extent);
}

void UGenFoliage::Spawn(UGenHeight* heightGenerator)
{
	TArray<FDesiredFoliageInstance> desiredInstances;

	ProceduralFoliageVolume->ProceduralComponent->GenerateProceduralContent(desiredInstances);

	TMap<const UFoliageType*, TArray<FFoliageInstance>> instanceMap;

	for (const FDesiredFoliageInstance& desiredInstance : desiredInstances)
	{
		TArray<FFoliageInstance>& instancesForType = instanceMap.FindOrAdd(desiredInstance.FoliageType);

		FVector location = desiredInstance.StartTrace;

		FVector normal;
		float height;
		if (!heightGenerator->HeightfieldCast(location.X, location.Y, height, normal)) continue;

		//TODO: Filter based on rock slope angle
		if (normal.Dot(FVector::UpVector) < .01f) continue;

		FFoliageInstance newInstance;
		location.Z = height;
		newInstance.Location = location;
		newInstance.Rotation = desiredInstance.Rotation.Rotator();
		newInstance.ProceduralGuid = desiredInstance.ProceduralGuid;
		newInstance.AlignToNormal(normal, FMath::DegreesToRadians(30.f));

		instancesForType.Add(newInstance);
	}

	auto instancedFoliageActor = AInstancedFoliageActor::Get(GetWorld(), true, GetWorld()->GetCurrentLevel(), FVector::ZeroVector);

	for (const auto& instance : instanceMap)
	{
		FFoliageInfo* info;

		instancedFoliageActor->AddFoliageType(instance.Key, &info);
		
		TArray<const FFoliageInstance*> instances;
		for (const FFoliageInstance& idk : instance.Value) instances.Add(&idk);

		info->AddInstances(instance.Key, instances);
		info->Refresh(true, false);
	}
}

void UGenFoliage::BeginPlay()
{
	Super::BeginPlay();

	ProceduralFoliageVolume = NewObject<AProceduralFoliageVolume>(this, "Procedural foliage volume");

	ProceduralFoliageVolume->ProceduralComponent->bAllowLandscape = false;
	ProceduralFoliageVolume->ProceduralComponent->bAllowBSP = false;

	if (FoliageSpawner)
	{
		ProceduralFoliageVolume->ProceduralComponent->FoliageSpawner = FoliageSpawner;
	}
}

void UGenFoliage::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

