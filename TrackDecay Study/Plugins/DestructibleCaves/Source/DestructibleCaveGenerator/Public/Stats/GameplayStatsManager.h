// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayStatsManager.generated.h"

/**
 * Statistics related to voxel terrain destruction.
 */
USTRUCT(BlueprintType)
struct FDestructionStats
{
	GENERATED_BODY()

	/** Number of destruction attempts (dig operations) performed */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (ToolTip = "Number of dig operations performed by the player or system."))
	int32 NumDigOperations = 0;

	/** Total number of individual voxels modified during destruction */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (ToolTip = "Total number of voxels affected by destruction events."))
	int32 NumVoxelsModified = 0;

	/** Estimated total destroyed volume in cubic meters */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (ToolTip = "Approximate volume of terrain removed, in cubic meters."))
	double EstimatedDestroyedVolumeM = 0.0;

	/** Updates stats when a dig operation is performed */
	void RegisterDig(int32 NumModifiedVoxels, float VoxelSize);
};
/**
 * Statistics related to world exploration and movement.
 * not Available yet
 */
USTRUCT(BlueprintType)
struct FExplorationStats
{
	GENERATED_BODY()

	/** Total distance traveled by the player in meters */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exploration", meta = (ToolTip = "Cumulative distance traveled by the player, in meters."))
	float DistanceTraveled = 0.0f;

	/** Number of unique terrain chunks the player has entered */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exploration", meta = (ToolTip = "Count of unique chunks entered during exploration."))
	int32 NumChunksEntered = 0;

	// Future extensions (e.g., time spent underground) can be added here
};
/**
 * Tracks and aggregates gameplay statistics including destruction and exploration.
 */
UCLASS(BlueprintType)
class DESTRUCTIBLECAVEGENERATOR_API UGameplayStatsManager : public UObject
{
	GENERATED_BODY()

public:
	/** Destruction-related statistics */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ToolTip = "Tracks voxel destruction statistics such as volume and dig count."))
	FDestructionStats Destruction;

	/** Exploration-related statistics */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ToolTip = "Tracks player exploration progress such as distance and visited chunks."))
	FExplorationStats Exploration;

	/*/** Clears all stored gameplay statistics #1#
	UFUNCTION(BlueprintCallable, Category = "Stats", meta = (ToolTip = "Reset all destruction and exploration statistics to default."))
	void ResetAllStats();*/
};