// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModifierManager.h"
#include "Containers/Queue.h"
#include "UObject/Object.h"
#include "Container/ThreadSafeMap.h"
#include "Mesh/MarchingCubes.h"
#include "ChunkManager.generated.h"

class ATerrainChunk;
class UChunkScheduler;
struct FMarchingCubes;


/**
 * Describes different chunk loading strategies.
 */
UENUM()
enum class EChunkLoadMode : uint8
{
	// 청크 로드까지 대기. 싱글쓰레드 이용
	BlockingSerial       UMETA(DisplayName = "Blocking (Single Thread)"),
	// 청크 로드까지 대기. 멀티쓰레딩 이용
	BlockingParallel     UMETA(DisplayName = "Blocking (Multi Threaded)"),
	// 비동기로 로드. FTask 이용 
	Task                 UMETA(DisplayName = "Async Task"),
	//스케줄링 후 태스크와 대기 적절히 사용
	ScheduledTask        UMETA(DisplayName = "Scheduled Task"),
	// 비동기로 로드. FRunnable 이용
	Runnable             UMETA(DisplayName = "Async Runnable"),
	// 청크 로드까지 대기. GPU 이용
	GPUBlocking          UMETA(DisplayName = "Blocking GPU"),
	// 비동기로 로드. FTask를 이용해서 GPU에 command를 전송
	GPUTask              UMETA(DisplayName = "Async GPU Task"),
	// 비동기로 로드. FRunnable을 이용해서 GPU에 command를 전송
	GPURunnable          UMETA(DisplayName = "Async GPU Runnable")
};
/**
 * Represents the result of a chunk data generation pass.
 */
struct FChunkFillResult
{
	FChunkData* GeneratedData = nullptr; // 생성된 노이즈 데이터
	FIntVector ChunkCoord;               // 청크 좌표

	// 기본 생성자 (TArray 초기화를 위해 필요)
	FChunkFillResult() = default;

	FChunkFillResult(FChunkData* InData, const FIntVector& InCoord)
		: GeneratedData(InData), ChunkCoord(InCoord) {}
};

/**
 * UChunkManager controls the lifecycle of terrain chunks including creation,
 * destruction, streaming, and LOD management. It supports various async modes.
 */
UCLASS(BlueprintType)
class DESTRUCTIBLECAVEGENERATOR_API UChunkManager : public UObject
{
	GENERATED_BODY()

	friend UModifierManager;

private:
	/** Reference to owning TerrainManager */
	UPROPERTY()
	ATerrainManager* OwnerManager = nullptr;

public:
	UChunkManager();
	void Initialize(ATerrainManager* InManager);

	void ProcessChunks();
	FChunkData* GetChunkData(const FIntVector& ChunkCoord);
	
	void MarkAsRebuildNeeded(const FIntVector& ChunkCoord);

private:

public:
	/** Chunk scheduler that dispatches prepare/upload tasks */
	UPROPERTY()
	UChunkScheduler* ChunkScheduler = nullptr;
	/**
	 * 청크 풀
	 */
	/** Pool of reusable terrain chunks */
	UPROPERTY()
	class UChunkPool* ChunkPool = nullptr;

	/**
	 * 모두 로드가 된 청크
	 */
	/** Map of currently active (loaded) chunks */
	TThreadSafeMap<FIntVector, ATerrainChunk*> ActiveChunks;
	/**
	 * 액터 풀 사이즈. ChunkSizeLevel의 세제곱보다 커야 합니다.
	 */
	/** Initial chunk pool size (must be >= ChunkSize^3) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pool", meta=(ClampMin=1, ToolTip="Initial size of the chunk pool. Should be at least larger than the chunk volume (e.g., ChunkSize^3)."))
	int32 InitialPoolSize = 128;

	/**
	 * 청크 업데이트를 실시간으로 할 것인지의 여부
	 */
	/** Enable streaming-based chunk updates */
	UPROPERTY(EditAnywhere, Category="Streaming", meta=(ToolTip="If true, chunks will be updated automatically around the player."))
	bool bUpdateStreaming = true;

	/**
	 * 실시간으로 청크 업데이트를 할 때의 범위 
	 */
	/** Number of chunks loaded in each direction from camera */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Streaming", meta = (EditCondition = "bUpdateStreaming", ClampMin = 0, ToolTip="Number of chunks to render in each direction from the player."))
	int32 RenderDistance = 3;
	
	/** Extended generation range beyond render distance (internal use) */
	UPROPERTY()
	int32 FillDistance = 0; // RenderDistance보다 넓게 설정

	/**
	 * BeginPlay에서 청크를 생성할 지의 여부
	 */
	/** If true, load initial chunk grid at BeginPlay */
	UPROPERTY(EditAnywhere, Category="Streaming", meta=(ToolTip="Whether to load initial chunk region on BeginPlay."))
	bool bInitialChunkLoad = false;

