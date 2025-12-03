// Fill out your copyright notice in the Description page of Project Settings.


#include "GenWorld.h"

// Sets default values
AGenWorld::AGenWorld()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* root = CreateDefaultSubobject<USceneComponent>("Root");
	SetRootComponent(root);

	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>("Terrain Mesh");
	TerrainMesh->AttachToComponent(root, FAttachmentTransformRules::KeepRelativeTransform);

	HeightGenerator = CreateDefaultSubobject<UGenHeight>("Height Generator");
	FoliageGenerator = CreateDefaultSubobject<UGenFoliage>("Foliage generator");
	GenerationStats = CreateDefaultSubobject<UGenStats>("Generation statistics");

	HeightGenCounter = GenerationStats->AddCounter(TEXT("HeightmapGen"));
	TBNCalcCounter = GenerationStats->AddCounter(TEXT("TBNCalculation"));
	ErosionCounter = GenerationStats->AddCounter(TEXT("Erosion"));
}

// Called when the game starts or when spawned
void AGenWorld::BeginPlay()
{
	Super::BeginPlay();

	TerrainSectionReady.AddUObject(this, &AGenWorld::OnNextSectionReady);
	AllTerrainSectionsReady.AddUObject(this, &AGenWorld::CalculateTerrainTBN);

	//Do not start generating right after startup
	//GenerateTerrain();
}

// Called every frame
void AGenWorld::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AGenWorld::GenerateTerrain()
{
	BatchGenerationEnabled = false;

	FoliageGenerator->Clear();
	TerrainMesh->ClearAllMeshSections();

	GenerationStats->ResetAllCounters(true);

	HeightGenerator->SetEnableOptimizations(GenOptions.enableOptimizations);
	HeightGenerator->Initialize(GenOptions.xSections, GenOptions.ySections, GenOptions.xVertexCount, GenOptions.yVertexCount, GenOptions.edgeSize);

	for (int32 y = 0; y < GenOptions.ySections; y++)
	{
		for (int32 x = 0; x < GenOptions.xSections; x++)
		{
			SectionQueue.Enqueue({x, y});
		}
	}

	HeightGenCounter->Start();
	GenerateNextSection();
}

void AGenWorld::BatchGenerate(int32 count)
{
	BatchIndex = count;

	//Precompute seeds (do not reset)
	if (BatchSeeds.Num() != count)
	{
		for (int32 i = 0; i < count; i++)
		{
			BatchSeeds.Add(FMath::RandRange(0.f, 9999.f));
		}
	}

	FHeightGeneratorOptions newSeedOptions = HeightGenerator->GetGenerationOptions();
	newSeedOptions.seed = BatchSeeds[BatchIndex - 1];
	HeightGenerator->SetGenerationOptions(newSeedOptions);

	GenerateTerrain();
	BatchGenerationEnabled = true;
}

void AGenWorld::UpdateMaterial(FTerrainMaterialOptions options)
{
	auto newMaterial = UMaterialInstanceDynamic::Create(terrainMaterial, this);

	newMaterial->SetScalarParameterValue("Ice", options.iceEnable ? 1.f : 0.f);
	newMaterial->SetScalarParameterValue("Ice blend contrast", options.iceBlendContrast);
	newMaterial->SetScalarParameterValue("Ice height", options.iceHeight);
	newMaterial->SetScalarParameterValue("Ice noise blend contrast", options.iceNoiseBlendContrast);
	newMaterial->SetScalarParameterValue("Ice noise scale", options.iceNoiseScale);

	newMaterial->SetScalarParameterValue("Rock blend power", options.rockBlendPower);
	newMaterial->SetScalarParameterValue("Rock slope angle", options.rockSlopeAngle);

	newMaterial->SetScalarParameterValue("Beach blend contrast", options.beachBlendContrast);
	newMaterial->SetScalarParameterValue("Beach blend noise contrast", options.beachBlendNoiseContrast);
	newMaterial->SetScalarParameterValue("Beach blend noise scale", options.beachBlendNoiseScale);
	newMaterial->SetScalarParameterValue("Water level", options.beachWaterLevel);

	for (int32 i = 0; i < GenOptions.xSections * GenOptions.ySections; i++)
	{
		TerrainMesh->SetMaterial(i, newMaterial);
	}
}

