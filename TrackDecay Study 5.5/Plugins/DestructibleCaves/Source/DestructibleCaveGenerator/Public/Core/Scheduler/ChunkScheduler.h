// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ChunkManager.h"
#include "UObject/Object.h"
#include "ConvexVolume.h"
#include "ChunkScheduler.generated.h"

class ATerrainManager;
class UChunkManager;
class ATerrainChunk;

USTRUCT(BlueprintType)
struct FChunkTask
{
	GENERATED_BODY()

	UPROPERTY()
	ATerrainChunk* Chunk = nullptr;

	UPROPERTY()
	FIntVector ChunkCoord=FIntVector::ZeroValue;
	UPROPERTY()
	float Priority = 0.f;

	UPROPERTY()
	double EstimatedCost = 0.0;

	FChunkTask() = default;
	FChunkTask(ATerrainChunk* InChunk, float InPriority, double InCost);

	bool operator==(const FChunkTask& Other) const
	{
		return ChunkCoord == Other.ChunkCoord;
	}
	bool MatchesChunk() const;
};
UENUM(BlueprintType)
enum class EChunkUpdateReason : uint8
{
	Create,
	Destroy
};

UCLASS(Blueprintable)
class DESTRUCTIBLECAVEGENERATOR_API UChunkScheduler : public UObject
{
	GENERATED_BODY()

public:
	UChunkScheduler() = default;
	void SetTerrainManger(ATerrainManager* InManager);

	void PrepareNewChunks(TArray<ATerrainChunk*>& ChunksToGenerateMesh, const FVector& PlayerLocation);
	void PrepareExistingChunks(TArray<ATerrainChunk*>& ChunksToUpdateMesh, const FVector& PlayerLocation);

	void ProcessGenQueue();
	//void UpdateCameraInfo(const FVector& InCameraLocation,const FVector& InCameraForward);
	void UpdateCameraViewProjection(UWorld* WorldContext);
	
	int32 GetChunkCoordDistance(const FIntVector& A, const FIntVector& B) const;
	int32 GetDistanceFromCamera(const ATerrainChunk* Chunk) const;
	const FIntVector& GetCameraChunkCoord() const { return CachedCameraChunkCoord; }
private:
	void ProcessGenQueue_Physics();
	void ProcessGenQueue_Render();
	void ProcessGenQueue_Background();

	float EvaluatePriority(const ATerrainChunk* Chunk) const;
	bool IsInViewFrustum(ATerrainChunk* Chunk) const;

	//bool ShouldBeInRenderQueue(ATerrainChunk* Chunk) const;

	bool NeedsPhysicsInteraction(ATerrainChunk* Chunk/*, const FVector& PlayerLocation*/) const;

	double EstimateCost(ATerrainChunk* Chunk) const;
	UPROPERTY()
	UChunkManager* ChunkManager;
	UPROPERTY()
	ATerrainManager* TerrainManager;
private:
	//특정 카메라 기준 우선순위 판단
	FVector CachedCameraLocation;
	FVector CachedCameraForward;
	//FMatrix ViewProjectionMatrix;
	FConvexVolume ViewFrustum;
	FIntVector CachedCameraChunkCoord;
	
	TQueue<UE::Tasks::TTask<FChunkTask>> NewMeshTasks;
	TMap<ATerrainChunk*, UE::Tasks::TTask<FChunkTask>> UpdateMeshTaskMap;
	bool IsChunkDestroyed(ATerrainChunk* Chunk) const;
	/*
	UPROPERTY()
	TArray<FChunkTask> PrepareQueue;*/
	TQueue<FChunkTask,EQueueMode::Mpsc> GenQueue_Physics;
	TQueue<FChunkTask,EQueueMode::Mpsc> GenQueue_Render;
	TQueue<FChunkTask,EQueueMode::Mpsc> GenQueue_Background;
public:
	void Release();
};