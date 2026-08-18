// Copyright 2025 J2K2. All Rights Reserved.

#include "Mesh/LODCalculator.h"

#include "Core/DCGMacros.h"

ULODCalculator::ULODCalculator()
{
}

void ULODCalculator::CalculateLOD(const FIntVector& ReferenceChunkCoord, const FIntVector& TargetChunkCoord,
	int& OutLODLevel, int& OutFace) const
{
	FIntVector TargetToRef = ReferenceChunkCoord - TargetChunkCoord;

	int dx = TargetToRef.X;
	int dy = TargetToRef.Y;
	int dz = TargetToRef.Z;

	int dxAbs = FMath::Abs(dx);
	int dyAbs = FMath::Abs(dy);
	int dzAbs = FMath::Abs(dz);

	// Chebyshev 거리 (각 축의 절대값 중 최대)
	int Distance = FMath::Max3(dxAbs, dyAbs, dzAbs);

	int MaxLOD = MAX_LOD_LEVEL;

	OutLODLevel = MaxLOD;
	for (int Level = 0; Level <= MaxLOD; Level++)
	{
		if (Distance < LODDistance * (Level + 1))
		{
			OutLODLevel = Level;
			break;
		}
	}
	
	OutFace = -1; // 기본값

	// LOD 경계면에 정확히 위치하면 direction 결정 (숫자가 낮은 방향)
	if (OutLODLevel > 0 && Distance == LODDistance * OutLODLevel)
	{
		int ChebyEdges[3] = { dxAbs, dyAbs, dzAbs };
		int Axes[3] = { dx, dy, dz };

		// Chebyshev 거리와 같은 축의 개수를 센다
		int MaxAxisCount = 0;
		int MaxAxisIndex = -1;
		for (int i = 0; i < 3; ++i)
		{
			if (ChebyEdges[i] == Distance)
			{
				MaxAxisCount++;
				MaxAxisIndex = i;
			}
		}

		// *** 오직 한 축만 최대 거리일 때만 방향 부여 ***
		if (MaxAxisCount == 1)
		{
			if (Axes[MaxAxisIndex] > 0)
				OutFace = MaxAxisIndex;      // 0:X+, 1:Y+, 2:Z+
			else if (Axes[MaxAxisIndex] < 0)
				OutFace = MaxAxisIndex + 3;  // 3:X-, 4:Y-, 5:Z-
			// 축 값이 0인 경우엔 방향 없음(실제로는 이 조건에 안걸림)
		}
		// 두 축 이상이 Chebyshev 최대라면(모서리나 꼭짓점), -1을 유지
	}
}

void ULODCalculator::UpdateChunkLOD(const FIntVector& ChunkCoord, const FVector& ReferenceLocation)
{
}

void ULODCalculator::CleanupLODCache()
{
}
