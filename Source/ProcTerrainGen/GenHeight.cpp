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
	uint32 xStart = xSection * (xSize );
	uint32 yStart = ySection * (ySize );

	for (uint32 y = 0; y < ySize; y++)
	{
		for (uint32 x = 0; x < xSize; x++)
		{
			FVector2D position(float(xStart + x), float(yStart + y));

			float heightValue = CalculateHeightValue(position);
			//HeightData[ySection * xSections * xSize * ySize + y * xSections * xSize + xSection * xSize + x] = heightValue;
			HeightData[GetGlobalIndex(xSection, ySection, x, y)] = heightValue;
			outLocalHeightData.Add(heightValue);
		}
	}

	return HeightData;
}

void UGenHeight::Erode()
{
	switch (GenOptions.erosionMethod)
	{
	case EErosionMethod::EROSION_METHOD_Particle:
		ParticleBasedErosion();
		break;
	case EErosionMethod::EROSION_METHOD_Grid:
		GridBasedErosion();
		break;
	}
}

//https://dl-acm-org.cobalt.champlain.edu/doi/10.1145/74334.74337
void UGenHeight::GridBasedErosion()
{
	int32 erosionIterations = GenOptions.gridErosion_iterations;
	int32 rainfallInterval = GenOptions.gridErosion_rainfallInterval;
	float rainfall = GenOptions.gridErosion_rainfall;

	float Kd = GenOptions.gridErosion_depositionConstant;
	float Kc = GenOptions.gridErosion_sedimentCapacity;
	float Ks = GenOptions.gridErosion_soilSoftness;

	TArray<float> water;
	TArray<float> sediment;

	TArray<float> newHeight;
	TArray<float> newWater;
	TArray<float> newSediment;

	for (int32 i = 0; i < HeightData.Num(); i++)
	{
		water.Add(rainfall);
		newWater.Add(0.f);

		sediment.Add(0.f);
		newSediment.Add(0.f);

		newHeight.Add(0.f);
	}

	int32 width = xSections * xSize;
	TArray<int32> deltas;
	deltas.Add(-1);
	deltas.Add(1);
	deltas.Add(-width);
	deltas.Add(width);

	for (int32 e = 0; e < erosionIterations; e++)
	{
		for (int32 i = 0; i < HeightData.Num(); i++)
		{
			for (const int32& delta : deltas)
			{
				//TODO: no wraparound

				int32 ai = i + delta; //Adjacent index
				if (ai < 0 || ai >= HeightData.Num()) continue;

				float waterFlow = FMath::Min(water[i], (water[i] + HeightData[i]) - (water[ai] + HeightData[ai]));

				if (waterFlow <= 0.f)
				{
					float kdsi = Kd * sediment[i];
					newHeight[i] += kdsi;
					newSediment[i] -= kdsi;
				}
				else
				{
					newWater[i] -= waterFlow;
					newWater[ai] += waterFlow;
					float c = Kc * waterFlow;

					if (sediment[i] > c)
					{
						newSediment[ai] += c;
						newHeight[i] += Kd * (sediment[i] - c);
						newSediment[i] = (1.f - Kd) * (sediment[i] - c);
					}
					else
					{
						newSediment[ai] += sediment[i] + Ks * (c - sediment[i]);
						newHeight[i] -= Ks * (c - sediment[i]);
						newSediment[i] = 0.f;
					}
				}
			}
		}

		float additionalRainfall = e % rainfallInterval == 0 ? rainfall : 0.f;

		for (int32 i = 0; i < HeightData.Num(); i++)
		{
			HeightData[i] += FMath::Clamp(newHeight[i], -10.f, 10.f);
			water[i] = newWater[i] + additionalRainfall;
			sediment[i] = newSediment[i];


			newHeight[i] = 0.f;
			newWater[i] = 0.f;
			newSediment[i] = 0.f;
		}
	}
}

