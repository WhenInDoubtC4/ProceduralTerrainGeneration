// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "GenHeight.h"
#include "GenWorld.generated.h"

DECLARE_MULTICAST_DELEGATE(FTerrainSectionReady);
DECLARE_MULTICAST_DELEGATE(FAllTerrainSectionsReady);

UCLASS()
class PROCTERRAINGEN_API AGenWorld : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AGenWorld();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UFUNCTION(BlueprintCallable)
	void GenerateTerrain();

	UFUNCTION(BlueprintCallable)
	UGenHeight* GetHeightGenerator() const { return HeightGenerator; };

private:
	UPROPERTY(EditAnywhere)
	int32 xVertexCount = 5;

	UPROPERTY(EditAnywhere)
	int32 yVertexCount = 2;

	UPROPERTY(EditAnywhere)
	uint32 xSections = 1;

	UPROPERTY(EditAnywhere)
	uint32 ySections = 1;

	UPROPERTY(EditAnywhere)
	float edgeSize = 100.f;

	UPROPERTY(EditAnywhere)
	UMaterialInterface* terrainMaterial = nullptr;
	
	UPROPERTY(VisibleAnywhere)
	UProceduralMeshComponent* TerrainMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintGetter = GetHeightGenerator)
	UGenHeight* HeightGenerator = nullptr;

	TQueue<TPair<uint32, uint32>> SectionQueue;
	void GenerateNextSection();
	void OnNextSectionReady();
	void OnAllTerrainSectionsReady();
	TQueue<uint32> TBNQueue;
	void GenerateNextTBN();

	FTerrainSectionReady TerrainSectionReady;
	uint32 sectionIndex;
	TArray<FVector> vertices;
	TArray<int32> triangles;
	TArray<FVector2D> uvs;
	
	FAllTerrainSectionsReady AllTerrainSectionsReady;
	TArray<FVector> normals;
	TArray<FProcMeshTangent> tangents;
};