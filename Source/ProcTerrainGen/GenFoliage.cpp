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
	//ProceduralFoliageVolume->SetActorScale3D(extent);

	auto foliageVolumeAABB = FBox::BuildAABB(position, FVector(10000.f, 10000.f, 5000.f));
	ProceduralFoliageVolume->GetBrushComponent()->Bounds = foliageVolumeAABB;
}

void UGenFoliage::Spawn(UGenHeight* heightGenerator, FFoliageGenerationOptions options)
{
	float slopeAngleValue = FMath::Cos(FMath::DegreesToRadians(options.maxSlopeAngleDeg));

	TArray<FDesiredFoliageInstance> desiredInstances;

	ProceduralFoliageVolume->ProceduralComponent->GenerateProceduralContent(desiredInstances);

	TMap<const UFoliageType*, TArray<FFoliageInstance>> instanceMap;

	for (const FDesiredFoliageInstance& desiredInstance : desiredInstances)
	{
		TArray<FFoliageInstance>& instancesForType = instanceMap.FindOrAdd(desiredInstance.FoliageType);

		//Cannot use AInstancedFoliageActor::FoliageTrace as it will not collide with the procedural mesh
		FHitResult result;
		if (!GetWorld()->LineTraceSingleByChannel(result, desiredInstance.StartTrace, desiredInstance.EndTrace, ECollisionChannel::ECC_WorldDynamic)) continue;

		float height = result.Location.Z;
		if (height > options.alpineZone || height < options.beachHeight) continue;

		if (result.Normal.GetAbs().Dot(FVector::UpVector) < slopeAngleValue) continue;

		FFoliageInstance newInstance;
		newInstance.Location = result.Location;
		newInstance.Rotation = desiredInstance.Rotation.Rotator();
		newInstance.ProceduralGuid = desiredInstance.ProceduralGuid;
		newInstance.AlignToNormal(result.Normal, FMath::DegreesToRadians(30.f));

		instancesForType.Add(newInstance);
	}

	InstancedFoliageActor = AInstancedFoliageActor::Get(GetWorld(), true, GetWorld()->GetCurrentLevel(), FVector::ZeroVector);

	for (const auto& instance : instanceMap)
	{
		FFoliageInfo* info;

		InstancedFoliageActor->AddFoliageType(instance.Key, &info);
		
		TArray<const FFoliageInstance*> instances;
		for (const FFoliageInstance& idk : instance.Value) instances.Add(&idk);

		info->AddInstances(instance.Key, instances);
		info->Refresh(true, false);
	}
}

void UGenFoliage::Clear()
{
	if (!InstancedFoliageActor) return;

	InstancedFoliageActor->DeleteInstancesForAllProceduralFoliageComponents(true);
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