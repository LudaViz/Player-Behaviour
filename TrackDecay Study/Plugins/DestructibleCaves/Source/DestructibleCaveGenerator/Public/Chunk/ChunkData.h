// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Container/LinearArray3D.h"
// #include "Container/MortonArray32.h"
#include "Core/DCGMacros.h"
#include "UObject/Object.h"
#include "Containers/Queue.h"
#include "ChunkData.generated.h"

struct FDestructionEvent;
using mat_type = uint8;
/**
 * Additional data per voxel such as biome type and hardness.
 */
struct FVoxelData
{
	// 바이옴 정보
	uint8 BiomeType = 0;

	/** Hardness level used for tools or destruction resistance */
	uint8 Hardness = 0;
};

// using ChunkMaterialArray = TMortonArray32<mat_type, CHUNK_SIZE + 1, CHUNK_MARGIN>;
// using ChunkVoxelDataArray = TMortonArray32<FVoxelData, CHUNK_SIZE + 1, CHUNK_MARGIN>;

// Voxel storage per material type
using ChunkDensityArray = TLinearArray3D<float, CHUNK_SIZE + 1, CHUNK_MARGIN>; 
using ChunkVoxelDataArray = TLinearArray3D<FVoxelData, CHUNK_SIZE + 1, CHUNK_MARGIN>;

/**
 * 청크 내부의 정보
 * ***갖고 있음***
 * 1. 청크의 크기(= 갖고 있는 복셀의 수)
 * 2. 복셀의 값(Density, Material, Biome)
 *
 * *** 갖고 있지 않음***
 * 1. 복셀의 월드상 위치
 * 2. 청크의 위치
 * 3. 청크 외부의 정보들
 */
/**
 * Stores all voxel data for a single chunk.
 * 
 * This includes material-separated density fields and voxel metadata.
 * It does NOT store the chunk's world position, transform, or global placement context.
 */
USTRUCT(BlueprintType)
struct DESTRUCTIBLECAVEGENERATOR_API FChunkData
{
	GENERATED_BODY()
public:
	// ChunkMaterialArray MaterialData;
	// ChunkVoxelDataArray VoxelData;
	
	/** Per-material scalar field density data */
	TMap<mat_type, ChunkDensityArray> DensityMap;
	/** Per-material voxel metadata (e.g., biome, hardness) */
	TMap<mat_type, ChunkVoxelDataArray> DataMap;
	
	/**
	 * Applies destructive modification using a spherical shape.
	 * Optionally outputs a queue of destruction events.
	 */
	int32 ModifyVoxelsBySphere(const FSphere& Sphere, float VoxelSize, const FIntVector& ChunkCoord, float Strength,TQueue<FDestructionEvent,EQueueMode::Mpsc>* OutDestructionEvents=nullptr);
	
	/** Serializes the chunk into raw bytes (for saving to disk) */
	void SerializeToBytes(TArray<uint8>& OutBytes) const;
	/** Loads chunk data from raw bytes (from disk) */
	void DeserializeFromBytes(const TArray<uint8>& InBytes);

	/** Returns a set of all material types present in the chunk */
	TSet<uint8> CollectUsedMaterialTypes() const;

	/** Creates a max-density field across all material layers */
	ChunkDensityArray GetMaxDensity() const;

	/** Sets the same density for the given coordinate across all material arrays */
	void SetDensityForAllArrays(int16 x, int16 y, int16 z, float InDensity)
	{
		SetDensityForAllArrays(FIntVector(x, y, z), InDensity);
	}
	/** Sets the same density for the given coordinate across all material arrays */
	void SetDensityForAllArrays(FIntVector Coord, float InDensity)
	{
		for (auto& Pair : DensityMap)
		{
			Pair.Value.Set(Coord, InDensity);
		}
	}
	/** Applies a lambda to modify density at Coord for all material types */
	void ApplyForAllArrays(FIntVector Coord, const TFunction<float(mat_type, float)>& Func)
	{
		for (auto& Pair : DensityMap)
		{
			mat_type Type = Pair.Key;
			float Density = Pair.Value.Get(Coord);
			Density = Func(Type, Density);
			Pair.Value.Set(Coord, Density);
		}
	}
	//해당 좌표의 제일 큰 Density 가진 Mat 반환
	/** Returns the material type with the highest density at the given voxel coordinate */
	mat_type GetMaterialType(FIntVector Coord);
	
private:
	// Recursive parallel density modification for hierarchical shapes
	int32 ModifyDensityRecursive_Parallel(
		const FIntVector& Min, const FIntVector& Max,
		int32 Size, float VoxelSize, const FVector& ChunkOrigin,
		const struct FShapeData& ShapeData, const FTransform& ShapeTransform,
		const FTransform& InverseTransform, const float& RadiusSquared,
		const ChunkDensityArray& MaxDensityArray,
		float Strength,
		TQueue<FDestructionEvent,EQueueMode::Mpsc>* OutDestructionEvents,
		int32 CurrentDepth = 0,
		int32 MaxDepth = 2);
	// Recursive single-threaded density modification
	void ModifyDensityRecursive(
		const FIntVector& Min, const FIntVector& Max,
		int32 Size, float VoxelSize, const FVector& ChunkOrigin,
		const struct FShapeData& ShapeData, const FTransform& ShapeTransform,
		const ChunkDensityArray& MaxDensityArray,
		float Strength,
		int32& NumModified,
		int32 CurrentDepth = 0,
		int32 MaxDepth = 2);
};