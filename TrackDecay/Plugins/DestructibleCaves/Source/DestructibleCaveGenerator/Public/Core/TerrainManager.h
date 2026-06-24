// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/NoiseGenerator.h"
#include "Scheduler/TimeBudgetSystem.h"
#include "Engine/DataTable.h"
#include "TerrainManager.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class ATerrainChunk;

/**
 * TerrainManager manages procedural terrain generation and destruction,
 * including chunk streaming, noise-based voxel generation, LOD control,
 * and integration with cave presets and destruction effects.
 */
UCLASS(BlueprintType, Blueprintable)
class DESTRUCTIBLECAVEGENERATOR_API ATerrainManager : public AActor
{
	GENERATED_BODY()

public:
	ATerrainManager();
	~ATerrainManager();
	/** Main chunk manager handling terrain chunk lifecycle */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Terrain")
	class UChunkManager* ChunkManager;

	/** Noise generator for voxel density calculation */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Terrain")
	class UNoiseGenerator* NoiseGenerator;

	/** LOD calculator for terrain chunks */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Terrain")
	class ULODCalculator* LODCalculator;

	/** Runtime terrain modifier system */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Terrain")
	class UModifierManager* ModifierManager;
	
	UFUNCTION(BlueprintCallable, Category="Terrain")
	UModifierManager* GetModifierManager() const { return ModifierManager; }
	

	/** Configuration for time budget-based tick scheduling */
	UPROPERTY(EditAnywhere, Category = "Performance|TimeBudget", meta=(ToolTip="Time budget configuration to control chunk update CPU usage per frame"))
	FTimeBudgetConfig TimeBudgetConfig;

	/** Runtime state of the time budget system */
	FTimeBudgetState TimeBudgetState;
	

	//파티클 관련 코드, 추후 별도 매니저로 이동
	// Niagara 시스템 기본값
	/** Default Niagara system for destruction FX */
	UPROPERTY(EditAnywhere, Category = "Destruction FX", meta=(ToolTip="Default particle system for destruction effects"))
	UNiagaraSystem* DefaultDestructionSystem;

	/** Mapping of material type to pooled NiagaraComponents for FX reuse */
	UPROPERTY()
	TMap<uint8, UNiagaraComponent*> NiagaraEmitters;

	/*// 머티리얼 타입별 파괴 위치 큐
	TMap<uint8, TArray<FVector>> MaterialDestructionPositions;*/
private:
	void InitDestructionFX();

	bool bIsInitialized;
public:
	/** Immediately spawn destruction FX at given locations */
	UFUNCTION(BlueprintCallable, Category = "DestructionFX")
	void TriggerDestructionFXImmediate(uint8 MaterialType, const TArray<FVector>& Locations,const int SpawnCount=0);

public:
	
	/** Current player reference location for distance-based processing */
	FVector ReferenceLocation;

	// === Blueprint 인터페이스 ===
	//나중에 다른 정보나 배열로 바꿀 수 있음
	//Chunk 등에서 카메라 기반으로 처리할 때 사용
	/** Cached reference to player camera (used for LOD calculations) */
	UPROPERTY()
	class APlayerCameraManager* CachedCameraManager;

	// === 런타임 지형 갱신 ===
	
	/** 지형 새로고침 */
	UFUNCTION(BlueprintCallable, Category = "Cave System")
	void RefreshTerrain(bool bForceRegeneration = false);

	/** 특정 범위 내 청크들 새로고침 */
	UFUNCTION(BlueprintCallable, Category = "Cave System")
	void RefreshChunksInRadius(const FVector& Center, float Radius);

	/** 특정 영역 내 청크들 새로고침 */
	UFUNCTION(BlueprintCallable, Category = "Cave System")
	void RefreshChunksInBounds(const FBox& Bounds);

	// 거리 기반 LOD 업데이트
	// 메시 생성 및 로드
	/** Updates terrain LODs based on ReferenceLocation */
	UFUNCTION(BlueprintCallable,Category="LOD")
	void UpdateLOD();

	/** 플레이어가 소속한 청크의 좌표를 반환 */
	FIntVector GetReferenceChunkCoord() const;
	
	// === Debug Draw Settings ===
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebugPoint = false;

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebugGradient = false;

	UPROPERTY(EditAnywhere, Category = "Debug", meta=(ClampMin="1"))
	int PointSamplingStride = 4;

	UPROPERTY(EditAnywhere, Category = "Debug", meta=(ClampMin="-1", ClampMax="1"))
	float IsoSurfaceLevelForDebug = 0;

	UPROPERTY(EditAnywhere, Category = "Debug", meta=(ClampMin="0", ClampMax="255"))
	uint8 MaterialTypeForDebug = 0;

	UPROPERTY(EditAnywhere, Category = "Debug", meta=(ClampMin="1"))
	int GradientSamplingStride = 4;

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawBounds = false;

	UPROPERTY(EditAnywhere, Category = "Performance")
	bool bLogTimeBudget = false;

	// === 이벤트 ===
	
	/** 지형 새로고침 시 호출되는 이벤트 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTerrainRefreshed, const TArray<FIntVector>&, RefreshedChunks);
	/** Called when terrain is refreshed */
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnTerrainRefreshed OnTerrainRefreshed;
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;;

	virtual void Tick(float DeltaTime) override;
	virtual void PostInitializeComponents()override;

private:
	
	// === 청크 재생성 관련 ===
	
	/** 재생성이 필요한 청크들 */
	UPROPERTY()
	TSet<FIntVector> ChunksToRegenerate;
	
	/** 청크 재생성 처리 */
	void ProcessChunkRegeneration();
	
	/** 청크들을 재생성 대상으로 마킹 */
	void MarkChunksForRegeneration(const TArray<FIntVector>& ChunkCoords);

	// === 기존 메서드들 ===
	
	/** 디스크 저장/로드 */
	void SaveChunkToDisk(const FIntVector& ChunkCoord);
	void LoadChunkFromDisk(const FIntVector& ChunkCoord);
	
	/** 디버그 드로잉 */
	void DrawDebugBounds();
	void DrawDebugPoints();
	void DrawDebugGradients();
	
public:
	/** Material asset list indexed by MaterialType (uint8) */
	UPROPERTY(EditAnywhere, Category="Terrain", meta=(ToolTip="TerrainDatas used for different voxel types"))
	FDataTableRowHandle TerrainDataInfo;

	int32 TerrainDataCount;
	TArray<FTerrainData*> TerrainDatas;

public:
	UMaterialInterface* GetMaterial(uint8 Type) const;
	uint8 GetNumMaterials() const;
};