void UGenHeight::ParticleBasedErosion()
{
	struct FErsonionParticle
	{
		FVector2D position = FVector2D::ZeroVector;
		FVector2D velocity = FVector2D::ZeroVector;
		float waterVolume = 0.f;
		float sediment = 0.f;
	};

	float Ka = GenOptions.particleErosion_accelerationConsant; //Acceleration constant
	float Kf = GenOptions.particleErosion_frictionConstant; //Friction constant
	float Kc = GenOptions.particleErosion_sedimentCarryingCapacity; //Sediment carrying constant
	float Kd = GenOptions.particleErosion_depositionConstant; //Deposition constant (0-1, inclusive)
	float Ks = GenOptions.particleErosion_soilSoftnessConstant; //Soil softness constant (0-1, inclusive)
	float evaporationRate = GenOptions.particleErosion_evaporationRate;

	int32 erosionIterations = 16384;

	float totalWidth = xSections * xSize;
	float totalHeight = ySections * ySize;

	float angleTolerance = FMath::Cos(FMath::DegreesToRadians(GenOptions.particleErosion_minAngle));

	for (int32 e = 0; e < erosionIterations; e++)
	{
		FErsonionParticle currentParticle;
		currentParticle.waterVolume = GenOptions.particleErosion_waterAmount;
		currentParticle.position = FVector2D(FMath::RandRange(1.f, totalWidth - 2.f), FMath::RandRange(1.f, totalHeight - 2.f));

		while (currentParticle.waterVolume > 0.f)
		{
			FVector normal = GetNormalF(currentParticle.position.X, currentParticle.position.Y);
			if (normal == FVector::ZeroVector) break;

			//Discard if normal is below an angle tolerance (to avoid weird sediment piles on (almost) flat sutfaces)
			if (normal.Dot(FVector::UpVector) > angleTolerance) break;

			currentParticle.velocity += Ka * FVector2D(normal.X, normal.Y);

			currentParticle.velocity *= (1.f - Kf);

			currentParticle.position += currentParticle.velocity;
			if (currentParticle.position.X <= 0.f || currentParticle.position.X >= totalWidth - 1.f || currentParticle.position.Y <= 0.f || currentParticle.position.Y >= totalHeight - 1.f) break;

			float maxSediment = Kc * currentParticle.velocity.Length() * currentParticle.waterVolume;
			if (currentParticle.sediment > maxSediment)
			{
				float excessSediment = currentParticle.sediment - maxSediment;
				excessSediment *= Kd;

				ModifyHeightF(currentParticle.position.X, currentParticle.position.Y, excessSediment);
				currentParticle.sediment -= excessSediment;
			}
			else
			{
				float missingSediment = maxSediment - currentParticle.sediment;
				missingSediment *= Ks;

				ModifyHeightF(currentParticle.position.X, currentParticle.position.Y, -missingSediment);
				currentParticle.sediment += missingSediment;
			}

			currentParticle.waterVolume -= evaporationRate;
		}
	}

	//EnsureSectionsConnect();
}

void UGenHeight::ThermalWeathering()
{
	float T = FMath::DegreesToRadians(15.f);
	float c = .1f;
	int32 iterations = 8;

	TArray<float> newHeight;

	for (int32 i = 0; i < HeightData.Num(); i++)
	{
		newHeight.Add(HeightData[i]);
	}

	int32 width = xSections * xSize;
	TArray<int32> deltas;
	deltas.Add(-1);
	deltas.Add(1);
	deltas.Add(-width);
	deltas.Add(width);

	for (int32 e = 0; e < iterations; e++)
	{
		for (int32 i = 0; i < HeightData.Num(); i++)
		{
			for (const int32& delta : deltas)
			{
				//TODO: no wraparound

				int32 ai = i + delta; //Adjacent index
				if (ai < 0 || ai >= HeightData.Num()) continue;

				if (HeightData[i] - HeightData[ai] > T)
				{
					newHeight[ai] += c * (HeightData[i] - HeightData[ai] - T);
				}
			}
		}

		for (int32 i = 0; i < HeightData.Num(); i++)
		{
			HeightData[i] = newHeight[i];
		}
	}
}

void UGenHeight::GlobalSmooth()
{
	TArray<float> newHeightData = HeightData;

	int32 smoothSize = 2;
	int32 width = xSections * xSize;

	for (uint32 xSec = 0; xSec < xSections; xSec++)
	{
		for (uint32 ySec = 0; ySec < ySections; ySec++)
		{
			for (uint32 y = 0; y < ySize; y++)
			{
				for (uint32 x = 0; x < xSize; x++)
				{
					float heightSum = 0.f;
					int32 neighbors = 0;

					for (int32 dy = -smoothSize; dy <= smoothSize; dy++)
					{
						for (int32 dx = -smoothSize; dx <= smoothSize; dx++)
						{
							//No wraparound
							if (xSec == 0 && int32(x) + dx < 0) continue;
							if (xSec == xSections - 1 && int32(x) + dx >= int32(xSize)) continue;

							int32 newIndex = GetGlobalIndex(xSec, ySec, x, y) + dy * width + dx;
							if (newIndex < 0 || newIndex >= HeightData.Num()) continue;

							heightSum += HeightData[newIndex];
							neighbors++;
						}
					}

					if (neighbors == 0) continue;

					newHeightData[GetGlobalIndex(xSec, ySec, x, y)] = heightSum / float(neighbors);
				}
			}
		}
	}

	for (int32 i = 0; i < HeightData.Num(); i++)
	{
		HeightData[i] = newHeightData[i];
	}
}

