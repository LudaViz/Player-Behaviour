// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/TimerHandle.h"
#include "LODCalculator.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class DESTRUCTIBLECAVEGENERATOR_API ULODCalculator : public UObject
{
	GENERATED_BODY()

public:
	ULODCalculator();

	// LOD 레벨 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite,Category="LOD")
	int LODDistance = 2;

	// LOD 계산
	UFUNCTION(BlueprintPure,Category="LOD")
	void CalculateLOD(const FIntVector& ReferenceChunkCoord, const FIntVector& TargetChunkCoord,
		int& OutLODLevel, int& OutFace) const;

	// 청크별 LOD 캐시
	// UPROPERTY()
	// TMap<FIntVector, struct FLODData> LODCache;

	// LOD 상태 업데이트
	UFUNCTION(BlueprintCallable,Category="LOD")
	void UpdateChunkLOD(const FIntVector& ChunkCoord, const FVector& ReferenceLocation);

	// 캐시 정리
	UFUNCTION(BlueprintCallable,Category="LOD")
	void CleanupLODCache();

private:
	// Lazy 정점 정리 타이머
	FTimerHandle LODCleanupTimer;
};
