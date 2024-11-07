// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProceduralFoliageVolume.h"
#include "ProceduralFoliageComponent.h"
#include "ProceduralFoliageSpawner.h"
#include "InstancedFoliageActor.h"
#include "Components/BrushComponent.h"
#include "GenHeight.h"
#include "GenFoliage.generated.h"

USTRUCT(BlueprintType)
struct FFoliageGenerationOptions
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite)
	float maxSlopeAngleDeg = 40.f;

	UPROPERTY(BlueprintReadWrite)
	float beachHeight = 200.f;

	UPROPERTY(BlueprintReadWrite)
	float alpineZone = 8000.f;
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROCTERRAINGEN_API UGenFoliage : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UGenFoliage();

	void UpdateBounds(const FVector& position, const FVector& extent);
	void Spawn(UGenHeight* heightGenerator, FFoliageGenerationOptions options);
	void Clear();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere)
	AProceduralFoliageVolume* ProceduralFoliageVolume = nullptr;

	UPROPERTY(EditAnywhere)
	UProceduralFoliageSpawner* FoliageSpawner = nullptr;

	AInstancedFoliageActor* InstancedFoliageActor = nullptr;
};
