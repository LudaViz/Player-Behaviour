// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TerrainDestructComponent.generated.h"

class UChunkManager;
class UModifierManager;
UENUM(BlueprintType)
enum class ETerrainDestructShapeType : uint8
{
	Sphere,
	//Box,
	// Capsule, Cylinder, Mesh 등 확장 가능
};

USTRUCT(BlueprintType)
struct FTerrainDestructShape
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere,Category="Destruct Shape")
	ETerrainDestructShapeType ShapeType = ETerrainDestructShapeType::Sphere;

	UPROPERTY(EditAnywhere,Category="Destruct Shape", meta = (EditCondition = "ShapeType == ETerrainDestructShapeType::Sphere"))
	float Radius = 300.0f;

	/*UPROPERTY(EditAnywhere, meta = (EditCondition = "ShapeType == ETerrainDestructShapeType::Box"))
	FVector HalfExtent = FVector(300.0f);*/
};
/**
 * Component that applies terrain destruction using a shape (e.g., sphere, box) as a volume.
 * Can be used by projectiles or other actors to affect the voxel terrain.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DESTRUCTIBLECAVEGENERATOR_API UTerrainDestructComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UTerrainDestructComponent();
protected:
	virtual void BeginPlay() override;
public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnRegister() override;

	/*
	//나중에 다른걸로 변경해야함
	/** Shape used to define the area of terrain destruction (e.g., SphereComponent) #1#
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ExposeOnSpawn = true, ToolTip = "Shape used to define the destruction area (e.g., Sphere, Box). Must be a UShapeComponent.")) 
	UShapeComponent* ShapeComponent;
	*/
	/** Destruction shape definition */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction")
	FTerrainDestructShape DestructShape;

	/** Destruction strength applied to affected voxels. Negative values may mean full removal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction", meta = (ToolTip = "Strength of the destruction applied to voxels within the shape. If negative, full removal may be assumed."))
	float Strength = -1.0f;

	/**
	 * Applies destruction using the current shape and strength settings.
	 * Returns the number of chunks modified.
	 */
	UFUNCTION(BlueprintCallable, Category = "Destruction", meta = (ToolTip = "Applies terrain destruction using the configured shape and strength. Returns the number of affected chunks."))
	int32 ApplyDestruction();

private:
	/** Cached pointer to the terrain chunk manager */
	UPROPERTY()
	UChunkManager* ChunkManager = nullptr;

	/** Cached pointer to the voxel modifier system */
	UPROPERTY()
	UModifierManager* ModifierManager = nullptr;

	/** Internal fill distance used to determine the area affected by destruction */
	UPROPERTY()
	int32 FillDistance = 2;

	/** Cached set of chunk coordinates within the fill distance */
	UPROPERTY()
	TSet<FIntVector> CoordsInFillDistance;
};