void AGenWorld::UpdateFoliage()
{
	FoliageGenerator->Clear();

	if (!GenFoliage) return;

	FoliageGenerator->Spawn(HeightGenerator, FoliageGenOptions);
}

void AGenWorld::CalculateSectionTBN(const TArray<FVector>& secVertices, const TArray<int32>& secIndices, const TArray<FVector2D>& secUVs, TArray<FVector>& outNormals, TArray<FProcMeshTangent>& outTangents)
{
	if (GenOptions.enableOptimizations) CalculateSectionTBN_Intrin(secVertices, secIndices, secUVs, outNormals, outTangents);
	else CalculateSectionTBN_Impl(secVertices, secIndices, secUVs, outNormals, outTangents);
}

void AGenWorld::CalculateSectionTBN_Impl(const TArray<FVector>& secVertices, const TArray<int32>& secIndices, const TArray<FVector2D>& secUVs, TArray<FVector>& outNormals, TArray<FProcMeshTangent>& outTangents)
{
	TArray<FVector> intTangents;
	TArray<FVector> intNormals;

	for (int32 i = 0; i < secVertices.Num(); i++)
	{
		intNormals.Add(FVector::ZeroVector);
		intTangents.Add(FVector::ZeroVector);
	}

	for (int32 i = 0; i < secIndices.Num(); i += 3)
	{
		FVector pos0 = secVertices[secIndices[i]];
		FVector pos1 = secVertices[secIndices[i + 1]];
		FVector pos2 = secVertices[secIndices[i + 2]];

		FVector2D tex0 = secUVs[secIndices[i]];
		FVector2D tex1 = secUVs[secIndices[i + 1]];
		FVector2D tex2 = secUVs[secIndices[i + 2]];

		FVector edge1 = pos1 - pos0;
		FVector edge2 = pos2 - pos0;

		FVector2D uv1 = tex1 - tex0;
		FVector2D uv2 = tex2 - tex0;

		float r = 1.0f / (uv1.X * uv2.Y - uv1.Y * uv2.X);

		FVector normal = edge2 ^ edge1;

		FVector tangent;
		tangent.X = ((edge1.X * uv2.Y) - (edge2.X * uv1.Y)) * r;
		tangent.Y = ((edge1.Y * uv2.Y) - (edge2.Y * uv1.Y)) * r;
		tangent.Z = ((edge1.Z * uv2.Y) - (edge2.Z * uv1.Y)) * r;

		intTangents[secIndices[i]] += tangent;
		intTangents[secIndices[i + 1]] += tangent;
		intTangents[secIndices[i + 2]] += tangent;

		intNormals[secIndices[i]] += normal;
		intNormals[secIndices[i + 1]] += normal;
		intNormals[secIndices[i + 2]] += normal;
	}

	for (int32 i = 0; i < secVertices.Num(); i++)
	{
		FVector n = intNormals[i].GetSafeNormal();
		outNormals.Add(n);

		FVector t0 = intTangents[i];

		FVector t = t0 - (n * FVector::DotProduct(n, t0));

		outTangents.Add(FProcMeshTangent(t.GetSafeNormal(), false));
	}
}

