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
}

// Called when the game starts or when spawned
void AGenWorld::BeginPlay()
{
	Super::BeginPlay();

	TerrainSectionReady.AddUObject(this, &AGenWorld::OnNextSectionReady);
	AllTerrainSectionsReady.AddUObject(this, &AGenWorld::CalculateTerrainTBN);

	GenerateTerrain();
}

// Called every frame
void AGenWorld::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AGenWorld::GenerateTerrain()
{
	TerrainMesh->ClearAllMeshSections();

	HeightGenerator->Initialize(xSections, ySections, xVertexCount, yVertexCount);

	for (uint32 y = 0; y < ySections; y++)
	{
		for (uint32 x = 0; x < xSections; x++)
		{
			SectionQueue.Enqueue({x, y});
		}
	}

	GenerateNextSection();
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

	uint32 xSection = section.Key;
	uint32 ySection = section.Value;

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [=, this]()
	{
		FPlatformProcess::Sleep(.1f);

		sectionIndex = ySection * ySections + xSection;

		TArray<float> heightData;
		HeightGenerator->GenerateHeight(xSection, ySection, heightData);

		bool hasXBorder = xSection < xSections - 1;
		bool hasYBorder = ySection < ySections - 1;

		//Create vertex and UV arrays
		for (int32 y = 0; y < yVertexCount; y++)
		{
			for (int32 x = 0; x < xVertexCount; x++)
			{
				float xValue = x * edgeSize;
				float yValue = y * edgeSize;

				xValue += xSection * (xVertexCount) * edgeSize;
				yValue += ySection * (yVertexCount) * edgeSize;

				float heightValue = heightData[y * xVertexCount + x];
				//float heightValue = 0.f;

				FVector newVertex(xValue, yValue, heightValue);
				vertices.Add(newVertex);

				FVector2D uvCoord((float)x, (float)y);
				uvs.Add(uvCoord);
			}

			if (hasXBorder)
			{
				FVector borderVertex;
				borderVertex.X = xVertexCount * edgeSize + xSection * (xVertexCount)*edgeSize;
				borderVertex.Y = y * edgeSize + ySection * (yVertexCount)*edgeSize;
				borderVertex.Z = heightData[y * xVertexCount + xVertexCount - 1];

				vertices.Add(borderVertex);

				FVector2D uvCoord(xVertexCount, (float)y);
				uvs.Add(uvCoord);
			}
		}

		if (hasYBorder)
		{
			for (int32 x = 0; x < xVertexCount; x++)
			{
				float xValue = x * edgeSize;
				float yValue = yVertexCount * edgeSize;

				xValue += xSection * (xVertexCount)*edgeSize;
				yValue += ySection * (yVertexCount)*edgeSize;

				float heightValue = heightData[(yVertexCount - 1) * xVertexCount + x];

				FVector newVertex(xValue, yValue, heightValue);
				vertices.Add(newVertex);

				FVector2D uvCoord((float)x, (float)yVertexCount);
				uvs.Add(uvCoord);
			}

			if (hasXBorder)
			{
				FVector borderVertex;
				borderVertex.X = xVertexCount * edgeSize + xSection * (xVertexCount)*edgeSize;
				borderVertex.Y = yVertexCount * edgeSize + ySection * (yVertexCount)*edgeSize;
				borderVertex.Z = heightData[(yVertexCount - 1) * xVertexCount + xVertexCount - 1];

				vertices.Add(borderVertex);

				FVector2D uvCoord(xVertexCount, (float)yVertexCount);
				uvs.Add(uvCoord);
			}
		}

		//Create triangles (ccw winding order)
		for (int32 y = 0; y < yVertexCount - 1; y++)
		{
			for (int32 x = 0; x < xVertexCount - 1; x++)
			{
				int32 startIndex = y * xVertexCount + x;

				if (hasXBorder) startIndex += y;

				int32 offset = hasXBorder ? 1 : 0;

				triangles.Add(startIndex); //(0,0)
				triangles.Add(startIndex + xVertexCount + offset); //(0, 1)
				triangles.Add(startIndex + 1); //(1,0)

				triangles.Add(startIndex + xVertexCount + offset); //(0, 1)
				triangles.Add(startIndex + xVertexCount + 1 + offset); // (1, 1)
				triangles.Add(startIndex + 1); //(1, 0)
			}

			if (hasXBorder)
			{
				int32 startIndex = y * xVertexCount + (xVertexCount - 1) + y;

				triangles.Add(startIndex); //(0,0)
				triangles.Add(startIndex + xVertexCount + 1); //(0, 1)
				triangles.Add(startIndex + 1); //(1,0)

				triangles.Add(startIndex + xVertexCount + 1); //(0, 1)
				triangles.Add(startIndex + xVertexCount + 2); // (1, 1)
				triangles.Add(startIndex + 1); //(1, 0)
			}
		}

		if (hasYBorder)
		{
			for (int32 x = 0; x < xVertexCount - 1; x++)
			{
				int32 startIndex = (yVertexCount - 1) * xVertexCount + x;

				if (hasXBorder) startIndex += yVertexCount - 1;

				int32 offset = hasXBorder ? 1 : 0;

				triangles.Add(startIndex); //(0,0)
				triangles.Add(startIndex + xVertexCount + offset); //(0, 1)
				triangles.Add(startIndex + 1); //(1,0)

				triangles.Add(startIndex + xVertexCount + offset); //(0, 1)
				triangles.Add(startIndex + xVertexCount + 1 + offset); // (1, 1)
				triangles.Add(startIndex + 1); //(1, 0)
			}

			if (hasXBorder)
			{
				int32 startIndex = (yVertexCount - 1) * xVertexCount + (xVertexCount - 1) + yVertexCount - 1;

				triangles.Add(startIndex); //(0,0)
				triangles.Add(startIndex + xVertexCount + 1); //(0, 1)
				triangles.Add(startIndex + 1); //(1,0)

				triangles.Add(startIndex + xVertexCount + 1); //(0, 1)
				triangles.Add(startIndex + xVertexCount + 2); // (1, 1)
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
	for (uint32 i = 0; i < xSections * ySections; i++) TBNQueue.Enqueue(i);

	GenerateNextTBN();
}

void AGenWorld::GenerateNextTBN()
{
	/*normals.Empty();
	tangents.Empty();*/

	if (TBNQueue.IsEmpty())
	{
		RunGlobalFilters();

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

		UKismetProceduralMeshLibrary::CalculateTangentsForMesh(extracedVertices, extracedTriangles, extracedUVs, normals, tangents);

		AsyncTask(ENamedThreads::GameThread, [=, this]
		{
			TerrainMesh->UpdateMeshSection(nextSectionIndex, extracedVertices, normals, extracedUVs, TArray<FColor>(), tangents);
			GenerateNextTBN();
		});
	});
}

void AGenWorld::RunGlobalFilters()
{
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [=, this]
	{
		//HeightGenerator->GridBasedErosion();
		HeightGenerator->ParticleBasedErosion();
		//HeightGenerator->GlobalSmooth();

		AsyncTask(ENamedThreads::GameThread, [=, this]
		{
			HeightGenerator->DrawTexture();
			
			UpdateAllSectionsPost();
		});
	});
}

void AGenWorld::UpdateAllSectionsPost()
{
	for (uint32 i = 0; i < xSections * ySections; i++) PostSectionQueue.Enqueue(i);

	UpdateNextSectionPost();
}

void AGenWorld::UpdateNextSectionPost()
{
	if (PostSectionQueue.IsEmpty()) return;

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

		uint32 i = 0;
		for (const FProcMeshVertex& vertexData : currentSection->ProcVertexBuffer)
		{
			FVector newVertex = vertexData.Position;
			newVertex.Z = height[i++];

			vertices.Add(newVertex);
			uvs.Add(vertexData.UV0);
		}

		for (const uint32& index : currentSection->ProcIndexBuffer)
		{
			indices.Add(index);
		}

		TArray<FVector> normals;
		TArray<FProcMeshTangent> tangents;

		//TODO: Seamless normal generation
		UKismetProceduralMeshLibrary::CalculateTangentsForMesh(vertices, indices, uvs, normals, tangents);

		AsyncTask(ENamedThreads::GameThread, [=, this]
		{
			TerrainMesh->UpdateMeshSection(nextSectionIndex, vertices, normals, uvs, TArray<FColor>(), tangents);

			UpdateNextSectionPost();
		});
	});
}