	/**
	 * 청크 생성 초기값으로, BeginPlay()호출 시 x,y,z방향으로 2*InitialChunkLoadNum개의 청크가 로드됩니다.
	 */
	/** Half-extent in chunks of initial region to load around player */
	UPROPERTY(EditAnywhere, Category="Streaming", meta = (EditCondition = "bInitialChunkLoad", ToolTip="Initial half-extent of chunks loaded in X/Y/Z from the player position."))
	uint32 InitialChunkLoadNum = 0;

	/**
	 * 청크 생성 모드. 동기/비동기/GPU를 지원합니다.
	 */
	UPROPERTY(EditAnywhere,Category="Chunk Manager")
	EChunkLoadMode ChunkLoadMode = EChunkLoadMode::BlockingSerial;

	/**
	 * 멀티쓰레딩에 사용할 쓰레드 수. 0일 경우 청크 당 쓰레드 한 개가 할당됩니다.
	 */
	/** Number of threads to use for parallel chunk loading (0 = auto) */
	UPROPERTY(EditAnywhere,Category="Chunk Manager", meta = (EditCondition = "ChunkLoadMode != EChunkLoadMode::BlockingSerial && ChunkLoadMode != EChunkLoadMode::Task"
		, ToolTip="Number of threads to use for chunk generation. Set to 0 to auto-distribute."))
	uint32 NumThreads = 4;

	/**
	 * 한 프레임에 메시 생성을 할 Chunk 수. 0일 경우 모든 Chunk를 처리합니다.
	 * Chunk Load Mode가 Task일때의 옵션입니다.
	 */
	/** Max number of chunks to process per tick in Task mode */
	UPROPERTY(EditAnywhere,Category="Chunk Manager", meta = (EditCondition = "ChunkLoadMode == EChunkLoadMode::Task", ToolTip="Max number of chunk meshes to generate per tick in Task mode."))
	uint32 NumMeshGeneratePerTick = 4;

	/**
	 * TerrainManager가 BeginPlay()시 처음 청크를 로드
	 */
	void LoadInitialChunks(const FVector& InitialLocation, class UNoiseGenerator* NoiseGenerator);

	/**
	 * 해당 위치 근처의 청크를 업데이트
	 */
	void UpdateChunkStreaming(const FVector& ReferenceLocation, class UNoiseGenerator* NoiseGenerator);

	// /**
	//  * Chunk의 한 모서리에 있는 Cube의 개수입니다. 
	//  */
	// UPROPERTY(EditAnywhere, BlueprintReadWrite)
	// int32 ChunkSize = 32;
	
	/**
	 * 복셀과 복셀 사이의 거리
	 */
	/** Voxel size (spacing between sample points) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = 1.0, ToolTip = "Spacing between voxel samples."))
	float VoxelSize = 32.0f;

	/**
	 * 각 청크의 BoundScale
	 * 늘리면 Flickering 현상이 줄어들음
	 */
	/** Scale applied to bounding box to prevent flickering */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (ToolTip = "Bounds scaling applied to each chunk to reduce flickering or occlusion culling artifacts."))
	float BoundsScale = 5.0f;

	// (디버그) 청크의 경계 표시		
	void DrawDebugBounds(UWorld* World, FColor Color);

	void DrawDebugPoints(UWorld* World, int Stride, mat_type MatType, float IsoLevel);

	void DrawDebugGradients(UWorld* World, int Stride);

	//Chunk Fill 관련 함수
	void AddToFillQueue(FIntVector Coord)
	{
		if (GetChunkData(Coord))
			return;
		ChunksToFill.Add(Coord);
	}
	/////////////////////////////////////////////////////////////////////
	/* 이하는 내부에서 사용하는 함수 */
private:
	/**
	 * 청크 데이터(복셀 데이터) 저장
	 * TODO : Octree
	 */

	//Delete At Release
	TThreadSafeMap<FIntVector, FChunkData*> ChunkDataMap;
	UPROPERTY()
	TSet<FIntVector> ChunksToFill;
	
	/**
	 * 청크 풀링 관리를 위한 함수
	 */

	/**
	 * 청크를 풀에서 가져옵니다. 만약 가능한 청크가 없으면 nullptr를 반환합니다.
	 * game thread 전용입니다.
	 */
	UFUNCTION(BlueprintCallable,Category = "Terrain Manager")
	ATerrainChunk* LoadChunk(const FIntVector& ChunkCoord);
	
	// 청크를 풀로 반납합니다.
	UFUNCTION(BlueprintCallable,Category = "Terrain Manager")
	void UnloadChunk(const FIntVector& ChunkCoord);

public:
	// 청크 좌표 변환
	UFUNCTION(BlueprintPure, Category="Coordinate", meta=(ToolTip="Converts world position to chunk coordinates."))
	FIntVector WorldToChunkCoord(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category="Coordinate", meta=(ToolTip="Converts chunk coordinates to world space position."))
	FVector ChunkToWorldLocation(const FIntVector& ChunkCoord) const;

	UFUNCTION(BlueprintPure, Category="Coordinate", meta=(ToolTip="Returns all chunk coordinates intersecting the given box."))
	TSet<FIntVector> GetChunkCoordsInAABB(FBox Box) const;

	UFUNCTION(BlueprintPure, Category="Coordinate", meta=(ToolTip="Collects all chunk coordinates within a given range from a center position (in chunk units)."))
	void GetChunkCoordsInRange(TSet<FIntVector>& OutCoords,const FVector& Center, int32 DistanceInChunks);
