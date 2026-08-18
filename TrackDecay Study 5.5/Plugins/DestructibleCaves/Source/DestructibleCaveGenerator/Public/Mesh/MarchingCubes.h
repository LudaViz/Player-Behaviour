// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshData.h"
#include "Chunk/ChunkData.h"
#include "UObject/Object.h"
#include "MarchingCubes.generated.h"

UENUM(BlueprintType)
enum class EMarchingCubesType : uint8
{
	Legacy			UMETA(DisplayName="FMarchingCubes"),
	VoxelMesher		UMETA(DisplayName="VoxelMesher"),
	// TransVoxel		UMETA(DisplayName="TransVoxel"),
	// Regular			UMETA(DisplayName="Regular"),
	// Transition		UMETA(DisplayName="Transition"),
	// GPU				UMETA(DisplayName="GPU")
};

/**
 * 
 */
USTRUCT(BlueprintType)
struct DESTRUCTIBLECAVEGENERATOR_API FCaveMarchingCubes
{
	GENERATED_BODY()

public:
	void GenerateMesh(const FChunkData* ChunkData, int32 LODLevel, FChunkLODLevel& OutMultiMesh);

	UPROPERTY(EditAnywhere,Category="Marching Cubes")
	EMarchingCubesType Type=EMarchingCubesType::VoxelMesher;
private:
	static void GenerateMeshLegacy(const FChunkData* ChunkData, int32 LODLevel, FChunkLODLevel& OutMultiMesh);
	static void GenerateMeshMesher(const FChunkData* ChunkData, int32 LODLevel, FChunkLODLevel& OutMultiMesh);
	// static void GenerateMeshTransVoxel(const FChunkData* ChunkData, int32 LODLevel, FChunkLODLevel& OutMultiMesh);
	// static void GenerateMeshRegular(const FChunkData* ChunkData, int32 LODLevel, FChunkLODLevel& OutMultiMesh);
	// static void GenerateMeshTransition(const FChunkData* ChunkData, int32 LODLevel, FChunkLODLevel& OutMultiMesh);
};
