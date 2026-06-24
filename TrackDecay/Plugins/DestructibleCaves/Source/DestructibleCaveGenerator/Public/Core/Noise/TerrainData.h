// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Math/NoiseParameter.h"
#include "Materials/MaterialInterface.h"
#include "TerrainData.generated.h"

USTRUCT(BlueprintType)
struct FTerrainData : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

	FTerrainData()
		: RowName(NAME_None)
		, Material(nullptr)
		, UseNoise2DUpperBound(false)
		, UpperBound(0.0f)
		, UseNoise2DLowerBound(false)
		, LowerBound(0.0f)
		, UseNoise3D(false)
		, bIsOre(false)
		, IsInsideTerrain(false)
		, BaseTerrainRowName(NAME_None)
		, OffSet(0.0f)
		, Noise2DUpperBound()
		, Noise2DLowerBound()
		, Noise3D()
	{}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Metadata", meta = (ToolTip = "The unique name of this data table row. It's set automatically from the Data Table."))
	FName RowName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Layer", meta = (ToolTip = "The material interface to apply to this terrain layer's surface."))
	UMaterialInterface* Material;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Z Layer", meta = (ToolTip = "If true, the upper surface of the terrain will be defined by a 2D noise function + Upper Bound."))
	bool UseNoise2DUpperBound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Z Layer", meta = (ToolTip = "The constant height for the terrain's upper surface."))
	float UpperBound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Z Layer", meta = (ToolTip = "If true, the lower surface of the terrain will be defined by a 2D noise function + Lower Bound."))
	bool UseNoise2DLowerBound;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Z Layer", meta = (ToolTip = "The constant height for the terrain's lower surface."))
	float LowerBound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Z Layer", meta = (ToolTip = "If true, 3D noise will be used to generate volumetric features like caves."))
	bool UseNoise3D;

	// --- Ore Configuration ---
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore Configuration", meta = (ToolTip = "Check this if the terrain layer represents an ore vein or a special deposit, enabling ore-specific settings."))
	bool bIsOre;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore Configuration", meta = (EditCondition = "bIsOre", ToolTip = "If true, this ore will generate inside another base terrain type. If false, it generates as a standalone layer."))
	bool IsInsideTerrain;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore Configuration", meta = (EditCondition = "bIsOre && IsInsideTerrain", ToolTip = "The RowName of the base terrain layer where this ore should be generated."))
	FName BaseTerrainRowName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ore Configuration", meta = (EditCondition = "bIsOre && IsInsideTerrain", ToolTip = "Used for vertical displacement"))
	float OffSet;

	// --- Noise Parameters ---
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (EditCondition = "UseNoise2DUpperBound", ToolTip = "Parameters for the 2D noise that defines the upper surface of the terrain."))
	FNoiseParameter Noise2DUpperBound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (EditCondition = "UseNoise2DLowerBound", ToolTip = "Parameters for the 2D noise that defines the lower surface of the terrain."))
	FNoiseParameter Noise2DLowerBound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (EditCondition = "UseNoise3D", ToolTip = "Parameters for the 3D noise used for volumetric generation."))
	FNoiseParameter Noise3D;
};