public:
	// 청크 검색 (Octree 기반)
	UFUNCTION(BlueprintCallable, Category="Query", meta=(ToolTip="Returns all loaded chunks within a sphere centered at the given position."))
	TArray<ATerrainChunk*> GetChunksInRadius(const FVector& Center, float Radius);
	UFUNCTION(BlueprintCallable, Category="Query", meta=(ToolTip="Returns all loaded chunks within a given axis-aligned bounding box."))
	TArray<ATerrainChunk*> GetChunksInBox(const FBox& Box);
public:
	//Disk Save/Load
	UFUNCTION(BlueprintCallable, Category="IO", meta=(ToolTip="Save Chunk Datas"))
	void SaveChunksToDisk(const FString& MapName);
	UFUNCTION(BlueprintCallable, Category="IO", meta=(ToolTip="Load Chunk Datas"))
	void LoadChunksFromDisk(const FString& MapName);
	static FString GetChunkSaveFilePath(const FString& MapName);

	//Chunks To Save(Modified etc...)
	FCriticalSection ChunkCoordsToSaveLock;
	UPROPERTY()
	TSet<FIntVector> ChunkCoordsToSave;

private:
	// Octree for spatial partitioning
	class FOctree* ChunkOctree = nullptr;

	/**
	 * UChunkData -> FMeshData 변환에 사용
	 */
	UPROPERTY(EditAnywhere, Category="Generation", meta=(ToolTip="Instance of marching cubes used to extract mesh from voxel data."))
	FCaveMarchingCubes MarchingCubes;

	/**
	 * 비동기 관련 함수/변수
	 */

	// 비동기 태스크 관리
	/**
	 * ATerrainChunk::PrepareMesh()를 호출하는 Queue
	 */
	TQueue<UE::Tasks::TTask<ATerrainChunk*>> PrepareMeshTasks; // TTask<void> = FTask

	/**
	 * 실제 청크 생성 구현부
	 */
	void UpdateChunks(const FVector& ReferenceLocation, UNoiseGenerator* NoiseGenerator);
	void UpdateChunksBlockingSerial(const FVector& ReferenceLocation, UNoiseGenerator* NoiseGenerator);
	void UpdateChunksBlockingParallel(const FVector& ReferenceLocation, UNoiseGenerator* NoiseGenerator);
	void UpdateChunksTask(const FVector& ReferenceLocation, UNoiseGenerator* NoiseGenerator);
	void UpdateChunksScheduled(const FVector& ReferenceLocation, UNoiseGenerator* NoiseGenerator);
	// void UpdateChunksBlockingRunnable(const FVector& ReferenceLocation, UNoiseGenerator* NoiseGenerator);
	// void UpdateChunksBlockingGPUBlocking(const FVector& ReferenceLocation, UNoiseGenerator* NoiseGenerator);
	// void UpdateChunksBlockingGPUTask(const FVector& ReferenceLocation, UNoiseGenerator* NoiseGenerator);
	// void UpdateChunksBlockingGPURunnable(const FVector& ReferenceLocation, UNoiseGenerator* NoiseGenerator);

	/** 청크 생성 헬퍼 함수*/
	TSet<FIntVector> GetRequiredChunks(const FVector& ReferenceLocation, const int32 Distance) const;
	void LoadAndUnloadChunks(const UNoiseGenerator* Generator,
											TArray<ATerrainChunk*>& ChunksToGenerateMesh,
											TArray<ATerrainChunk*>& ChunksToUpdateMesh,
											const FVector& ReferenceLocation);
	void FillChunks(const UNoiseGenerator* Generator);
	//Fill 관련 함수
	//Actor와는 별도로 동작
	void LaunchChunkDataTasks(/*const TSet<FIntVector>&,*/const UNoiseGenerator* Generator);
	void ProcessPendingChunkFillTasks();
	//void FinalizeGeneratedChunks(const TArray<FChunkFillResult>& GenerationResults, TArray<ATerrainChunk*>& OutChunksToGenerateMesh);

	void FillChunkData_SingleThreaded(
		/*const TSet<FIntVector>& ChunksToFill,*/
		const UNoiseGenerator* Generator
		/*TArray<ATerrainChunk*>& OutChunksToGenerateMesh*/);
	UPROPERTY()
	TSet<FIntVector> FillingChunks;
	TArray<UE::Tasks::TTask<FChunkFillResult>> PendingChunkFillTasks;
public:
	void Release();
private:
	
	// Release 헬퍼 함수들
	/** Waits for all pending async tasks to complete and cleans up their resources. */
	void ShutdownAsyncTasks();
	/** Releases all memory held by the chunk data map. */
	void ReleaseChunkDataMap();
};