void AGenWorld::CalculateSectionTBN_Intrin(const TArray<FVector>& secVertices, const TArray<int32>& secIndices, const TArray<FVector2D>& secUVs, TArray<FVector>& outNormals, TArray<FProcMeshTangent>& outTangents)
{
	TArray<FVector> intTangents;
	TArray<FVector> intNormals;

	for (int32 i = 0; i < secVertices.Num(); i++)
	{
		intNormals.Add(FVector::ZeroVector);
		intTangents.Add(FVector::ZeroVector);
	}

	for (int32 i = 0; i < secIndices.Num(); i += 3)
	{
		__m128i indices = _mm_set1_epi32(i);
		__m128i indexOffsets = _mm_setr_epi32(0, 1, 2, 0);
		indices = _mm_add_epi32(indices, indexOffsets);

		FVector pos0 = secVertices[secIndices[indices.m128i_i32[0]]];
		FVector pos1 = secVertices[secIndices[indices.m128i_i32[1]]];
		FVector pos2 = secVertices[secIndices[indices.m128i_i32[2]]];

		FVector2D tex0 = secUVs[secIndices[indices.m128i_i32[0]]];
		FVector2D tex1 = secUVs[secIndices[indices.m128i_i32[1]]];
		FVector2D tex2 = secUVs[secIndices[indices.m128i_i32[2]]];

		__m256 pos12 = _mm256_setr_ps(pos1.X, pos1.Y, pos1.Z, 0, pos2.X, pos2.Y, pos2.Z, 0);
		__m256 pos00 = _mm256_setr_ps(pos0.X, pos0.Y, pos0.Z, 0, pos0.X, pos0.Y, pos0.Z, 0);
		__m256 edge12 = _mm256_sub_ps(pos12, pos00);

		__m128 edge1 = _mm256_extractf128_ps(edge12, 0);
		__m128 edge2 = _mm256_extractf128_ps(edge12, 1);

		__m128 tex12 = _mm_setr_ps(tex1.X, tex1.Y, tex2.X, tex2.Y);
		__m128 tex00 = _mm_setr_ps(tex0.X, tex0.Y, tex0.X, tex0.Y);
		__m128 uv12 = _mm_sub_ps(tex12, tex00);
		__m128 uv12_reversed = _mm_shuffle_ps(uv12, uv12, _MM_SHUFFLE(0, 1, 2, 3));

		__m128 r = _mm_mul_ps(uv12, uv12_reversed);
		r = _mm_hsub_ps(r, r);
		r = _mm_rcp_ss(r);
		r = _mm_broadcastss_ps(r);

		__m128 normal = mm128_cross_product(edge2, edge1);

		__m128 uv2y = _mm_shuffle_ps(uv12, uv12, _MM_SHUFFLE(0, 0, 0, 0));
		__m128 uv1y = _mm_shuffle_ps(uv12, uv12, _MM_SHUFFLE(2, 2, 2, 2));

		edge1 = _mm_mul_ps(edge1, uv2y);
		edge2 = _mm_mul_ps(edge2, uv1y);
		__m128 tangent = _mm_sub_ps(edge1, edge2);
		tangent = _mm_mul_ps(tangent, r);

		FVector normalVector(normal.m128_f32[0], normal.m128_f32[1], normal.m128_f32[2]);
		FVector tangentVector(tangent.m128_f32[0], tangent.m128_f32[1], tangent.m128_f32[2]);

		intTangents[secIndices[indices.m128i_i32[0]]] += tangentVector;
		intTangents[secIndices[indices.m128i_i32[1]]] += tangentVector;
		intTangents[secIndices[indices.m128i_i32[2]]] += tangentVector;

		intNormals[secIndices[indices.m128i_i32[0]]] += normalVector;
		intNormals[secIndices[indices.m128i_i32[1]]] += normalVector;
		intNormals[secIndices[indices.m128i_i32[2]]] += normalVector;
	}

	for (int32 i = 0; i < secVertices.Num(); i++)
	{
		FVector n = intNormals[i].GetSafeNormal();
		outNormals.Add(n);

		FVector t0 = intTangents[i];

		FVector t = t0 - (n * FVector::DotProduct(n, t0));

		outTangents.Add(FProcMeshTangent(t.GetSafeNormal(), false));
	}
}

void AGenWorld::GenerateNextSection()
{
	if (GenOptions.enableOptimizations) GenerateNextSection_Intrin();
	else GenerateNextSection_Impl();
}

