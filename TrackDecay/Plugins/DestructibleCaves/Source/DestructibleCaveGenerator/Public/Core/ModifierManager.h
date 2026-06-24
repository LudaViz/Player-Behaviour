// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ModifierManager.generated.h"

/**
 * Delegate triggered when a single chunk is modified by voxel destruction.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVoxelDestructionEvent, const FDestructionEventContext&, Context);

/**
 * Delegate triggered globally when multiple chunks are modified at once.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVoxelDestructionGlobalEvent, const TArray<FDestructionEventContext>&, Contexts);

/**
 * Internal structure to represent a pending dig operation.
 */
struct FPendingDigOperation
{
	FSphere Sphere;
	TSet<FIntVector> FailedCoords;
	float Strength;
};

/**
 * UModifierManager handles runtime voxel destruction (and possibly addition/smoothing in the future).
 * Delegates changes to the appropriate TerrainManager and manages retry logic for chunk modification.
 */
UCLASS(BlueprintType)
class DESTRUCTIBLECAVEGENERATOR_API UModifierManager : public UObject
{
	GENERATED_BODY()

public:
	/** Initialize the manager with a reference to the TerrainManager */
	void Initialize(class ATerrainManager* InTerrainManager);

	/** Called per-frame to handle retry logic */
	void Tick(float DeltaTime);

	/** Attempt to dig using a generic ShapeComponent (e.g., Sphere, Box, Capsule) */
	UFUNCTION(BlueprintCallable, Category="Voxel")
	int32 DigWithShape(const UShapeComponent* ShapeComponent, float Strength);

	/** Dig with a spherical volume */
	UFUNCTION(BlueprintCallable, Category="Voxel")
	int32 DigWithSphere(const FSphere& Sphere, float Strength);

	/** Dig with sphere and expose FailedCoords + MaskingCoords for controlled editing */
	int32 DigWithSphere(const FSphere& Sphere, float Strength, TSet<FIntVector>& FailedCoords, const TSet<FIntVector>* MaskingCoords = nullptr);

	/** Event triggered per chunk upon voxel destruction */
	UPROPERTY(BlueprintAssignable, Category="Voxel")
	FVoxelDestructionEvent OnVoxelDestruction;

	/** Event triggered globally with all affected chunks */
	UPROPERTY(BlueprintAssignable, Category="Voxel")
	FVoxelDestructionGlobalEvent OnVoxelDestructionGlobal;
	

private:
	/** Reference to TerrainManager used for voxel manipulation */
	UPROPERTY()
	ATerrainManager* TerrainManager;

	/** List of pending retry operations */
	TArray<FPendingDigOperation> RetryList;
};
