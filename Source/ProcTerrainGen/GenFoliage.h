// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProceduralFoliageVolume.h"
#include "ProceduralFoliageComponent.h"
#include "ProceduralFoliageSpawner.h"
#include "InstancedFoliageActor.h"
#include "GenHeight.h"
#include "GenFoliage.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROCTERRAINGEN_API UGenFoliage : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UGenFoliage();

	void UpdateBounds(const FVector& position, const FVector& extent);
	void Spawn(UGenHeight* heightGenerator);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(VisibleAnywhere)
	AProceduralFoliageVolume* ProceduralFoliageVolume = nullptr;

	UPROPERTY(EditAnywhere)
	UProceduralFoliageSpawner* FoliageSpawner = nullptr;
};