void AGenWorld::GenerateNextSection_Impl()
{
	vertices.Empty();
	triangles.Empty();
	uvs.Empty();
	//normals.Empty();
	//tangents.Empty();

	//All sections generated
	if (SectionQueue.IsEmpty())
	{
		HeightGenerator->DrawTexture();
		AllTerrainSectionsReady.Broadcast();
		return;
	}

	TPair<uint32, uint32> section;
	SectionQueue.Dequeue(section);

	int32 xSection = section.Key;
	int32 ySection = section.Value;

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [=, this]()
	{
		FPlatformProcess::Sleep(.1f);

		sectionIndex = ySection * GenOptions.ySections + xSection;

		TArray<float> heightData;
		HeightGenerator->GenerateHeight(xSection, ySection, heightData);

		bool hasXBorder = xSection < GenOptions.xSections - 1;
		bool hasYBorder = ySection < GenOptions.ySections - 1;

		//Create vertex and UV arrays
		for (int32 y = 0; y < GenOptions.yVertexCount; y++)
		{
			for (int32 x = 0; x < GenOptions.xVertexCount; x++)
			{
				float xValue = x * GenOptions.edgeSize;
				float yValue = y * GenOptions.edgeSize;

				xValue += xSection * (GenOptions.xVertexCount) * GenOptions.edgeSize;
				yValue += ySection * (GenOptions.yVertexCount) * GenOptions.edgeSize;

				float heightValue = heightData[y * GenOptions.xVertexCount + x];
				//float heightValue = 0.f;

				FVector newVertex(xValue, yValue, heightValue);
				vertices.Add(newVertex);

				FVector2D uvCoord((float)x, (float)y);
				uvs.Add(uvCoord);
			}

			if (hasXBorder)
			{
				FVector borderVertex;
				borderVertex.X = GenOptions.xVertexCount * GenOptions.edgeSize + xSection * (GenOptions.xVertexCount) * GenOptions.edgeSize;
				borderVertex.Y = y * GenOptions.edgeSize + ySection * (GenOptions.yVertexCount) * GenOptions.edgeSize;
				borderVertex.Z = heightData[y * GenOptions.xVertexCount + GenOptions.xVertexCount - 1];

				vertices.Add(borderVertex);

				FVector2D uvCoord(GenOptions.xVertexCount, (float)y);
				uvs.Add(uvCoord);
			}
		}

		if (hasYBorder)
		{
			for (int32 x = 0; x < GenOptions.xVertexCount; x++)
			{
				float xValue = x * GenOptions.edgeSize;
				float yValue = GenOptions.yVertexCount * GenOptions.edgeSize;

				xValue += xSection * (GenOptions.xVertexCount) * GenOptions.edgeSize;
				yValue += ySection * (GenOptions.yVertexCount) * GenOptions.edgeSize;

				float heightValue = heightData[(GenOptions.yVertexCount - 1) * GenOptions.xVertexCount + x];

				FVector newVertex(xValue, yValue, heightValue);
				vertices.Add(newVertex);

				FVector2D uvCoord((float)x, (float)GenOptions.yVertexCount);
				uvs.Add(uvCoord);
			}

			if (hasXBorder)
			{
				FVector borderVertex;
				borderVertex.X = GenOptions.xVertexCount * GenOptions.edgeSize + xSection * (GenOptions.xVertexCount) * GenOptions.edgeSize;
				borderVertex.Y = GenOptions.yVertexCount * GenOptions.edgeSize + ySection * (GenOptions.yVertexCount) * GenOptions.edgeSize;
				borderVertex.Z = heightData[(GenOptions.yVertexCount - 1) * GenOptions.xVertexCount + GenOptions.xVertexCount - 1];

				vertices.Add(borderVertex);

				FVector2D uvCoord(GenOptions.xVertexCount, (float)GenOptions.yVertexCount);
				uvs.Add(uvCoord);
			}
		}

		//Create triangles (ccw winding order)
		for (int32 y = 0; y < GenOptions.yVertexCount - 1; y++)
		{
			for (int32 x = 0; x < GenOptions.xVertexCount - 1; x++)
			{
				int32 startIndex = y * GenOptions.xVertexCount + x;

				if (hasXBorder) startIndex += y;

				int32 offset = hasXBorder ? 1 : 0;

				triangles.Add(startIndex); //(0,0)
				triangles.Add(startIndex + GenOptions.xVertexCount + offset); //(0, 1)
				triangles.Add(startIndex + 1); //(1,0)

				triangles.Add(startIndex + GenOptions.xVertexCount + offset); //(0, 1)
				triangles.Add(startIndex + GenOptions.xVertexCount + 1 + offset); // (1, 1)
				triangles.Add(startIndex + 1); //(1, 0)
			}

			if (hasXBorder)
			{
				int32 startIndex = y * GenOptions.xVertexCount + (GenOptions.xVertexCount - 1) + y;

				triangles.Add(startIndex); //(0,0)
				triangles.Add(startIndex + GenOptions.xVertexCount + 1); //(0, 1)
				triangles.Add(startIndex + 1); //(1,0)

				triangles.Add(startIndex + GenOptions.xVertexCount + 1); //(0, 1)
				triangles.Add(startIndex + GenOptions.xVertexCount + 2); // (1, 1)
				triangles.Add(startIndex + 1); //(1, 0)
			}
		}

		if (hasYBorder)
		{
			for (int32 x = 0; x < GenOptions.xVertexCount - 1; x++)
			{
				int32 startIndex = (GenOptions.yVertexCount - 1) * GenOptions.xVertexCount + x;

				if (hasXBorder) startIndex += GenOptions.yVertexCount - 1;

				int32 offset = hasXBorder ? 1 : 0;

				triangles.Add(startIndex); //(0,0)
				triangles.Add(startIndex + GenOptions.xVertexCount + offset); //(0, 1)
				triangles.Add(startIndex + 1); //(1,0)

				triangles.Add(startIndex + GenOptions.xVertexCount + offset); //(0, 1)
				triangles.Add(startIndex + GenOptions.xVertexCount + 1 + offset); // (1, 1)
				triangles.Add(startIndex + 1); //(1, 0)
			}

			if (hasXBorder)
			{
				int32 startIndex = (GenOptions.yVertexCount - 1) * GenOptions.xVertexCount + (GenOptions.xVertexCount - 1) + GenOptions.yVertexCount - 1;

				triangles.Add(startIndex); //(0,0)
				triangles.Add(startIndex + GenOptions.xVertexCount + 1); //(0, 1)
				triangles.Add(startIndex + 1); //(1,0)

				triangles.Add(startIndex + GenOptions.xVertexCount + 1); //(0, 1)
				triangles.Add(startIndex + GenOptions.xVertexCount + 2); // (1, 1)
				triangles.Add(startIndex + 1); //(1, 0)
			}
		}

		AsyncTask(ENamedThreads::GameThread, [&]()
			{
				TerrainSectionReady.Broadcast();
			});
	});
}

