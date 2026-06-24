// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/DCGMacros.h"
#include "TimeBudgetSystem.generated.h"
/**
 * Enum representing distinct task categories used in time budgeting.
 */
UENUM(BlueprintType)
enum class ETimeBudgetSection : uint8
{
	ChunkManager_UnloadChunks UMETA(DisplayName = "Unload Chunks", ToolTip = "Time spent unloading chunks to pool from memory to reduce memory usage."),

	ChunkManager_LoadChunks UMETA(DisplayName = "Load Chunks", ToolTip = "Time allocated for loading new chunks from pool including registration."),

	Scheduler_RenderFrustum UMETA(DisplayName = "Render Frustum", ToolTip = "Update or generate mesh for chunks visible in the current camera view (frustum)."),

	Scheduler_RenderNonFrustum UMETA(DisplayName = "Render Non-Frustum", ToolTip = "Prepare chunks not currently visible."),
	
	Scheduler_BackgroundHigh UMETA(DisplayName = "Background High Priority", ToolTip = "Prepare and generate mesh for new chunks. Generate close mesh first."),

	Scheduler_BackgroundLow UMETA(DisplayName = "Background Low Cost", ToolTip = "Prepare and generate mesh for new chunks. Generate small mesh first."),


	Max UMETA(Hidden)
};

/**
 * Configuration struct for assigning budget per frame to different systems.
 */
USTRUCT(BlueprintType)
struct FTimeBudgetConfig
{
	GENERATED_BODY()

	// 총 사용 가능한 시간(ms)
	/**
	 * Total time budget per frame in milliseconds. Ignored if bUseFixedTimes is true.
	 * If set to 0, all time limits are disabled.
	 */
	UPROPERTY(EditAnywhere, Category = "Budget", meta=(ToolTip="Total frame time budget in milliseconds. Ignored if bUseFixedTimes is true. If 0, disables all time limits.",EditCondition ="!bUseFixedTimes"))
	float TotalBudgetMS = 6.0f;
	
	/**
	 * Whether to interpret SectionWeights as fixed milliseconds.
	 * If true, each value in SectionWeights is interpreted as absolute time in milliseconds.
	 * If false, values are interpreted as relative weights, and TotalBudgetMS is distributed proportionally.
	 */
	UPROPERTY(EditAnywhere, Category = "Budget", meta=(ToolTip="If true, SectionWeights values are fixed milliseconds. If false, they are proportional weights distributed from TotalBudgetMS."))
	bool bUseFixedTimes = false;
	
	// 각 섹션별 가중치 (전체 합은 1.0으로 가정)
	/**
	 * Proportional weights or fixed time values (ms) for each section, depending on bUseFixedTimes.
	 * If a section's value is 0, its time limit is considered unlimited.
	 */
	UPROPERTY(EditAnywhere, Category = "Budget", meta=(ToolTip="Weight or time value (ms) per section. If a value is 0, that section has no time limit."))
	TMap<ETimeBudgetSection, float> SectionWeights;

	UPROPERTY(EditAnywhere, Category = "Budget")
	bool bAllowCarryOver = true;
	
	FTimeBudgetConfig()
	{
		SectionWeights.Add(ETimeBudgetSection::ChunkManager_UnloadChunks,1.0f);
		SectionWeights.Add(ETimeBudgetSection::ChunkManager_LoadChunks,1.0f);
		SectionWeights.Add(ETimeBudgetSection::Scheduler_RenderFrustum, 3.0f);
		SectionWeights.Add(ETimeBudgetSection::Scheduler_RenderNonFrustum, 1.0f);
		SectionWeights.Add(ETimeBudgetSection::Scheduler_BackgroundLow, 0.5f);
		SectionWeights.Add(ETimeBudgetSection::Scheduler_BackgroundHigh, 0.5f);
	}
};
/**
 * Runtime tracking of how much time each system has spent during a frame.
 */
USTRUCT(BlueprintType)
struct FTimeBudgetState
{
	GENERATED_BODY()

private:
	UPROPERTY()
	TMap<ETimeBudgetSection, double> StartTimes;
	UPROPERTY()
	TMap<ETimeBudgetSection, float> TimeSpent;
	UPROPERTY()
	TMap<ETimeBudgetSection, float> TimeLimits;
	bool bAllowCarryOver = false;
	float EffectiveCarryOver = 0.f; // 현재 프레임 내에서 carry 되는 시간(ms)
	double FrameStartTime = 0.0;

	float TotalBudgetUsed = 0.f;
	float TotalBudgetAllocated = 0.f;

