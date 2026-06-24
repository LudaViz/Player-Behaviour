// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "Meshdata.generated.h"

#define CHUNK_MAX_LOD 3

// 한 Section(서브 메시)의 기초 데이터
USTRUCT()
struct FMeshData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FVector> Vertices;

	UPROPERTY()
	TArray<int32> Triangles;

	UPROPERTY()
	TArray<FVector> Normals;

	UPROPERTY()
	TArray<FVector2D> UVs;

	UPROPERTY()
	TArray<FLinearColor> Colors;

	UPROPERTY()
	TArray<FProcMeshTangent> Tangents;
};

/**
 * One FMeshData has all 6 faces. Only one face has transition cells.
 */
USTRUCT()
struct FChunkMeshSectionBorders
{
	GENERATED_BODY()

	UPROPERTY()
	FMeshData Borders[6];
};

// LOD별, Material별 메시 데이터
USTRUCT()
struct FChunkMaterialSection
{
	GENERATED_BODY()

	/** Mesh for [0,CHUNK_SIZE], for chunks neighboring with same LOD */
	UPROPERTY()
	FMeshData Interior;

	/**
	 * Mesh for [0,CHUNK_SIZE-1] or [1, CHUNK_SIZE], which has neighboring chunks with different LOD
	 */
	UPROPERTY()
	FChunkMeshSectionBorders BorderTransitions;

	/** Mesh for chunks which does not have neighboring chunks with different LOD */
	UPROPERTY()
	FMeshData BorderRegular;
};

// LOD별 Material Map (MaterialIndex → Section 데이터)
USTRUCT()
struct FChunkLODLevel
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<uint8, FChunkMaterialSection> MaterialSections;
};

// Chunk 전체의 LOD 메시 데이터
USTRUCT()
struct FChunkStaticMeshData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FChunkLODLevel>LODLevels;
};