void AGenWorld::GenerateNextSection_Intrin()
{
	vertices.Empty();
	triangles.Empty();
	uvs.Empty();
	//normals.Empty();
	//tangents.Empty();

	//All sections generated
	if (SectionQueue.IsEmpty())
	{
		HeightGenerator->DrawTexture();
		AllTerrainSectionsReady.Broadcast();
		return;
	}

	TPair<uint32, uint32> section;
	SectionQueue.Dequeue(section);

	int32 xSection = section.Key;
	int32 ySection = section.Value;

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [=, this]()
		{
			FPlatformProcess::Sleep(.1f);

			sectionIndex = ySection * GenOptions.ySections + xSection;

			TArray<float> heightData;
			HeightGenerator->GenerateHeight(xSection, ySection, heightData);

			bool hasXBorder = xSection < GenOptions.xSections - 1;
			bool hasYBorder = ySection < GenOptions.ySections - 1;

			//Create vertex and UV arrays
			for (int32 y = 0; y < GenOptions.yVertexCount; y++)
			{
				for (int32 x = 0; x < GenOptions.xVertexCount; x++)
				{
					__m128i xy00_int = _mm_setr_epi32(x, y, 0, 0);
					__m128 xy00 = _mm_cvtepi32_ps(xy00_int);

					__m128 edgeSize = _mm_set1_ps(GenOptions.edgeSize);
					xy00 = _mm_mul_ps(xy00, edgeSize); //(float) [x,y] * edgeSize

					__m128i offset_int = _mm_setr_epi32(xSection, ySection, 0, 0);
					__m128i vertexCount_int = _mm_setr_epi32(GenOptions.xVertexCount, GenOptions.yVertexCount, 0, 0);
					__m128 offset = _mm_cvtepi32_ps(_mm_mullo_epi32(offset_int, vertexCount_int)); //(float) [x,y]section * GenOptions.[x,y]VertexCount
					offset = _mm_mul_ps(offset, edgeSize); // offset *= edgeSize

					xy00 = _mm_add_ps(xy00, offset); // [x,y] += offset

					//float xValue = x * GenOptions.edgeSize;
					//float yValue = y * GenOptions.edgeSize;

					//xValue += xSection * (GenOptions.xVertexCount) * GenOptions.edgeSize;
					//yValue += ySection * (GenOptions.yVertexCount) * GenOptions.edgeSize;

					float heightValue = heightData[y * GenOptions.xVertexCount + x];
					//float heightValue = 0.f;

					FVector newVertex(xy00.m128_f32[0], xy00.m128_f32[1], heightValue);
					vertices.Add(newVertex);

					FVector2D uvCoord((float)x, (float)y);
					uvs.Add(uvCoord);
				}

				if (hasXBorder)
				{
					FVector borderVertex;
					borderVertex.X = GenOptions.xVertexCount * GenOptions.edgeSize + xSection * (GenOptions.xVertexCount) * GenOptions.edgeSize;
					borderVertex.Y = y * GenOptions.edgeSize + ySection * (GenOptions.yVertexCount) * GenOptions.edgeSize;
					borderVertex.Z = heightData[y * GenOptions.xVertexCount + GenOptions.xVertexCount - 1];

					vertices.Add(borderVertex);

					FVector2D uvCoord(GenOptions.xVertexCount, (float)y);
					uvs.Add(uvCoord);
				}
			}

			if (hasYBorder)
			{
				for (int32 x = 0; x < GenOptions.xVertexCount; x++)
				{
					float xValue = x * GenOptions.edgeSize;
					float yValue = GenOptions.yVertexCount * GenOptions.edgeSize;

					xValue += xSection * (GenOptions.xVertexCount) * GenOptions.edgeSize;
					yValue += ySection * (GenOptions.yVertexCount) * GenOptions.edgeSize;

					float heightValue = heightData[(GenOptions.yVertexCount - 1) * GenOptions.xVertexCount + x];

					FVector newVertex(xValue, yValue, heightValue);
					vertices.Add(newVertex);

					FVector2D uvCoord((float)x, (float)GenOptions.yVertexCount);
					uvs.Add(uvCoord);
				}

				if (hasXBorder)
				{
					FVector borderVertex;
					borderVertex.X = GenOptions.xVertexCount * GenOptions.edgeSize + xSection * (GenOptions.xVertexCount) * GenOptions.edgeSize;
					borderVertex.Y = GenOptions.yVertexCount * GenOptions.edgeSize + ySection * (GenOptions.yVertexCount) * GenOptions.edgeSize;
					borderVertex.Z = heightData[(GenOptions.yVertexCount - 1) * GenOptions.xVertexCount + GenOptions.xVertexCount - 1];

					vertices.Add(borderVertex);

					FVector2D uvCoord(GenOptions.xVertexCount, (float)GenOptions.yVertexCount);
					uvs.Add(uvCoord);
				}
			}

			//Create triangles (ccw winding order)
			for (int32 y = 0; y < GenOptions.yVertexCount - 1; y++)
			{
				for (int32 x = 0; x < GenOptions.xVertexCount - 1; x++)
				{
					int32 startIndex = y * GenOptions.xVertexCount + x;

					if (hasXBorder) startIndex += y;

					int32 offset = hasXBorder ? 1 : 0;

					__m128i t1 = _mm_set1_epi32(startIndex);
					__m128i t1Offset = _mm_setr_epi32(0, GenOptions.xVertexCount, GenOptions.xVertexCount + 1, 1);
					__m128i t1Indices = _mm_setr_epi32(0, offset, offset, 0);

					t1Offset = _mm_add_epi32(t1Offset, t1Indices);
					t1 = _mm_add_epi32(t1, t1Offset); //[startIndex, startIndex + GenOptions.xVertexCount + offset, startIndex + GenOptions.xVertexCount + 1 + offset, startIndex + 1]

					triangles.Add(t1.m128i_i32[0]); //(0,0)
					triangles.Add(t1.m128i_i32[1]); //(0, 1)
					triangles.Add(t1.m128i_i32[3]); //(1,0)

					triangles.Add(t1.m128i_i32[1]); //(0, 1)
					triangles.Add(t1.m128i_i32[2]); // (1, 1)
					triangles.Add(t1.m128i_i32[3]); //(1, 0)
				}

				if (hasXBorder)
				{
					int32 startIndex = y * GenOptions.xVertexCount + (GenOptions.xVertexCount - 1) + y;

					triangles.Add(startIndex); //(0,0)
					triangles.Add(startIndex + GenOptions.xVertexCount + 1); //(0, 1)
					triangles.Add(startIndex + 1); //(1,0)

					triangles.Add(startIndex + GenOptions.xVertexCount + 1); //(0, 1)
					triangles.Add(startIndex + GenOptions.xVertexCount + 2); // (1, 1)
					triangles.Add(startIndex + 1); //(1, 0)
				}
			}

			if (hasYBorder)
			{
				for (int32 x = 0; x < GenOptions.xVertexCount - 1; x++)
				{
					int32 startIndex = (GenOptions.yVertexCount - 1) * GenOptions.xVertexCount + x;

					if (hasXBorder) startIndex += GenOptions.yVertexCount - 1;

					int32 offset = hasXBorder ? 1 : 0;

					triangles.Add(startIndex); //(0,0)
					triangles.Add(startIndex + GenOptions.xVertexCount + offset); //(0, 1)
					triangles.Add(startIndex + 1); //(1,0)

					triangles.Add(startIndex + GenOptions.xVertexCount + offset); //(0, 1)
					triangles.Add(startIndex + GenOptions.xVertexCount + 1 + offset); // (1, 1)
					triangles.Add(startIndex + 1); //(1, 0)
				}

				if (hasXBorder)
				{
					int32 startIndex = (GenOptions.yVertexCount - 1) * GenOptions.xVertexCount + (GenOptions.xVertexCount - 1) + GenOptions.yVertexCount - 1;

					triangles.Add(startIndex); //(0,0)
					triangles.Add(startIndex + GenOptions.xVertexCount + 1); //(0, 1)
					triangles.Add(startIndex + 1); //(1,0)

					triangles.Add(startIndex + GenOptions.xVertexCount + 1); //(0, 1)
					triangles.Add(startIndex + GenOptions.xVertexCount + 2); // (1, 1)
					triangles.Add(startIndex + 1); //(1, 0)
				}
			}

			AsyncTask(ENamedThreads::GameThread, [&]()
				{
					TerrainSectionReady.Broadcast();
				});
		});
}

