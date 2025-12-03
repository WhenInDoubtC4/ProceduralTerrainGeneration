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

void UGenHeight::Initialize(uint32 xSectionCount, uint32 ySectionCount, uint32 sectionWidth, uint32 sectionHeight, float edgeSize)
{
	xSections = xSectionCount;
	ySections = ySectionCount;
	xSize = sectionWidth;
	ySize = sectionHeight;
	vertexSize = edgeSize;

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
	if (EnableOptimizations) GridBasedErosion_Intrin();
	else GridBasedErosion_Impl();
}

void UGenHeight::GridBasedErosion_Impl()
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

void UGenHeight::GridBasedErosion_Intrin()
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
	//TArray<int32> deltas;
	//deltas.Add(-1);
	//deltas.Add(1);
	//deltas.Add(-width);
	//deltas.Add(width);

	__m128i deltas = _mm_setr_epi32(-1, 1, -width, width);

	for (int32 e = 0; e < erosionIterations; e++)
	{
		for (int32 i = 0; i < HeightData.Num(); i++)
		{
			__m128i adjacentIndices = _mm_set1_epi32(i);
			adjacentIndices = _mm_add_epi32(adjacentIndices, deltas);

			//Clamp between 0 and HeightData.Num
			adjacentIndices = _mm_max_epi32(_mm_set1_epi32(0), adjacentIndices);
			adjacentIndices = _mm_min_epi32(_mm_set1_epi32(HeightData.Num() - 1), adjacentIndices);

			__m128 currentWater = _mm_set1_ps(water[i]);
			__m128 currentHeight = _mm_set1_ps(HeightData[i]);
			__m128 currentLevel = _mm_add_ps(currentWater, currentHeight);

			__m128 adjacentWaterLevel = _mm_setr_ps(water[adjacentIndices.m128i_i32[0]], water[adjacentIndices.m128i_i32[1]], water[adjacentIndices.m128i_i32[2]], water[adjacentIndices.m128i_i32[3]]);
			__m128 adjacentHeight = _mm_setr_ps(HeightData[adjacentIndices.m128i_i32[0]], HeightData[adjacentIndices.m128i_i32[1]], HeightData[adjacentIndices.m128i_i32[2]], HeightData[adjacentIndices.m128i_i32[3]]);
			adjacentWaterLevel = _mm_add_ps(adjacentWaterLevel, adjacentHeight); //water[ai4] + HeightData[ai4]

			__m128 waterFlow = _mm_sub_ps(currentLevel, adjacentWaterLevel); //for each delta
			waterFlow = _mm_min_ps(currentWater, waterFlow);

			__m128 waterFlowAlpha = mm128_is_negative(waterFlow); 

			//waterFlowAlpha 0 if waterFlow > 0 -- HIGH WATER
				__m128 di_newWater_highWater = _mm_mul_ps(_mm_set1_ps(-1.f), waterFlow);
				__m128 dai_newWater_highWater = waterFlow;
			
				__m128 c = _mm_mul_ps(_mm_set1_ps(Kc), waterFlow);
				__m128 sedimentLevel = _mm_sub_ps(_mm_set1_ps(sediment[i]), c);
				__m128 sedimentAlpha = mm128_is_negative(sedimentLevel);
			
					//sedimentAlpha 0 if sediment > c -- HIGH SEDIMENT
					__m128 dai_newSediment_highSediment = c;
					__m128 di_newHeight_highSediment = _mm_mul_ps(_mm_set1_ps(Kd), sedimentLevel);
					__m128 di_newSediment_highSediment = _mm_mul_ps(_mm_set1_ps(1.f - Kd), sedimentLevel);
					//sedimentAlpha 1 if sediment < c -- LOW SEDIMENT
					__m128 dai_newSediment_lowSediment = _mm_mul_ps(_mm_set1_ps(Ks), sedimentLevel);
					dai_newSediment_lowSediment = _mm_add_ps(dai_newSediment_lowSediment, _mm_set1_ps(sediment[i]));
					__m128 di_newHeight_lowSediment = _mm_mul_ps(_mm_set1_ps(-Ks), sedimentLevel);
					__m128 i_newSediment_lowSediment = _mm_set1_ps(0.f);

				__m128 dai_newSediment = mm128_lerp(dai_newSediment_highSediment, dai_newSediment_lowSediment, sedimentAlpha);
				__m128 di_newHeight = mm128_lerp(di_newHeight_highSediment, di_newHeight_lowSediment, sedimentAlpha);
				__m128 di_newSediment = mm128_lerp(di_newSediment_highSediment, i_newSediment_lowSediment, sedimentAlpha);

			//waterFlowAlpha 1 if waterFlow < 0 -- LOW WATER
			float kdsi = Kd* sediment[i];
			__m128 di_newHeight_lowWater = _mm_set1_ps(kdsi);
			__m128 di_newSediment_lowWater = _mm_set1_ps(-kdsi);

			//lerp(highwater, lowwater)
			__m128 di_newHeight_final = mm128_lerp(di_newHeight, di_newHeight_lowWater, waterFlowAlpha);
			__m128 dai_newWater_final = mm128_lerp(dai_newWater_highWater, _mm_set1_ps(0.f), waterFlowAlpha);
			__m128 dai_newSediment_final = mm128_lerp(dai_newSediment, _mm_set1_ps(0.f), waterFlowAlpha);
			__m128 di_newSediment_final = mm128_lerp(di_newSediment, di_newSediment_lowWater, waterFlowAlpha);

			//apply the deltas
			di_newHeight_final = _mm_hadd_ps(di_newHeight_final, di_newHeight_final);
			di_newHeight_final = _mm_hadd_ps(di_newHeight_final, di_newHeight_final);
			newHeight[i] += di_newHeight_final.m128_f32[0];

			di_newSediment_final = _mm_hadd_ps(di_newSediment_final, di_newSediment_final);
			di_newSediment_final = _mm_hadd_ps(di_newSediment_final, di_newSediment_final);
			newSediment[i] += di_newSediment_final.m128_f32[0];

			for (int f = 0; f < 4; f++) newWater[adjacentIndices.m128i_i32[f]] += dai_newWater_final.m128_f32[f];

			for (int f = 0; f < 4; f++) newSediment[adjacentIndices.m128i_i32[f]] += dai_newSediment_final.m128_f32[f];

			//for (const int32& delta : deltas)
			//{
			//	//TODO: no wraparound

			//	int32 ai = i + delta; //Adjacent index
			//	if (ai < 0 || ai >= HeightData.Num()) continue;

			//	float waterFlow = FMath::Min(water[i], (water[i] + HeightData[i]) - (water[ai] + HeightData[ai]));

			//	if (waterFlow <= 0.f) -- LOW WATER
			//	{
			//		float kdsi = Kd * sediment[i];
			//		newHeight[i] += kdsi;
			//		newSediment[i] -= kdsi;
			//	}
			//	else -- HIGH WATER
			//	{
			//		newWater[i] -= waterFlow;
			//		newWater[ai] += waterFlow;
			//		float c = Kc * waterFlow;

			//		if (sediment[i] > c)
			//		{
			//			newSediment[ai] += c;
			//			newHeight[i] += Kd * (sediment[i] - c);
			//			newSediment[i] += (1.f - Kd) * (sediment[i] - c);
			//		}
			//		else
			//		{
			//			newSediment[ai] += sediment[i] + Ks * (c - sediment[i]);
			//			newHeight[i] -= Ks * (c - sediment[i]);
			//			newSediment[i] += 0.f;
			//		}
			//	}
			//}
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
	if (EnableOptimizations) ParticleBasedErosion_Intrin();
	else ParticleBasedErosion_Impl();
}

void UGenHeight::ParticleBasedErosion_Impl()
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
			FVector normal3 = GetNormalF(currentParticle.position.X, currentParticle.position.Y);
			FVector2D normal(normal3.X, normal3.Y);

			//Discard if normal is below an angle tolerance (to avoid weird sediment piles on (almost) flat sutfaces)
			//if (normal.Dot(FVector::UpVector) > angleTolerance) break;
			//UE_LOG(LogTemp, Warning, TEXT("%f"), normal.GetAbs().Dot(FVector::UpVector));

			currentParticle.velocity += Ka * normal;

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

void UGenHeight::ParticleBasedErosion_Intrin()
{
	//struct FErsonionParticle
	//{
	//	FVector2D position = FVector2D::ZeroVector;
	//	FVector2D velocity = FVector2D::ZeroVector;
	//	float waterVolume = 0.f;
	//	float sediment = 0.f;
	//};

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

	for (int32 e = 0; e < erosionIterations; e += 4)
	{
		__m256 currentVelocity = _mm256_set1_ps(0.f);

		float rp[8];
		for (int p = 0; p < 8; p++) rp[p] = FMath::RandRange(1.f, totalWidth - 2.f);
		__m256 currentPosition = _mm256_setr_ps(rp[0], rp[1], rp[2], rp[3], rp[4], rp[5], rp[6], rp[7]);

		__m128 currentWaterVolume = _mm_set1_ps(GenOptions.particleErosion_waterAmount * .25f);
		__m128 currentSediment = _mm_set1_ps(0.f);

		while (mm128_sum(currentWaterVolume) > 0.f)
		{
			FVector np[4];
			for (int p = 0; p < 4; p++) np[p] = GetNormalF(currentPosition.m256_f32[p * 2], currentPosition.m256_f32[p * 2 + 1]);

			__m256 normal = _mm256_setr_ps(np[0].X, np[0].Y, np[1].X, np[1].Y, np[2].X, np[2].Y, np[3].X, np[3].Y);

			__m256 deltaVelocity = _mm256_mul_ps(_mm256_set1_ps(Ka), normal);
			currentVelocity = _mm256_add_ps(currentVelocity, deltaVelocity);
			currentVelocity = _mm256_mul_ps(currentVelocity, _mm256_set1_ps(1.f - Kf));

			currentPosition = _mm256_add_ps(currentPosition, currentVelocity);
			currentPosition = _mm256_min_ps(_mm256_set1_ps(totalWidth - 1.f), currentPosition);
			currentPosition = _mm256_max_ps(_mm256_set1_ps(0.f), currentPosition);

			__m256 currentVelocityDot = _mm256_pow_ps(currentVelocity, _mm256_set1_ps(2.f));
			currentVelocityDot = _mm256_hadd_ps(currentVelocityDot, currentVelocityDot);
			__m128 currentSpeed = _mm256_extractf32x4_ps(currentVelocityDot, 0);
			currentSpeed = _mm_sqrt_ps(currentSpeed);

			__m128 maxSediment = _mm_mul_ps(currentSpeed, currentWaterVolume);
			maxSediment = _mm_mul_ps(maxSediment, _mm_set_ps1(Kc));

			__m128 missingSediment = _mm_sub_ps(maxSediment, currentSediment);

			__m128 missingSedimentAlpha = mm128_is_negative(missingSediment);
			//0 if maxSediment > currentSediment - MISSING
			missingSediment = _mm_mul_ps(_mm_set1_ps(Ks), missingSediment); //positive
		
			//1 if maxSediment < currentSediment - EXCESS
			__m128 excessSediment = _mm_mul_ps(_mm_set1_ps(Kd), missingSediment); //negative


			__m128 dSediment = mm128_lerp(missingSediment, excessSediment, missingSedimentAlpha);
			currentSediment = _mm_add_ps(currentSediment, dSediment);
			for (int p = 0; p < 4; p++) ModifyHeightF(currentPosition.m256_f32[p * 2], currentPosition.m256_f32[p * 2 + 1], -currentSediment.m128_f32[p]);

			currentWaterVolume = _mm_sub_ps(currentWaterVolume, _mm_set1_ps(evaporationRate));
		}
	}
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

bool UGenHeight::HeightfieldCast(float xPos, float yPos, float& outHeight, FVector& outNormal)
{
	float localx = xPos / vertexSize;
	float localy = yPos / vertexSize;

	if (localx < 0.f || localy < 0.f || localx + 1.f >= float(xSections * xSize) || localy + 1.f >= float(ySections * ySize)) return false;

	outHeight = GetHeightF(localx, localy);
	outNormal = GetNormalF(localx, localy);

	return true;
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
	float result = 0.f;

	//Step1
	FVector2D step1Position = position;

	step1Position *= GenOptions.step1Period;
	step1Position += .1f * FVector2D::UnitVector; //Add a fraction here as this could produce bad results with integer values
	step1Position += FMath::Fmod(GenOptions.seed, 5000.f) * FVector2D::UnitVector;

	float step1Value = FMath::PerlinNoise2D(step1Position) * GenOptions.step1Amplitude;
	result += step1Value;

	//Step2
	FVector2D step2Position = position;

	step2Position *= GenOptions.step2Period;
	step2Position += .1f * FVector2D::UnitVector;

	float step2Value = FMath::PerlinNoise2D(step2Position) * GenOptions.step2Amplitude;
	result += step2Value;

	//Island modifier
	if (!GenOptions.islandModifier) return result;

	float midx = xSections * xSize * 0.5f;
	float midy = ySections * ySize * 0.5f;

	//Square gradient using chebyshev distance
	float distance = FMath::Max(FMath::Abs(position.X - midx), FMath::Abs(position.Y - midy));
	float distanceRatio = 1.f - (distance / FMath::Max(midx, midy));

	distanceRatio += GenOptions.islandGradientOffset;
	distanceRatio *= GenOptions.islandGradientContrast;

	distanceRatio = FMath::Clamp(distanceRatio, 0.f, 1.f);

	//Above water level
	result += GenOptions.islandWaterLevelOffset;

	//Only affect positive (above water level) values
	if (result > -200.f) result *= distanceRatio;

	result -= 1000.f; //So that edges don't stick out above the water

	return result;
}

float UGenHeight::NormalizeHeightValue(float heightValue)
{
	if (GenOptions.islandModifier)
	{
		heightValue += 1000.f;
		heightValue -= GenOptions.islandWaterLevelOffset;
	}

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

float UGenHeight::GetHeightF(float xPos, float yPos)
{
	float result = 0.f;

	int32 xMin, xMax, yMin, yMax;
	GetPositionRangeF(xPos, yPos, xMin, xMax, yMin, yMax);

	for (int32 y = yMin; y <= yMax; y++)
	{
		float yDiff = 1.f - FMath::Abs(yPos - float(y));
		for (int32 x = xMin; x <= xMax; x++)
		{
			float xDiff = 1.f - FMath::Abs(xPos - float(x));

			result += HeightData[GetGlobalIndex(x, y)] * yDiff * xDiff;
		}
	}

	return result;
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

	return normal.GetSafeNormal();
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
