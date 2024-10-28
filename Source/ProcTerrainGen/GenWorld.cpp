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
	AllTerrainSectionsReady.AddUObject(this, &AGenWorld::OnAllTerrainSectionsReady);

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
		sectionIndex = ySection * ySections + xSection;

		TArray<float> heightData;
		HeightGenerator->GenerateHeight(xSection, ySection, heightData);

		//Create vertex and UV arrays
		for (int32 y = 0; y < yVertexCount; y++)
		{
			for (int32 x = 0; x < xVertexCount; x++)
			{
				float xValue = x * edgeSize;
				float yValue = y * edgeSize;

				xValue += xSection * (xVertexCount - 1) * edgeSize;
				yValue += ySection * (yVertexCount - 1) * edgeSize;

				float heightValue = heightData[y * xVertexCount + x];

				FVector newVertex(xValue, yValue, heightValue);
				vertices.Add(newVertex);

				FVector2D uvCoord((float)x, (float)y);
				uvs.Add(uvCoord);
			}
		}

		//Create triangles (ccw winding order)
		for (int32 y = 0; y < yVertexCount - 1; y++)
		{
			for (int32 x = 0; x < xVertexCount - 1; x++)
			{
				int32 startIndex = y * xVertexCount + x;

				triangles.Add(startIndex); //(0,0)
				triangles.Add(startIndex + xVertexCount); //(0, 1)
				triangles.Add(startIndex + 1); //(1,0)

				triangles.Add(startIndex + xVertexCount); //(0, 1)
				triangles.Add(startIndex + xVertexCount + 1); // (1, 1)
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
	TerrainMesh->CreateMeshSection(sectionIndex, vertices, triangles, normals, uvs, TArray<FColor>(), tangents, true);
	if (terrainMaterial) TerrainMesh->SetMaterial(sectionIndex, terrainMaterial);

	GenerateNextSection();
}

void AGenWorld::OnAllTerrainSectionsReady()
{
	for (uint32 i = 0; i < xSections * ySections; i++) TBNQueue.Enqueue(i);

	GenerateNextTBN();
}

void AGenWorld::GenerateNextTBN()
{
	normals.Empty();
	tangents.Empty();

	if (TBNQueue.IsEmpty()) return;

	uint32 nextSectionIndex;
	TBNQueue.Dequeue(nextSectionIndex);

	FProcMeshSection* currentSection = TerrainMesh->GetProcMeshSection(nextSectionIndex);

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
		UKismetProceduralMeshLibrary::CalculateTangentsForMesh(extracedVertices, extracedTriangles, extracedUVs, normals, tangents);

		AsyncTask(ENamedThreads::GameThread, [=, this]
		{
			TerrainMesh->UpdateMeshSection(nextSectionIndex, extracedVertices, normals, extracedUVs, TArray<FColor>(), tangents);
			GenerateNextTBN();
		});
	});
}
