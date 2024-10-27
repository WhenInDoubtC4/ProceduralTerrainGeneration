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

	if (terrainMaterial) TerrainMesh->SetMaterial(0, terrainMaterial);
}

// Called every frame
void AGenWorld::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AGenWorld::GenerateTerrain()
{
	TerrainMesh->ClearAllMeshSections();

	TArray<FVector> vertices;
	TArray<int32> triangles;
	TArray<FVector2D> uvs;

	HeightGenerator->Initialize(xVertexCount, yVertexCount);
	TArray<float>& heightData = HeightGenerator->GenerateHeight();

	//Create vertex and UV arrays
	for (int32 y = 0; y < yVertexCount; y++)
	{
		for (int32 x = 0; x < xVertexCount; x++)
		{
			float xValue = x * edgeSize;
			float yValue = y * edgeSize;

			float heightValue = heightData[y * xVertexCount + x];

			FVector newVertex(xValue, yValue, heightValue);
			vertices.Add(newVertex);

			FVector2D uvCoord(float(x % 2), float(y % 2));
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

	TerrainMesh->CreateMeshSection(0, vertices, triangles, TArray<FVector>(), uvs, TArray<FColor>(), TArray<FProcMeshTangent>(), true);
}