void AGenWorld::OnNextSectionReady()
{
	TerrainMesh->CreateMeshSection(sectionIndex, vertices, triangles, TArray<FVector>(), uvs, TArray<FColor>(), TArray<FProcMeshTangent>(), true);
	if (terrainMaterial) TerrainMesh->SetMaterial(sectionIndex, terrainMaterial);

	GenerateNextSection();
}

void AGenWorld::CalculateTerrainTBN()
{
	HeightGenCounter->Stop();

	for (int32 i = 0; i < GenOptions.xSections * GenOptions.ySections; i++) TBNQueue.Enqueue(i);

	TBNCalcCounter->Start();

	GenerateNextTBN();
}

void AGenWorld::GenerateNextTBN()
{
	/*normals.Empty();
	tangents.Empty();*/

	if (TBNQueue.IsEmpty())
	{
		OnTBNCalculationDone();

		return;
	}

	uint32 nextSectionIndex;
	TBNQueue.Dequeue(nextSectionIndex);

	FProcMeshSection* currentSection = TerrainMesh->GetProcMeshSection(nextSectionIndex);

	/*TArray<FVector> normals;
	TArray<FProcMeshTangent> tangents;*/

	TArray<FVector> extracedVertices;
	TArray<FVector2D> extracedUVs;
	TArray<int32> extracedTriangles;

	for (const FProcMeshVertex& currentVertexData : currentSection->ProcVertexBuffer)
	{
		extracedVertices.Add(currentVertexData.Position);
		extracedUVs.Add(currentVertexData.UV0);
	}

	//Stupid! (but necessary)
	for (const uint32& index : currentSection->ProcIndexBuffer)
	{
		extracedTriangles.Add(index);
	}

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [=, this]
	{
		FPlatformProcess::Sleep(.1f);

		TArray<FVector> normals;
		TArray<FProcMeshTangent> tangents;

		//TERRIBLY slow
		//UKismetProceduralMeshLibrary::CalculateTangentsForMesh(extracedVertices, extracedTriangles, extracedUVs, normals, tangents);

		CalculateSectionTBN(extracedVertices, extracedTriangles, extracedUVs, normals, tangents);

		AsyncTask(ENamedThreads::GameThread, [=, this]
		{
			TerrainMesh->UpdateMeshSection(nextSectionIndex, extracedVertices, normals, extracedUVs, TArray<FColor>(), tangents);
			GenerateNextTBN();
		});
	});
}

