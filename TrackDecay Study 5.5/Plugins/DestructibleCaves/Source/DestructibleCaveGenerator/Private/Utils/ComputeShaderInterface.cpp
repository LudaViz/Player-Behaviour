// Copyright 2025 J2K2. All Rights Reserved.

#include "Utils/ComputeShaderInterface.h"

UComputeShaderInterface::UComputeShaderInterface()
{
}

FMeshData UComputeShaderInterface::GenerateMarchingCubesMesh(const TArray<float>& DensityData, int32 ChunkSize,
	int32 LODLevel)
{
	return FMeshData();
}

TArray<float> UComputeShaderInterface::ComputeNoise(const FVector& StartPosition, int32 ChunkSize,
	const FNoiseSettings& Settings)
{
	return TArray<float>();	
}

TArray<int32> UComputeShaderInterface::ComputeLODLevels(const TArray<FVector>& ChunkPositions,
	const FVector& ReferencePosition, const TArray<float>& LODDistances)
{
	return TArray<int32>();
}

void UComputeShaderInterface::CreateBuffers()
{
}

void UComputeShaderInterface::ReleaseBuffers()
{
}
