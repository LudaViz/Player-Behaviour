// Copyright 2025 J2K2. All Rights Reserved.

#include "Destruct/TerrainDestructComponent.h"

#include "Core/TerrainSubsystem.h"
#include "Engine/World.h"
#include "Core/ChunkManager.h"
#include "Core/ModifierManager.h"
#include "Core/TerrainManager.h"

UTerrainDestructComponent::UTerrainDestructComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickInterval = 0.05f;
}

void UTerrainDestructComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UTerrainDestructComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ChunkManager)
	{
		ChunkManager->GetChunkCoordsInRange(CoordsInFillDistance, GetComponentLocation(), FillDistance);
		for (FIntVector& It : CoordsInFillDistance)
		{
			ChunkManager->AddToFillQueue(It);
		}
	}
}

int32 UTerrainDestructComponent::ApplyDestruction()
{
	if (!ModifierManager)
	{
		if (UWorld* World = GetWorld())
		{
			if (UTerrainSubsystem* TerrainSubsystem = World->GetSubsystem<UTerrainSubsystem>())
			{
				if (ATerrainManager* TerrainManager = TerrainSubsystem->GetTerrainManager())
				{
					ModifierManager = TerrainManager->ModifierManager;
					ChunkManager = TerrainManager->ChunkManager;
					switch (DestructShape.ShapeType)
					{
					case ETerrainDestructShapeType::Sphere:
						FillDistance = DestructShape.Radius / (CHUNK_SIZE * ChunkManager->VoxelSize) + 1;
						break;
					default:
						break;
					}
				}
			}
		}
	}

	if (!ModifierManager)
	{
		UE_LOG(LogDCG, Warning, TEXT("UTerrainDestructComponent: Failed to find ModifierManager"));
		return 0;
	}

	const FVector Center = GetComponentLocation();

	switch (DestructShape.ShapeType)
	{
	case ETerrainDestructShapeType::Sphere:
		return ModifierManager->DigWithSphere(FSphere(Center, DestructShape.Radius), Strength);

	/*case ETerrainDestructShapeType::Box:
		{
			const FTransform ShapeTransform = GetComponentTransform();
			//return ModifierManager->DigBox(Center, DestructShape.HalfExtent, ShapeTransform.GetRotation(), Strength);
		}*/

	default:
		UE_LOG(LogDCG, Warning, TEXT("UTerrainDestructComponent: Unknown shape type."));
		break;
	}

	return 0;
}

void UTerrainDestructComponent::OnRegister()
{
	Super::OnRegister();
}