	//로그 저장용.
	UPROPERTY()
	TMap<ETimeBudgetSection, float> CarryOverAtStart;
public:
	void Initialize(const FTimeBudgetConfig& Config)
	{
		TimeLimits.Empty();
		TimeSpent.Empty();
		StartTimes.Empty();
		bAllowCarryOver = Config.bAllowCarryOver;

		FrameStartTime = FPlatformTime::Seconds();

		const int32 SectionCount = static_cast<int32>(ETimeBudgetSection::Max);
		if (Config.SectionWeights.Num() != SectionCount)
		{
			UE_LOG(LogDCG, Warning, TEXT("FTimeBudgetState::Initialize - SectionWeights size mismatch. Expected %d, got %d"), SectionCount, Config.SectionWeights.Num());
			return;
		}
		if (Config.bUseFixedTimes)
		{
			for (auto& Pair : Config.SectionWeights)
			{
				TimeLimits.Add(Pair.Key, Pair.Value);
				TimeSpent.Add(Pair.Key, 0.f);
			}
		}
		else
		{
			// 총 가중치 합산
			float TotalWeight = 0.f;
			for (auto Index : Config.SectionWeights)
			{
				TotalWeight += Index.Value;
			}
			if (TotalWeight <= 0.f)
			{
				UE_LOG(LogDCG, Warning, TEXT("FTimeBudgetState::Initialize - Total weight is zero or negative"));
				return;
			}

			// 비례 기반 타임 리밋 계산
			for (auto& Pair : Config.SectionWeights)
			{
				float Ratio = Pair.Value / TotalWeight;
				float TimeLimitMS = Config.TotalBudgetMS * Ratio;
				TimeLimits.Add(Pair.Key, TimeLimitMS);
				TimeSpent.Add(Pair.Key, 0.f);
			}
		}
		
	}
	void BeginFrame()
	{
		FrameStartTime = FPlatformTime::Seconds();
		EffectiveCarryOver = 0.f;
		TotalBudgetUsed = 0.f;
		TotalBudgetAllocated = 0.f;
		
		// Clear per-frame tracking data
		StartTimes.Empty();
		TimeSpent.Empty();
		CarryOverAtStart.Empty();
	}
	void BeginSection(ETimeBudgetSection Section)
	{
		StartTimes.Add(Section, FPlatformTime::Seconds());
		TimeSpent.FindOrAdd(Section) = 0.f; // 초기화
		// 로그용으로 시작 시점의 CarryOver 저장
		CarryOverAtStart.FindOrAdd(Section) = EffectiveCarryOver;
	}

	void EndSection(ETimeBudgetSection Section)
	{
		const double Now = FPlatformTime::Seconds();
		if (double* StartPtr = StartTimes.Find(Section))
		{
			float Elapsed = static_cast<float>((Now - *StartPtr) * 1000.0);
			TimeSpent.FindOrAdd(Section) += Elapsed;
			StartTimes.Remove(Section);
		}

		if (bAllowCarryOver)
		{
			const float* BaseLimit = TimeLimits.Find(Section);
			if (BaseLimit)
			{
				// 이 섹션에 할당된 예산과 실제 사용량을 전체 누적값에 반영
				TotalBudgetAllocated += *BaseLimit;
				TotalBudgetUsed += TimeSpent.FindRef(Section);

				// CarryOver는 전체 할당량과 전체 사용량의 차이
				EffectiveCarryOver = TotalBudgetAllocated - TotalBudgetUsed;
			}
		}
		else
		{
			EffectiveCarryOver = 0.f; // CarryOver를 사용하지 않으면 항상 0
		}
	}


	bool ShouldContinue(ETimeBudgetSection Section) const
	{
		const float* Limit = TimeLimits.Find(Section);
		if (!Limit || *Limit <= 0) return true; // 예산이 0 이하면 무제한

		// 현재까지 Pause/Resume으로 누적된 시간
		float TotalElapsed = TimeSpent.FindRef(Section); 

		// 현재 섹션이 활성화(running) 상태라면, 마지막 Resume 이후 시간 추가
		if (const double* StartTime = StartTimes.Find(Section))
		{
			const double Now = FPlatformTime::Seconds();
			TotalElapsed += static_cast<float>((Now - *StartTime) * 1000.0f);
		}
    
		// 사용 가능한 보너스 시간 (CarryOver는 음수일 수 없으므로 0 이상만 사용)
		const float Bonus = EffectiveCarryOver > 0.f ? EffectiveCarryOver : 0.f;

		return TotalElapsed < (*Limit + Bonus);
	}
	
	void PauseSection(ETimeBudgetSection Section)
	{
		const double Now = FPlatformTime::Seconds();
		if (double* StartPtr = StartTimes.Find(Section))
		{
			float Elapsed = static_cast<float>((Now - *StartPtr) * 1000.0);
			TimeSpent.FindOrAdd(Section) += Elapsed;
			StartTimes.Remove(Section);
		}
	}
	void ResumeSection(ETimeBudgetSection Section)
	{
		// PauseSection에서 StartTimes가 제거되므로, 재시작 시 추가
		if (!StartTimes.Contains(Section))
		{
			StartTimes.Add(Section, FPlatformTime::Seconds());
		}
	}

	float GetRemainingMS(ETimeBudgetSection Section) const
	{
		const float* Spent = TimeSpent.Find(Section);
		const float* Limit = TimeLimits.Find(Section);
		if (Spent && Limit)
		{
			return *Limit - *Spent;
		}
		return 0.0f;
	}
	void LogSectionTimes() const
	{
		UE_LOG(LogDCG, Display, TEXT("[TimeBudget] --- Frame Time Budget ---"));
		UE_LOG(LogDCG, Display, TEXT("%.2f/%.2f Used"),TotalBudgetUsed,TotalBudgetAllocated);
		
		for (int32 i = 0; i < static_cast<int32>(ETimeBudgetSection::Max); ++i)
		{
			ETimeBudgetSection Section = static_cast<ETimeBudgetSection>(i);

			const float* Spent = TimeSpent.Find(Section);
			const float* Limit = TimeLimits.Find(Section);
			const float* InitialBonus = CarryOverAtStart.Find(Section);

			const FString SectionName = StaticEnum<ETimeBudgetSection>()->GetNameStringByValue(i);

			if (Spent && Limit)
			{
				float Bonus = InitialBonus ? *InitialBonus : 0.f;
				FString BonusStr = Bonus >= 0.f
					? FString::Printf(TEXT("+%.2f ms"), Bonus)
					: FString::Printf(TEXT("-%.2f ms"), -Bonus);

				UE_LOG(LogDCG, Display, TEXT("  [%s] Spent: %.2f ms / Limit: %.2f ms (CarryOver: %s)"),
					*SectionName, *Spent, *Limit, *BonusStr);
			}
			else
			{
				UE_LOG(LogDCG, Warning, TEXT("  [%s] No data (Spent or Limit missing)"), *SectionName);
			}
		}
	}

};
