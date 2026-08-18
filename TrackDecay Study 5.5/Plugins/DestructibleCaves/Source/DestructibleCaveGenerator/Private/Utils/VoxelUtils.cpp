// Copyright 2025 J2K2. All Rights Reserved.

#include "Utils/VoxelUtils.h"
#include "Utils/MarchingCubesTable.h"

inline int32 MCUtils::ComputeCaseIndex(const float CornerValues[8])
{
	int32 CaseIndex = 0;
	for (int32 i = 0; i < 8; ++i)
	{
		if (CornerValues[i] < 0.0f) // 음수 → 내부
		{
			CaseIndex |= (1 << i);
		}
	}
	return CaseIndex;
}

inline void MCUtils::GetTriangleEdges(int32 CaseIndex, TArray<int32>& OutEdges)
{
	OutEdges.Reset();
	for (int32 i = 0; i < 16; ++i)
	{
		int32 Edge = MarchingCubesTable[CaseIndex][i];
		if (Edge == -1)
			break;
		OutEdges.Add(Edge);
	}
}

FVector MCUtils::VertexInterp(const FVector& P1, const FVector& P2, float V1, float V2)
{
	if (FMath::Abs(0.0f - V1) < KINDA_SMALL_NUMBER) return P1;
	if (FMath::Abs(0.0f - V2) < KINDA_SMALL_NUMBER) return P2;
	if (FMath::Abs(V1 - V2) < KINDA_SMALL_NUMBER) return P1;

	float T = (0.0f - V1) / (V2 - V1);
	return FMath::Lerp(P1, P2, T);
}

void MCUtils::GenerateVertices(const TArray<float>& InDensityArray, TArray<FVector>& OutVertices,
	int32 ChunkSize, float VoxelSize, const FIntVector& ChunkCoord)
{
	OutVertices.Reset();
	const int32 EffectiveSize = ChunkSize + 1;
	const FVector ChunkOrigin = FVector(ChunkCoord) * ChunkSize * VoxelSize; // <= 인자 추가 필요

	for (int32 z = 0; z < ChunkSize ; ++z)
		for (int32 y = 0; y < ChunkSize ; ++y)
			for (int32 x = 0; x < ChunkSize ; ++x)
			{
				float CornerDensities[8];
				FVector CornerPositions[8];

				for (int i = 0; i < 8; ++i)
				{
					FVector Offset = VertexOffset[i];
					FIntVector Corner = FIntVector(x + (int32)Offset.X, y + (int32)Offset.Y, z + (int32)Offset.Z);
					//int Index = Corner.Z * ChunkSize * ChunkSize + Corner.Y * ChunkSize + Corner.X;
					int Index = Corner.Z * EffectiveSize * EffectiveSize + Corner.Y * EffectiveSize + Corner.X;

					CornerDensities[i] = InDensityArray[Index];
					
					CornerPositions[i] = ChunkOrigin + FVector(Corner) * VoxelSize;
					//CornerPositions[i] = FVector(Corner) * VoxelSize;

				}

				int CaseIndex = ComputeCaseIndex(CornerDensities);
				if (CaseIndex == 0 || CaseIndex == 255) continue;

				for (int i = 0; MarchingCubesTable[CaseIndex][i] != -1; i += 3)
				{
					for (int j = 0; j < 3; ++j)
					{
						int EdgeIdx = MarchingCubesTable[CaseIndex][i + j];
						int A = EdgeConnection[EdgeIdx][0];
						int B = EdgeConnection[EdgeIdx][1];

						FVector V = VertexInterp(CornerPositions[A], CornerPositions[B], CornerDensities[A],
						                         CornerDensities[B]);
						OutVertices.Add(V);
					}
				}
			}
}


void MCUtils::GenerateIndices(const TArray<float>& InDensityArray, TArray<int32>& OutIndices, int32 ChunkSize)
{
	OutIndices.Reset();
	int32 VertIndex = 0;

	for (int32 z = 0; z < ChunkSize; ++z)
		for (int32 y = 0; y < ChunkSize; ++y)
			for (int32 x = 0; x < ChunkSize; ++x)
			{
				float CornerDensities[8];
				for (int i = 0; i < 8; ++i)
				{
					FVector Offset = VertexOffset[i];

					FIntVector Corner = FIntVector(x + (int32)Offset.X, y + (int32)Offset.Y, z + (int32)Offset.Z);
					int Index = Corner.Z * ChunkSize * ChunkSize + Corner.Y * ChunkSize + Corner.X;
					CornerDensities[i] = InDensityArray[Index];

					//CornerDensities[i] = VoxelArray[Index].BiomeType;
				}

				int CaseIndex = ComputeCaseIndex(CornerDensities);
				if (CaseIndex == 0 || CaseIndex == 255) continue;

				for (int i = 0; MarchingCubesTable[CaseIndex][i] != -1; i += 3)
				{
					OutIndices.Add(VertIndex++);
					OutIndices.Add(VertIndex++);
					OutIndices.Add(VertIndex++);
				}
			}
}

