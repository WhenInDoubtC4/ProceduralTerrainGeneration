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
	FoliageGenerator->Clear();
	TerrainMesh->ClearAllMeshSections();

	HeightGenerator->Initialize(GenOptions.xSections, GenOptions.ySections, GenOptions.xVertexCount, GenOptions.yVertexCount, GenOptions.edgeSize);

	for (int32 y = 0; y < GenOptions.ySections; y++)
	{
		for (int32 x = 0; x < GenOptions.xSections; x++)
		{
			SectionQueue.Enqueue({x, y});
		}
	}

	GenerateNextSection();
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

void AGenWorld::GenerateNextSection()
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
				borderVertex.X = GenOptions.xVertexCount * GenOptions.edgeSize + xSection * (GenOptions.xVertexCount)*GenOptions.edgeSize;
				borderVertex.Y = y * GenOptions.edgeSize + ySection * (GenOptions.yVertexCount)*GenOptions.edgeSize;
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

				xValue += xSection * (GenOptions.xVertexCount)*GenOptions.edgeSize;
				yValue += ySection * (GenOptions.yVertexCount)*GenOptions.edgeSize;

				float heightValue = heightData[(GenOptions.yVertexCount - 1) * GenOptions.xVertexCount + x];

				FVector newVertex(xValue, yValue, heightValue);
				vertices.Add(newVertex);

				FVector2D uvCoord((float)x, (float)GenOptions.yVertexCount);
				uvs.Add(uvCoord);
			}

			if (hasXBorder)
			{
				FVector borderVertex;
				borderVertex.X = GenOptions.xVertexCount * GenOptions.edgeSize + xSection * (GenOptions.xVertexCount)*GenOptions.edgeSize;
				borderVertex.Y = GenOptions.yVertexCount * GenOptions.edgeSize + ySection * (GenOptions.yVertexCount)*GenOptions.edgeSize;
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

void AGenWorld::OnNextSectionReady()
{
	TerrainMesh->CreateMeshSection(sectionIndex, vertices, triangles, TArray<FVector>(), uvs, TArray<FColor>(), TArray<FProcMeshTangent>(), true);
	if (terrainMaterial) TerrainMesh->SetMaterial(sectionIndex, terrainMaterial);

	GenerateNextSection();
}

void AGenWorld::CalculateTerrainTBN()
{
	for (int32 i = 0; i < GenOptions.xSections * GenOptions.ySections; i++) TBNQueue.Enqueue(i);

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
	FVector half(GenOptions.xSections * GenOptions.xVertexCount * GenOptions.edgeSize * .5f, GenOptions.ySections * GenOptions.yVertexCount * GenOptions.edgeSize * .5f, 0.f);
	FoliageGenerator->UpdateBounds(half, half + (FVector::UpVector * 20000.f));
	RunGlobalFilters();
}

void AGenWorld::RunGlobalFilters()
{
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [=, this]
	{
		HeightGenerator->Erode();

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
}
