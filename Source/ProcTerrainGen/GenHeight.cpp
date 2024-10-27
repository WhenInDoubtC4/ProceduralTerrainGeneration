// Fill out your copyright notice in the Description page of Project Settings.


#include "GenHeight.h"

// Sets default values for this component's properties
UGenHeight::UGenHeight()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	bTickInEditor = true;
}

void UGenHeight::Initialize(uint32 xSectionCount, uint32 ySectionCount, uint32 sectionWidth, uint32 sectionHeight)
{
	xSections = xSectionCount;
	ySections = ySectionCount;
	xSize = sectionWidth;
	ySize = sectionHeight;

	uint32 arraySize = ySectionCount * sectionHeight * xSectionCount * sectionWidth;
	HeightData.Empty(arraySize);
	for (uint32 i = 0; i < arraySize; i++)
	{
		HeightData.AddUninitialized();
	}
}

TArray<float>& UGenHeight::GenerateHeight(uint32 xSection, uint32 ySection, TArray<float>& outLocalHeightData)
{	
	uint32 xStart = xSection * (xSize - 1);
	uint32 yStart = ySection * (ySize - 1);
	
	uint32 sectionSize = xSize * ySize;
	uint32 startIndex = ySection * xSections * sectionSize + xSection * sectionSize;

	for (uint32 y = 0; y < ySize; y++)
	{
		for (uint32 x = 0; x < xSize; x++)
		{
			FVector2D position(float(xStart + x), float(yStart + y));

			float heightValue = CalculateHeightValue(position);
			HeightData[ySection * xSections * xSize * ySize + y * xSections * xSize + xSection * xSize + x] = heightValue;
			outLocalHeightData.Add(heightValue);
		}
	}

	DrawTexture();

	return HeightData;
}


// Called when the game starts
void UGenHeight::BeginPlay()
{
	Super::BeginPlay();
}

void UGenHeight::DrawTexture()
{
	int32 xTexSize = xSections * xSize;
	int32 yTexSize = ySections * ySize;

	//Even though a R8 format would be the most efficiet, render as RGB so that it shows up ok in the UI
	HeightmapTexture = UTexture2D::CreateTransient(xTexSize, yTexSize, EPixelFormat::PF_R8G8B8A8, "Heightmap texture");
	HeightmapTexture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
	
	FByteBulkData& textureData = HeightmapTexture->GetPlatformData()->Mips[0].BulkData;
	auto pixels = reinterpret_cast<FColor*>(textureData.Lock(LOCK_READ_WRITE));

	for (int32 i = 0; i < HeightData.Num(); i++)
	{
		float normalizedValue = NormalizeHeightValue(HeightData[i]);
		uint8 grayscaleValue = FMath::Floor(normalizedValue * 255.f);

		pixels[i] = FColor(grayscaleValue, grayscaleValue, grayscaleValue);
	}

	textureData.Unlock();
	HeightmapTexture->UpdateResource();

	OnHeightmapTextureUpdated.Broadcast(HeightmapTexture);
}

float UGenHeight::CalculateHeightValue(const FVector2D& position)
{
	//Step1
	FVector2D step1Position = position;

	step1Position *= GenOptions.step1Period;
	step1Position += .1f * FVector2D::UnitVector; //Add a fraction here as this could produce bad results with integer values
	step1Position += FMath::Fmod(GenOptions.seed, 5000.f) * FVector2D::UnitVector;

	float step1Value = FMath::PerlinNoise2D(step1Position) * GenOptions.step1Amplitude;

	//Step2
	FVector2D step2Position = position;

	step2Position *= GenOptions.step2Period;
	step2Position += .1f * FVector2D::UnitVector;

	float step2Value = FMath::PerlinNoise2D(step2Position) * GenOptions.step2Amplitude;

	return step1Value + step2Value;
}

float UGenHeight::NormalizeHeightValue(float heightValue)
{
	heightValue /= (GenOptions.step1Amplitude + GenOptions.step2Amplitude);

	heightValue *= .5f;
	heightValue += .5f;

	return heightValue;
}