void AGenWorld::OnTBNCalculationDone()
{
	TBNCalcCounter->Stop();

	FVector half(GenOptions.xSections * GenOptions.xVertexCount * GenOptions.edgeSize * .5f, GenOptions.ySections * GenOptions.yVertexCount * GenOptions.edgeSize * .5f, 0.f);
	FoliageGenerator->UpdateBounds(half, half + (FVector::UpVector * 20000.f));
	RunGlobalFilters();
}

void AGenWorld::RunGlobalFilters()
{
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [=, this]
	{
		ErosionCounter->Start();
		HeightGenerator->Erode();
		ErosionCounter->Stop();

		AsyncTask(ENamedThreads::GameThread, [=, this]
		{
			HeightGenerator->DrawTexture();
			
			UpdateAllSectionsPost();
		});
	});
}

void AGenWorld::UpdateAllSectionsPost()
{
	for (int32 i = 0; i < GenOptions.xSections * GenOptions.ySections; i++) PostSectionQueue.Enqueue(i);

	UpdateNextSectionPost();
}

void AGenWorld::UpdateNextSectionPost()
{
	if (PostSectionQueue.IsEmpty())
	{
		OnAllSectionsUpdated();
		return;
	}

	uint32 nextSectionIndex;
	PostSectionQueue.Dequeue(nextSectionIndex);

	FProcMeshSection* currentSection = TerrainMesh->GetProcMeshSection(nextSectionIndex);

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [=, this]
	{
		FPlatformProcess::Sleep(.1f);

		//No normals and tangents here, since because the height changed, they will need to be recalculated
		TArray<FVector> vertices;
		TArray<FVector2D> uvs;
		TArray<int32> indices;

		TArray<float> height;

		HeightGenerator->GetSectionHeight(nextSectionIndex, height);

		uint32 j = 0;
		for (const FProcMeshVertex& vertexData : currentSection->ProcVertexBuffer)
		{
			FVector newVertex = vertexData.Position;
			newVertex.Z = height[j++];

			vertices.Add(newVertex);
			uvs.Add(vertexData.UV0);
		}

		for (const uint32& index : currentSection->ProcIndexBuffer)
		{
			indices.Add(index);
		}

		TArray<FVector> normals;
		TArray<FProcMeshTangent> tangents;

		CalculateSectionTBN(vertices, indices, uvs, normals, tangents);

		AsyncTask(ENamedThreads::GameThread, [=, this]
		{
			TerrainMesh->UpdateMeshSection(nextSectionIndex, vertices, normals, uvs, TArray<FColor>(), tangents);

			UpdateNextSectionPost();
		});
	});
}

