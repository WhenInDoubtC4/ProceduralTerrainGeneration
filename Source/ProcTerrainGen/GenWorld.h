// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "GenHeight.h"
#include "GenFoliage.h"
#include "GenWorld.generated.h"

USTRUCT(BlueprintType)
struct FWorldGenerationOptions
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite)
	int32 xVertexCount = 64;

	UPROPERTY(BlueprintReadWrite)
	int32 yVertexCount = 64;

	UPROPERTY(BlueprintReadWrite)
	int32 xSections = 4;

	UPROPERTY(BlueprintReadWrite)
	int32 ySections = 4;

	UPROPERTY(BlueprintReadWrite)
	float edgeSize = 100.f;
};

USTRUCT(BlueprintType)
struct FTerrainMaterialOptions
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite)
	bool iceEnable = false;

	UPROPERTY(BlueprintReadWrite)
	float iceBlendContrast = 300.f;

	UPROPERTY(BlueprintReadWrite)
	float iceHeight = -3000.f;

	UPROPERTY(BlueprintReadWrite)
	float iceNoiseBlendContrast = 500.f;

	UPROPERTY(BlueprintReadWrite)
	float iceNoiseScale = 10000.f;

	UPROPERTY(BlueprintReadWrite)
	float rockBlendPower = 16.f;

	UPROPERTY(BlueprintReadWrite)
	float rockSlopeAngle = .3f;

	UPROPERTY(BlueprintReadWrite)
	float beachBlendContrast = 200.f;

	UPROPERTY(BlueprintReadWrite)
	float beachBlendNoiseContrast = 200.f;

	UPROPERTY(BlueprintReadWrite)
	float beachBlendNoiseScale = 10000.f;

	UPROPERTY(BlueprintReadWrite)
	float beachWaterLevel = -500.f;
};

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

	UFUNCTION(BlueprintCallable)
	void SetGenerationOptions(FWorldGenerationOptions options) { GenOptions = options; };

	UFUNCTION(BlueprintCallable)
	void SetFoliageGenerationOptions(FFoliageGenerationOptions options) { FoliageGenOptions = options; };

	UFUNCTION(BlueprintCallable)
	void SetGenerateFoliage(bool generate) { GenFoliage = generate; };

	UFUNCTION(BlueprintCallable)
	void UpdateMaterial(FTerrainMaterialOptions options);
	
	UFUNCTION(BlueprintCallable)
	void UpdateFoliage();

private:
	//UPROPERTY(EditAnywhere)
	//int32 GenOptions.xVertexCount = 5;

	//UPROPERTY(EditAnywhere)
	//int32 GenOptions.yVertexCount = 2;

	//UPROPERTY(EditAnywhere)
	//uint32 GenOptions.xSections = 1;

	//UPROPERTY(EditAnywhere)
	//uint32 GenOptions.ySections = 1;

	//UPROPERTY(EditAnywhere)
	//float GenOptions.edgeSize = 100.f;

	UPROPERTY(BlueprintSetter = SetGenerationOptions)
	FWorldGenerationOptions GenOptions;

	UPROPERTY(BlueprintSetter = SetGenerateFoliage)
	bool GenFoliage = true;

	UPROPERTY(BlueprintSetter = SetFoliageGenerationOptions)
	FFoliageGenerationOptions FoliageGenOptions;

	UPROPERTY(EditAnywhere)
	UMaterialInterface* terrainMaterial = nullptr;
	
	UPROPERTY(VisibleAnywhere)
	UProceduralMeshComponent* TerrainMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintGetter = GetHeightGenerator)
	UGenHeight* HeightGenerator = nullptr;

	UPROPERTY(VisibleAnywhere)
	UGenFoliage* FoliageGenerator = nullptr;

	void CalculateSectionTBN(const TArray<FVector>& vertices, const TArray<int32>& indices, const TArray<FVector2D>& uvs, TArray<FVector>& normals, TArray<FProcMeshTangent>& tangents);

	TQueue<TPair<uint32, uint32>> SectionQueue;
	void GenerateNextSection();
	void OnNextSectionReady();
	void CalculateTerrainTBN();
	TQueue<uint32> TBNQueue;
	void GenerateNextTBN();

	FTerrainSectionReady TerrainSectionReady;
	uint32 sectionIndex;
	TArray<FVector> vertices;
	TArray<int32> triangles;
	TArray<FVector2D> uvs;
	
	FAllTerrainSectionsReady AllTerrainSectionsReady;
	//TArray<FVector> normals;
	//TArray<FProcMeshTangent> tangents;

	void RunGlobalFilters();
	void UpdateAllSectionsPost();
	TQueue<uint32> PostSectionQueue;
	void UpdateNextSectionPost();
};