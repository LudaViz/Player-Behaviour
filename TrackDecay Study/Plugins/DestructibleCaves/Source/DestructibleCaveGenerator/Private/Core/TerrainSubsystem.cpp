// Copyright 2025 J2K2. All Rights Reserved.

#include "Core/TerrainSubsystem.h"

#include "EngineUtils.h"
#include "Core/TerrainManager.h"
#include "GameFramework/InputSettings.h"
#include "Stats/GameplayStatsManager.h"

void UTerrainSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	StatsManager = NewObject<UGameplayStatsManager>(this);
	SetupInput();
}

void UTerrainSubsystem::Deinitialize()
{
	if (IsValid(TerrainManager))
	{
		//TerrainManager->RemoveFromRoot();
		TerrainManager = nullptr;
	}

	Super::Deinitialize();
}
ATerrainManager* UTerrainSubsystem::GetTerrainManager()
{
	if (!IsValid(TerrainManager))
	{
		UWorld* World = GetWorld();
		if (!World || !World->IsGameWorld()) return nullptr;

		for (TActorIterator<ATerrainManager> It(World); It; ++It)
		{
			TerrainManager = *It;
			break; // 첫 번째 TerrainManager만 등록
		}
	}
	return TerrainManager;
}
void UTerrainSubsystem::SetupInput()
{
	if (UInputSettings* Settings = const_cast<UInputSettings*>(GetDefault<UInputSettings>()))
	{
		// Axis Mappings
		TArray<FInputAxisKeyMapping> AxisMappings = {
			{ "MoveUp", EKeys::E,            1.0f },
			{ "MoveUp", EKeys::LeftControl, -1.0f },
			{ "MoveUp", EKeys::Q,           -1.0f },
			{ "MoveUp", EKeys::SpaceBar,     1.0f }
		};

		for (const FInputAxisKeyMapping& Mapping : AxisMappings)
		{
			if (!Settings->GetAxisMappings().Contains(Mapping))
			{
				Settings->AddAxisMapping(Mapping);
			}
		}

		/*// Action Mappings
		TArray<FInputActionKeyMapping> ActionMappings = {
			{ "ProfileCommand", EKeys::P },
			{ "ToggleGhostMode", EKeys::R },
			{ "ToggleLight", EKeys::F },
			{"Toggle Collision",EKeys::C}
		};

		for (const FInputActionKeyMapping& Mapping : ActionMappings)
		{
			if (!Settings->GetActionMappings().Contains(Mapping))
			{
				Settings->AddActionMapping(Mapping);
			}
		}*/

		Settings->SaveKeyMappings();
	}
}