void AGenWorld::OnAllSectionsUpdated()
{
	UpdateFoliage();

	//All done
	if (!BatchGenerationEnabled || --BatchIndex <= 0)
	{
		FGenStatData resultStats;
		resultStats.heightGenTime = HeightGenCounter->GetSeconds();
		resultStats.tbnCalcTime = TBNCalcCounter->GetSeconds();
		resultStats.erosionTime = ErosionCounter->GetSeconds();

		OnGenerationFinished.Broadcast(resultStats);
	}
	else
	{
		FoliageGenerator->Clear();
		TerrainMesh->ClearAllMeshSections();

		FHeightGeneratorOptions newSeedOptions = HeightGenerator->GetGenerationOptions();
		newSeedOptions.seed = BatchSeeds[BatchIndex - 1];
		HeightGenerator->SetGenerationOptions(newSeedOptions);

		HeightGenerator->SetEnableOptimizations(GenOptions.enableOptimizations);
		HeightGenerator->Initialize(GenOptions.xSections, GenOptions.ySections, GenOptions.xVertexCount, GenOptions.yVertexCount, GenOptions.edgeSize);

		for (int32 y = 0; y < GenOptions.ySections; y++)
		{
			for (int32 x = 0; x < GenOptions.xSections; x++)
			{
				SectionQueue.Enqueue({ x, y });
			}
		}

		GenerationStats->AddNewRowToAllCounters();

		HeightGenCounter->Start();
		GenerateNextSection();
	}
}
