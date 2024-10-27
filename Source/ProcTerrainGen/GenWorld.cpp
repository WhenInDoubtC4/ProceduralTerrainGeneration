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

	GenerateTerrain();

	if (!terrainMaterial) return;

	for (uint32 i = 0; i < xSections * ySections; i++)
	{
		TerrainMesh->SetMaterial(i, terrainMaterial);
	}
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
			GenerateSection(x, y);
		}
	}
}

void AGenWorld::GenerateSection(uint32 xSection, uint32 ySection)
{
	uint32 sectionIndex = ySection * ySections + xSection;

	TArray<FVector> vertices;
	TArray<int32> triangles;
	TArray<FVector2D> uvs;

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

	TerrainMesh->CreateMeshSection(sectionIndex, vertices, triangles, TArray<FVector>(), uvs, TArray<FColor>(), TArray<FProcMeshTangent>(), true);

	//AsyncTask(ENamedThreads::AnyHiPriThreadNormalTask, [=, this]()
	//	{
	//		//Hangs >128x128, crashes >256x256	
	//		TArray<FVector> normals;
	//		TArray<FProcMeshTangent> tangents;
	//		UKismetProceduralMeshLibrary::CalculateTangentsForMesh(vertices, triangles, uvs, normals, tangents);

	//		TerrainMesh->UpdateMeshSection(sectionIndex, vertices, normals, uvs, TArray<FColor>(), tangents);
	//	});
}
