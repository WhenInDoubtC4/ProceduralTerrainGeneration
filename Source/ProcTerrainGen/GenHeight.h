// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/Texture2D.h"
#include "GenHeight.generated.h"

USTRUCT(BlueprintType)
struct FHeightGeneratorOptions
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	float seed = 0.f;

	UPROPERTY(BlueprintReadWrite)
	float step1Period = .01f;

	UPROPERTY(BlueprintReadWrite)
	float step1Amplitude = 10000.f;

	float step2Period = .1f;

	float step2Amplitude = 300.f;
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

	UPROPERTY(BlueprintAssignable)
	FHeightmapTextureUpdated OnHeightmapTextureUpdated;
	
	UFUNCTION(BlueprintCallable)
	UTexture2D* GetHeightmapTexture() const { return HeightmapTexture; };

	UFUNCTION(BlueprintCallable)
	void SetGenerationOptions(FHeightGeneratorOptions options) { GenOptions = options; }

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private:
	uint32 xSections;
	uint32 ySections;
	uint32 xSize;
	uint32 ySize;
	TArray<float> HeightData;

	void DrawTexture();

	UPROPERTY(VisibleAnywhere, BlueprintGetter = GetHeightmapTexture)
	UTexture2D* HeightmapTexture = nullptr;

	UPROPERTY(BlueprintSetter = SetGenerationOptions)
	FHeightGeneratorOptions GenOptions;

	float CalculateHeightValue(const FVector2D& position);

	float NormalizeHeightValue(float heightValue);
};
