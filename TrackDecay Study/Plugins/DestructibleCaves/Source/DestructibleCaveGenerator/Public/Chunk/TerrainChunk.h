// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Mesh/MarchingCubes.h"
#include "TerrainChunk.generated.h"

class ULODCalculator;
class ATerrainManager;
struct FChunkData;
struct FMarchingCubes;
/**
 * State of a terrain chunk in its lifecycle.
 */
UENUM(BlueprintType)
enum class ETerrainChunkState : uint8
{
	//언로드됨. Fill 등 태스크 없는 놈들 언로드해야 하는데 어케하지
	Unloaded UMETA(DisplayName = "Unloaded"),
	//풀에서 가져온 직후
	Idle UMETA(DisplayName = "Idle"),
	//채우기 전
	NeedsFill UMETA(DisplayName = "Needs Fill"),
	//노이즈 넣는중, 다른 작업 하면 절대 안됨
	Filling UMETA(DisplayName = "Filling"),
	//변경된 상태. 해당 상태일 때 LoadAndUnload에서 수집하여 NeedsPrepare로 상태 변경하여 넘겨줌
	Modified UMETA(DisplayName = "Modified"),
	//Filling 종료시, 혹은 메시 변경 시 진입
	NeedsPrepare UMETA(DisplayName = "Needs Prepare"),
	//태스크에서 prepare를 할 때 사용 예정.
	//두 Prepare 간 PrepareTaskGeneration 비교
	Preparing UMETA(DisplayName = "Preparing"),
	//Preparing 종료 시 진입
	NeedsUpload UMETA(DisplayName = "Needs Upload"),
	//어짜피 GameThread에서 진행되어 의미는 없지만 업로드 중일 때
	Uploading UMETA(DisplayName = "Uploading"),
	//전부 완료된 상태
	Complete UMETA(DisplayName = "Complete")
};
/**
 * 실제 월드에 배치되는 청크 액터
 * 메시는 ProceduralMesh, 데이터는 ChunkData에 들어 있습니다.
 */
/**
 * ATerrainChunk is a world-placed actor representing a unit of procedural terrain.
 * It owns a procedural mesh and handles runtime LOD generation and updates.
 */
UCLASS(BlueprintType)
class DESTRUCTIBLECAVEGENERATOR_API ATerrainChunk : public AActor
{
	GENERATED_BODY()
public:
	ETerrainChunkState GetChunkState() const { return ChunkState; }
	void SetChunkState(ETerrainChunkState NewState) { ChunkState = NewState; }
	ATerrainChunk();

	// 메시
	/** Procedural mesh component used to render this chunk */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,Category="Terrain Chunk", meta=(ToolTip="Procedural mesh component used to visualize this terrain chunk."))
	class UCustomProceduralMeshComponent* ProceduralMesh;

	// 청크 데이터 : ChunkCoordinate로 UChunkManager를 통해 접근할것
	// FChunkData* ChunkData;

	// 청크 좌표
	/** World-space coordinate index of this chunk */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,Category="Terrain Chunk", meta=(ToolTip="Logical grid coordinate of this terrain chunk."))
	FIntVector ChunkCoordinate;


	/** Chunk initialization entry point */
	void InitializeChunk(const FIntVector& InChunkCoord, FCaveMarchingCubes* InMarchingCubesGenerator, ATerrainManager* InManager,const FVector& InLocation, const FVector& InScale);

	/**
	 * 메시에 이용될 데이터 (버텍스, 인덱스 등)을 생성합니다.
	 */
	/** Generation counter to detect stale prepare tasks */
	std::atomic<int32> PrepareTaskGeneration = 0; // 증가하는 generation
	/** Called on worker thread to prepare mesh data */
	void PrepareMesh(int32 LODLevel, const FChunkData* ChunkData);

	/** Called on game thread to upload mesh to ProceduralMeshComponent */
	void UploadMesh(/*bool bEnableWireframe=false*/);

	/** Update LOD. */
	void UpdateLOD(int32 NewLOD, int32 NewFace);
	
	/** Clear procedural mesh sections */
	void ClearMesh();
	// 청크 활성화/비활성화
	/** Activate or deactivate this chunk */
	UFUNCTION(BlueprintCallable,Category="Terrain Chunk", meta=(ToolTip="Set chunk active state (rendering, update logic)."))
	void SetActive(bool bActive);

	// 청크 리셋 (풀링용)
	/** Reset chunk to reusable state for pooling */
	UFUNCTION(BlueprintCallable,Category="Terrain Chunk", meta=(ToolTip="Reset this chunk before returning to pool."))
	void ResetChunk();

	/**
	 *Material
	 */
	// 청크 메시 생성 시 사용될 매니저 참조
	UPROPERTY()
	/** Reference to owning terrain manager */
	TWeakObjectPtr<ATerrainManager> Manager=nullptr;

	// MaterialType 별로 Chunk 고유 머티리얼 오버라이드
	/** Optional override material mapping per voxel material type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", meta=(ToolTip="Overrides the default material per voxel type (uint8 index)."))
	TMap<uint8, UMaterialInterface*> OverrideMaterialMap;

	/** Resolve material for given voxel material type */
	UMaterialInterface* GetMaterial(uint8 MaterialType) const;
	/** Get number of vertices used by the current mesh */
	int32 GetVerticesNum(int32 LODLevel = -1);

public:
	/** Per-chunk mesh data lock (LODMeshCache) */
	FCriticalSection MeshDataLock;
private:
	/** Chunk state value (atomic for thread-safe task coordination) */
	std::atomic<ETerrainChunkState> ChunkState = ETerrainChunkState::Idle;
	// 마칭 큐브 생성기
	/** Cached marching cubes generator */
	FCaveMarchingCubes* MarchingCubesGenerator;

	UPROPERTY()
	ULODCalculator* LODCalculator;

	// LOD별 메시 데이터 캐시
	/** LOD level to mesh data mapping */
	UPROPERTY()
	FChunkStaticMeshData LODMeshCache;

	// Lazy 정점 제거 타이머
	/** Optional timer for vertex cleanup */
	FTimerHandle VertexCleanupTimer;

	/** Runtime debug visibility */
	UPROPERTY(EditAnywhere,Category="Terrain Chunk", meta=(ToolTip="Whether this chunk is active in-game when spawned."))
	bool bActiveInGame = false;

private:
	void InitLODCache();
public:
	/** Get cached mesh data for specific LOD */
	const FChunkLODLevel& GetLODMeshData(int32 LODLevel) const;
	/** Get cached mesh data for current LOD */
	const FChunkLODLevel& GetCurrentLODMeshData();

private:
	/** Current LOD level used for this chunk */
	UPROPERTY(VisibleAnywhere,Category="Terrain Chunk", meta=(ToolTip="Current LOD level assigned to this chunk."))
	int32 CurrentLOD = 0;

	/**
	 * Direction to higher LOD chunk for transition cells
	 * -1 if transition cell is not needed.
	 */
	UPROPERTY(VisibleAnywhere,Category="Terrain Chunk", meta=(ToolTip="Current LOD level assigned to this chunk."))
	int32 CurrentLODFace = -1;

	/** UploadMesh but only for borders. */
	void UploadMeshFace();

	
};
