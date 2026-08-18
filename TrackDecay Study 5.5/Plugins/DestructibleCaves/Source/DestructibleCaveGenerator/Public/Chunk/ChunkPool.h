// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Chunk/TerrainChunk.h"
#include "ChunkPool.generated.h"

/**
 * 풀의 사이즈는 한번에 커지지 않습니다.
 * 부족했으면 틱당 조금씩 채웁니다.
 */
UENUM()
enum class EChunkPoolResizeState : uint8
{
	None,
	Expanding,
	Shrinking
};

UCLASS()
class DESTRUCTIBLECAVEGENERATOR_API UChunkPool : public UObject
{
	GENERATED_BODY()

public:
	UChunkPool();

	const TSet<ATerrainChunk*>& GetRentedChunks() const { return RentedChunks; }

	void Initialize(int32 InitialSize, int32 InMaxSize,int32 InMinSize, int32 InStepSize, TSubclassOf<ATerrainChunk> InChunkClass);
	void RequestResize(int32 NewSize);
	void TickResize();

	ATerrainChunk* RentChunk();
	void ReturnChunk(ATerrainChunk* Chunk);
	void CleanupPool();

protected:
	ATerrainChunk* CreateNewChunk() const;

private:
	UPROPERTY()
	TSubclassOf<ATerrainChunk> ChunkClass;
	
	UPROPERTY()
	TSet<ATerrainChunk*> ReturnedChunks;

	UPROPERTY()
	TSet<ATerrainChunk*> RentedChunks;

	int32 CurrentPoolSize;
	int32 TargetPoolSize;
	int32 MaxPoolSize;
	int32 MinPoolSize;
	int32 StepPerTick;

	EChunkPoolResizeState ResizeState;

};
