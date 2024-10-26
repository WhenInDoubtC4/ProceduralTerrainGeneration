// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "GenWorld.generated.h"

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

private:
	UPROPERTY(EditAnywhere)
	int32 xVertexCount = 5;

	UPROPERTY(EditAnywhere)
	int32 yVertexCount = 2;

	UPROPERTY(EditAnywhere)
	float edgeSize = 100.f;
	
	UProceduralMeshComponent* TerrainMesh;

	void GenerateTerrain();
};
