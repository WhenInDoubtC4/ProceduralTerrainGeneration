// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/Texture2D.h"
#include "GenHeight.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHeightmapTextureUpdated, UTexture2D*, heightmapTexture);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROCTERRAINGEN_API UGenHeight : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UGenHeight();

	void Initialize(uint32 width, uint32 height);
	TArray<float>& GenerateHeight();

	UPROPERTY(BlueprintAssignable)
	FHeightmapTextureUpdated OnHeightmapTextureUpdated;
	
	UFUNCTION(BlueprintCallable)
	UTexture2D* GetHeightmapTexture() const { return HeightmapTexture; };

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private:
	uint32 xSize;
	uint32 ySize;
	TArray<float> HeightData;

	void DrawTexture();

	UPROPERTY(VisibleAnywhere, BlueprintGetter = GetHeightmapTexture)
	UTexture2D* HeightmapTexture = nullptr;
};
