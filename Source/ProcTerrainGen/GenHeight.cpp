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

void UGenHeight::Initialize(uint32 width, uint32 height)
{
	xSize = width;
	ySize = height;

	HeightData.Empty();
}

TArray<float>& UGenHeight::GenerateHeight()
{
	for (uint32 y = 0; y < ySize; y++)
	{
		for (uint32 x = 0; x < xSize; x++)
		{
			FVector2D position((float)x, (float)y);

			position *= .01f;
			position += .1f * FVector2D::UnitVector;

			float heightValue = FMath::PerlinNoise2D(position) * 10000.f;
			HeightData.Add(heightValue);
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
	//Even though a R8 format would be the most efficiet, render as RGB so that it shows up ok in the UI
	HeightmapTexture = UTexture2D::CreateTransient(xSize, ySize, EPixelFormat::PF_R8G8B8A8, "AAAAA");
	HeightmapTexture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
	
	FByteBulkData& textureData = HeightmapTexture->GetPlatformData()->Mips[0].BulkData;
	auto pixels = reinterpret_cast<FColor*>(textureData.Lock(LOCK_READ_WRITE));

	for (int32 i = 0; i < HeightData.Num(); i++)
	{
		float normalizedValue = (HeightData[i] * .0001f) * .5f + .5f;
		uint8 grayscaleValue = FMath::Floor(normalizedValue * 255.f);

		pixels[i] = FColor(grayscaleValue, grayscaleValue, grayscaleValue);
	}

	textureData.Unlock();
	HeightmapTexture->UpdateResource();

	OnHeightmapTextureUpdated.Broadcast(HeightmapTexture);
}
