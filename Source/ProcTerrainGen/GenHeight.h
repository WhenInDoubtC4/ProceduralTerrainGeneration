// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/Texture2D.h"
#include "GenHeight.generated.h"

UENUM(BlueprintType)
enum EErosionMethod
{
	EROSION_METHOD_Grid,
	EROSION_METHOD_Particle,
};

USTRUCT(BlueprintType)
struct FHeightGeneratorOptions
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite)
	float seed = 0.f;

	UPROPERTY(BlueprintReadWrite)
	float step1Period = .01f;

	UPROPERTY(BlueprintReadWrite)
	float step1Amplitude = 10000.f;

	UPROPERTY(BlueprintReadWrite)
	float step2Period = .1f;

	UPROPERTY(BlueprintReadWrite)
	float step2Amplitude = 300.f;

	UPROPERTY(BlueprintReadWrite)
	TEnumAsByte<EErosionMethod> erosionMethod = EROSION_METHOD_Particle;

	UPROPERTY(BlueprintReadWrite)
	int32 particleErosion_iterations = 16384;

	UPROPERTY(BlueprintReadWrite)
	float particleErosion_minAngle = 5.f;

	UPROPERTY(BlueprintReadWrite)
	float particleErosion_waterAmount = 10.f;

	UPROPERTY(BlueprintReadWrite)
	float particleErosion_evaporationRate = 1.f;

	UPROPERTY(BlueprintReadWrite)
	float particleErosion_accelerationConsant = .6f;

	UPROPERTY(BlueprintReadWrite)
	float particleErosion_frictionConstant = .3f;

	UPROPERTY(BlueprintReadWrite)
	float particleErosion_sedimentCarryingCapacity = 10.f;

	UPROPERTY(BlueprintReadWrite)
	float particleErosion_depositionConstant = .3f;

	UPROPERTY(BlueprintReadWrite)
	float particleErosion_soilSoftnessConstant = .5f;

	UPROPERTY(BlueprintReadWrite)
	int32 gridErosion_iterations = 32;

	UPROPERTY(BlueprintReadWrite)
	int32 gridErosion_rainfallInterval = 4;

	UPROPERTY(BlueprintReadWrite)
	float gridErosion_rainfall = 10.f;

	UPROPERTY(BlueprintReadWrite)
	float gridErosion_depositionConstant = .1f;

	UPROPERTY(BlueprintReadWrite)
	float gridErosion_sedimentCapacity = 5.f;

	UPROPERTY(BlueprintReadWrite)
	float gridErosion_soilSoftness = .3f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHeightmapTextureUpdated, UTexture2D*, heightmapTexture);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROCTERRAINGEN_API UGenHeight : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UGenHeight();

	void Initialize(uint32 xSectionCount, uint32 ySectionCount, uint32 sectionWidth, uint32 sectionHeight);
	TArray<float>& GenerateHeight(uint32 xSection, uint32 ySection, TArray<float>& outLocalHeightData);
	void DrawTexture();

	UPROPERTY(BlueprintAssignable)
	FHeightmapTextureUpdated OnHeightmapTextureUpdated;
	
	UFUNCTION(BlueprintCallable)
	UTexture2D* GetHeightmapTexture() const { return HeightmapTexture; };

	UFUNCTION(BlueprintCallable)
	void SetGenerationOptions(FHeightGeneratorOptions options) { GenOptions = options; }

	void Erode();
	void GridBasedErosion();
	void ParticleBasedErosion();
	void ThermalWeathering();

	void GlobalSmooth();
	
	void GetSectionHeight(uint32 sectionIndex, TArray<float>& height);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private:
	uint32 xSections;
	uint32 ySections;
	uint32 xSize;
	uint32 ySize;
	TArray<float> HeightData;

	UPROPERTY(VisibleAnywhere, BlueprintGetter = GetHeightmapTexture)
	UTexture2D* HeightmapTexture = nullptr;

	UPROPERTY(BlueprintSetter = SetGenerationOptions)
	FHeightGeneratorOptions GenOptions;

	float CalculateHeightValue(const FVector2D& position);

	float NormalizeHeightValue(float heightValue);

	uint32 GetGlobalIndex(uint32 currentXSection, uint32 currentYSection, uint32 currentXPosition, uint32 currentYPosition);
	uint32 GetGlobalIndex(uint32 globalXPosition, uint32 globalYPosition);

	FVector GetNormal(int32 globalIndex);

	void GetPositionRangeF(float xPos, float yPos, int32& outXMin, int32& outXMax, int32& outYMin, int32& outYMax);

	FVector GetNormalF(float xPos, float yPos);

	//Helper function that modifies corresponding discrete height values
	void ModifyHeightF(float xPos, float yPos, float diff);
};