void UGenHeight::GetSectionHeight(uint32 sectionIndex, TArray<float>& height)
{
	uint32 xSection = sectionIndex % xSections;
	uint32 ySection = sectionIndex / ySections;

	bool hasXBorder = xSection < xSections - 1;
	bool hasYBorder = ySection < ySections - 1;

	for (uint32 y = 0; y < ySize; y++)
	{
		for (uint32 x = 0; x < xSize; x++)
		{
			height.Add(HeightData[GetGlobalIndex(xSection, ySection, x, y)]);
		}

		if (hasXBorder) height.Add(HeightData[GetGlobalIndex(xSection, ySection, xSize, y)]);
	}

	if (hasYBorder)
	{
		for (uint32 x = 0; x < xSize; x++)
		{
			height.Add(HeightData[GetGlobalIndex(xSection, ySection, x, ySize)]);
		}

		if (hasXBorder) height.Add(HeightData[GetGlobalIndex(xSection, ySection, xSize, ySize)]);
	}
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
	HeightmapTexture->Filter = TextureFilter::TF_Nearest;
	
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

uint32 UGenHeight::GetGlobalIndex(uint32 currentXSection, uint32 currentYSection, uint32 currentXPosition, uint32 currentYPosition)
{
	return currentYSection * xSections * xSize * ySize + currentYPosition * xSections * xSize + currentXSection * xSize + currentXPosition;
}

uint32 UGenHeight::GetGlobalIndex(uint32 globalXPosition, uint32 globalYPosition)
{
	uint32 width = xSections * xSize;
	return globalYPosition * width + globalXPosition;
}

FVector UGenHeight::GetNormal(int32 globalIndex)
{
	int32 width = xSections * xSize;
	float current = HeightData[globalIndex];

	float left = globalIndex > 0 ? HeightData[globalIndex - 1] : current;
	float right = globalIndex < HeightData.Num() - 1 ? HeightData[globalIndex + 1] : current;
	float top = globalIndex - width >= 0 ? HeightData[globalIndex - width] : current;
	float bottom = globalIndex + width < HeightData.Num() ? HeightData[globalIndex + width] : current;

	return -FVector(2.f * (right - left), 2.f * (bottom - top), -4.f).GetSafeNormal();
}

void UGenHeight::GetPositionRangeF(float xPos, float yPos, int32& outXMin, int32& outXMax, int32& outYMin, int32& outYMax)
{
	outXMin = FMath::FloorToInt32(xPos);
	outXMax = FMath::CeilToInt32(xPos);
	outYMin = FMath::FloorToInt32(yPos);
	outYMax = FMath::CeilToInt32(yPos);

	//Check out of bounds
	outXMin = FMath::Clamp(outXMin, 0, xSections * xSize);
	outXMax = FMath::Clamp(outXMax, outXMin, xSections * xSize);
	outYMin = FMath::Clamp(outYMin, 0, ySections * ySize);
	outYMax = FMath::Clamp(outYMax, outYMin, ySections * ySize);
}

FVector UGenHeight::GetNormalF(float xPos, float yPos)
{
	FVector normal = FVector::ZeroVector;

	int32 xMin, xMax, yMin, yMax;
	GetPositionRangeF(xPos, yPos, xMin, xMax, yMin, yMax);

	for (int32 y = yMin; y <= yMax; y++)
	{
		float yDiff = 1.f - FMath::Abs(yPos - float(y));
		for (int32 x = xMin; x <= xMax; x++)
		{
			float xDiff = 1.f - FMath::Abs(xPos - float(x));

			normal += GetNormal(GetGlobalIndex(x, y)) * yDiff * xDiff;
		}
	}

	return normal;
}

void UGenHeight::ModifyHeightF(float xPos, float yPos, float diff)
{
	int32 xMin, xMax, yMin, yMax;
	GetPositionRangeF(xPos, yPos, xMin, xMax, yMin, yMax);

	for (int32 y = yMin; y <= yMax; y++)
	{
		float yDiff = 1.f - FMath::Abs(yPos - float(y));
		for (int32 x = xMin; x <= xMax; x++)
		{
			float xDiff = 1.f - FMath::Abs(xPos - float(x));

			HeightData[GetGlobalIndex(x, y)] += yDiff * xDiff * diff;
		}
	}
}
