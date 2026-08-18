// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/GameplayStatsManager.h"
#include "UObject/Object.h"
#include "Subsystems/WorldSubsystem.h"
#include "TerrainSubsystem.generated.h"

class ATerrainManager;
/**
 * 
 */
/**
 * 지형 시스템의 월드 단위 Subsystem.
 * UTerrainManager를 생성 및 소유하며, 외부에서 접근 가능한 진입점 역할.
 */
UCLASS()
class DESTRUCTIBLECAVEGENERATOR_API UTerrainSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(class FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** TerrainManager 반환 */
	UFUNCTION(BlueprintCallable, Category = "Terrain")
	ATerrainManager* GetTerrainManager();

	void SetupInput();

	UFUNCTION(BlueprintCallable,Category = "Terrain Subsystem")
	UGameplayStatsManager* GetStatsManager() const { return StatsManager; }
private:
	/** 지형을 제어하는 메인 매니저 */
	UPROPERTY()
	ATerrainManager* TerrainManager;
	UPROPERTY()
	UGameplayStatsManager* StatsManager;
};