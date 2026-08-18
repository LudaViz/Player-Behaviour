// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Mesh/MarchingCubes.h"
#include "Core/NoiseGenerator.h"
#include "ComputeShaderInterface.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class DESTRUCTIBLECAVEGENERATOR_API UComputeShaderInterface : public UObject
{
	GENERATED_BODY()
public:
	UComputeShaderInterface();
	// 마칭 큐브 GPU 계산
	struct FMeshData GenerateMarchingCubesMesh(const TArray<float>& DensityData, int32 ChunkSize, int32 LODLevel);
	// 노이즈 GPU 생성
	TArray<float> ComputeNoise(const FVector& StartPosition, int32 ChunkSize, const struct FNoiseSettings& Settings);
	// LOD 계산
	TArray<int32> ComputeLODLevels(const TArray<FVector>& ChunkPositions, const FVector& ReferencePosition, const TArray<float>& LODDistances);
private:
	// 셰이더 리소스
	class FRHIComputeShader* MarchingCubesShader;
	class FRHIComputeShader* NoiseShader;
	class FRHIComputeShader* LODShader;
	// 버퍼 관리
	void CreateBuffers();
	void ReleaseBuffers();
};