void MCUtils::GenerateNormals(const TArray<float>& InDensityArray, TArray<FVector>& OutNormals, int32 ChunkSize)
{
	OutNormals.Reset();
	int32 Count = InDensityArray.Num();
	OutNormals.SetNum(Count);

	// 간단한 중심차분 기반 노멀 추정
	for (int32 z = 1; z < ChunkSize; ++z)
		for (int32 y = 1; y < ChunkSize; ++y)
			for (int32 x = 1; x < ChunkSize; ++x)
			{
				int32 Index = z * ChunkSize * ChunkSize + y * ChunkSize + x;

				float dx = InDensityArray[Index + 1] - InDensityArray[Index - 1];
				float dy = InDensityArray[Index + ChunkSize] - InDensityArray[Index - ChunkSize];
				float dz = InDensityArray[Index + ChunkSize * ChunkSize] - InDensityArray[Index - ChunkSize * ChunkSize]
					;

				FVector Normal = FVector(dx, dy, dz).GetSafeNormal();
				OutNormals[Index] = -Normal; // 내부가 음수 → 바깥 방향으로
			}
}

void MCUtils::GenerateVerticesFromScalarField(const TArray<float>& InDensityArray, TArray<FVector>& OutVertices,
	int32 Size, const FIntVector& ChunkCoord)
{
	OutVertices.Reset();
	const FVector ChunkOrigin =/* FVector(ChunkCoord) * Size * VoxelSize*/FVector::Zero();
	int Stride = Size + 1;

	for (int32 z = 0; z < Size; ++z)
		for (int32 y = 0; y < Size; ++y)
			for (int32 x = 0; x < Size; ++x)
			{
				FVector CornerPositions[8];
				float CornerValues[8];

				for (int32 i = 0; i < 8; ++i)
				{
					int vx = x + VertexOffset[i].X;
					int vy = y + VertexOffset[i].Y;
					int vz = z + VertexOffset[i].Z;

					int Index = vz * Stride * Stride + vy * Stride + vx;
					CornerValues[i] = InDensityArray[Index];
					CornerPositions[i] = ChunkOrigin + FVector(vx, vy, vz);
				}

				int CaseIndex = MCUtils::ComputeCaseIndex(CornerValues);
				TArray<int32> EdgeIndices;
				MCUtils::GetTriangleEdges(CaseIndex, EdgeIndices);

				for (int32 i = 0; i + 2 < EdgeIndices.Num(); i += 3)
				{
					for (int32 j = 0; j < 3; ++j)
					{
						int Edge = EdgeIndices[i + j];
						int A = EdgeConnection[Edge][0];
						int B = EdgeConnection[Edge][1];

						const FVector& PA = CornerPositions[A];
						const FVector& PB = CornerPositions[B];
						float VA = CornerValues[A];
						float VB = CornerValues[B];

						OutVertices.Add(VertexInterp(PA,PB,VA,VB));
					}
				}
			}
}
void MCUtils::GenerateIndicesFromScalarField(const TArray<float>& InDensityArray, TArray<int32>& OutIndices, int32 Size)
{
	OutIndices.Reset();
	int32 VertIndex = 0;

	for (int32 z = 0; z < Size; ++z)
		for (int32 y = 0; y < Size; ++y)
			for (int32 x = 0; x < Size; ++x)
			{
				float CornerValues[8];

				for (int32 i = 0; i < 8; ++i)
				{
					int vx = x + VertexOffset[i].X;
					int vy = y + VertexOffset[i].Y;
					int vz = z + VertexOffset[i].Z;

					int Index = vx + vy * (Size + 1) + vz * (Size + 1) * (Size + 1);
					CornerValues[i] = InDensityArray[Index];
				}

				int CaseIndex = MCUtils::ComputeCaseIndex(CornerValues);
				TArray<int32> EdgeIndices;
				MCUtils::GetTriangleEdges(CaseIndex, EdgeIndices);

				for (int32 i = 0; i + 2 < EdgeIndices.Num(); i += 3)
				{
					OutIndices.Add(VertIndex++);
					OutIndices.Add(VertIndex++);
					OutIndices.Add(VertIndex++);
				}
			}
}
