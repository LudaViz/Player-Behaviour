// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
/**
 * 
 */
namespace MCUtils
{
	constexpr int32 EdgeConnection[12][2] = {
		{0, 1}, {1, 2}, {2, 3}, {3, 0}, // 상단
		{4, 5}, {5, 6}, {6, 7}, {7, 4}, // 하단
		{0, 4}, {1, 5}, {2, 6}, {3, 7} // 측면 연결
	};

	const FVector VertexOffset[8] = {
		{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
		{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}
	};

	// inline int32 ComputeCaseIndex(const float CornerValues[8])
	// {
	// 	int32 CaseIndex = 0;
	// 	for (int32 i = 0; i < 8; ++i)
	// 	{
	// 		if (CornerValues[i] < 0.0f) // 음수 → 내부
	// 		{
	// 			CaseIndex |= (1 << i);
	// 		}
	// 	}
	// 	return CaseIndex;
	// }
	//
	// inline void GetTriangleEdges(int32 CaseIndex, TArray<int32>& OutEdges)
	// {
	// 	OutEdges.Reset();
	// 	for (int32 i = 0; i < 16; ++i)
	// 	{
	// 		int32 Edge = MarchingCubesTable[CaseIndex][i];
	// 		if (Edge == -1)
	// 			break;
	// 		OutEdges.Add(Edge);
	// 	}
	// }
	
	// 유틸리티
	int32 ComputeCaseIndex(const float CornerValues[8]);
	void GetTriangleEdges(int32 CaseIndex, TArray<int32>& OutEdges);
	FVector VertexInterp(const FVector& P1, const FVector& P2, float V1, float V2);

	// 생성기
	//void GenerateVertices(const TArray<FVoxelData>& VoxelArray, TArray<FVector>& OutVertices, int32 ChunkSize,float VoxelSize);
	void GenerateVertices(const TArray<float>& InDensityArray, TArray<FVector>& OutVertices, int32 ChunkSize, float VoxelSize, const FIntVector& ChunkCoord);
	void GenerateIndices(const TArray<float>& InDensityArray, TArray<int32>& OutIndices, int32 ChunkSize);
	void GenerateNormals(const TArray<float>& InDensityArray, TArray<FVector>& OutNormals, int32 ChunkSize);

	void GenerateVerticesFromScalarField(const TArray<float>& InDensityArray, TArray<FVector>& OutVertices,
		int32 Size, const FIntVector& ChunkCoord);
	void GenerateIndicesFromScalarField(const TArray<float>& InDensityArray, TArray<int32>& OutIndices, int32 Size);



	
	// // 3D 좌표 → 1D 배열 인덱스
	// UFUNCTION(BlueprintPure, Category="Voxel")
	// static int32 CoordToIndex(int32 X, int32 Y, int32 Z, int32 ChunkSize);
	//
	// // 1D 인덱스 → 3D 좌표
	// UFUNCTION(BlueprintPure, Category="Voxel")
	// static FIntVector IndexToCoord(int32 Index, int32 ChunkSize);
	//
	// // 월드 좌표 → 복셀 좌표
	// UFUNCTION(BlueprintPure, Category="Voxel")
	// static FIntVector WorldToVoxelCoord(const FVector& WorldLocation, float VoxelSize);
	//
	// // 복셀 좌표 → 월드 좌표
	// UFUNCTION(BlueprintPure, Category="Voxel")
	// static FVector VoxelCoordToWorld(const FIntVector& VoxelCoord, float VoxelSize);
	//
	// // 구 범위 내 복셀 좌표 나열
	// UFUNCTION(BlueprintPure, Category="Voxel")
	// static TArray<FIntVector> GetVoxelsInSphere(const FVector& Center, float Radius, float VoxelSize);
	//
	// // 선형 보간 (Density 등)
	// UFUNCTION(BlueprintPure, Category="Voxel")
	// static float Lerp(float A, float B, float Alpha);
	//
	// // Clamp
	// UFUNCTION(BlueprintPure, Category="Voxel")
	// static int32 ClampVoxelCoord(int32 Coord, int32 ChunkSize);
	//
	// // 마스킹 (예: 경계 체크)
	// UFUNCTION(BlueprintPure, Category="Voxel")
	// static bool IsInsideChunk(int32 X, int32 Y, int32 Z, int32 ChunkSize);
	//
	// // 등치선 보간 (Marching Cubes)
	// UFUNCTION(BlueprintPure, Category="Voxel")
	// static FVector InterpolateIsoSurface(const FVector& P1, const FVector& P2, float V1, float V2, float IsoLevel);
};