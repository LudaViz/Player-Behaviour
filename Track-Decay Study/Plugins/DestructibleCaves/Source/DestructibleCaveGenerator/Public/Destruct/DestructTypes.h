// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DestructTypes.generated.h"

/**
 * Holds voxel positions grouped by material type.
 */
USTRUCT(BlueprintType)
struct FPerMaterialVoxels
{
	GENERATED_BODY()

	/** Material ID used for this group of voxels */
	UPROPERTY(BlueprintReadWrite, Category = "Destruction", meta = (ToolTip = "Material type (ID) associated with this group of voxels."))
	uint8 MaterialType=0;

	/** World positions of voxels that share the same material */
	UPROPERTY(BlueprintReadWrite, Category = "Destruction", meta = (ToolTip = "World-space positions of voxels with the same material type."))
	TArray<FVector> VoxelPositions;
};
/**
 * Describes a single voxel modification event, including position and density change.
 */
USTRUCT(BlueprintType)
struct FDestructionEvent
{
	GENERATED_BODY()

	/** World-space position of the modified voxel */
	UPROPERTY(BlueprintReadOnly, Category = "Destruction", meta = (ToolTip = "World-space position of the voxel that was modified."))
	FVector VoxelWorldPosition=FVector::ZeroVector;
	
	/** Voxel coordinate within its chunk */
	UPROPERTY(BlueprintReadOnly, Category = "Destruction", meta = (ToolTip = "Local voxel coordinate inside the chunk."))
	FIntVector VoxelCoord=FIntVector::ZeroValue;

	/** Material type of the voxel before/after modification */
	UPROPERTY(BlueprintReadOnly, Category = "Destruction", meta = (ToolTip = "Material type (ID) of the voxel."))
	uint8 MaterialType=0;

	/** Density before destruction */
	UPROPERTY(BlueprintReadOnly, Category = "Destruction", meta = (ToolTip = "Original density of the voxel before modification."))
	float OldDensity=0.0f;

	/** Density after destruction */
	UPROPERTY(BlueprintReadOnly, Category = "Destruction", meta = (ToolTip = "New density of the voxel after modification."))
	float NewDensity=0.0f;
};
/**
 * Aggregated context for a destruction operation, including affected voxels and shape.
 */
USTRUCT(BlueprintType)
struct FDestructionEventContext
{
	GENERATED_BODY()

	/** Center of the destructive shape in world space */
	UPROPERTY(BlueprintReadWrite, Category = "Destruction", meta = (ToolTip = "Center position of the shape (e.g., sphere) that caused the destruction."))
	FVector ShapeWorldCenter=FVector::ZeroVector;

	/** Radius of the shape used to apply destruction */
	UPROPERTY(BlueprintReadWrite, Category = "Destruction", meta = (ToolTip = "Radius of the destruction shape (e.g., sphere)."))
	float ShapeRadius=0.0f;

	/** Chunk coordinate where the destruction occurred */
	UPROPERTY(BlueprintReadWrite, Category = "Destruction", meta = (ToolTip = "Coordinate of the chunk that was affected by destruction."))
	FIntVector ChunkCoord=FIntVector::ZeroValue;

	/** List of individual voxel destruction events */
	UPROPERTY(BlueprintReadWrite, Category = "Destruction", meta = (ToolTip = "List of individual voxels that were modified in this chunk."))
	TArray<FDestructionEvent> Events;

	/** Per-material voxel world positions for optimized visualization or grouping */
	UPROPERTY(BlueprintReadWrite, Category = "Destruction", meta = (ToolTip = "Voxel world positions grouped by material type."))
	TArray<FPerMaterialVoxels> PerMaterialVoxelPositions;
};