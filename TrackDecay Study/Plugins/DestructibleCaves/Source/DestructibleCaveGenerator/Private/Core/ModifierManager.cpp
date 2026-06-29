// Copyright 2025 J2K2. All Rights Reserved.

#include "Core/ModifierManager.h"

#include "Stats/GameplayStatsManager.h"
#include "Chunk/TerrainChunk.h"
#include "Components/ShapeComponent.h"
#include "Components/SphereComponent.h"
#include "Core/ChunkManager.h"
#include "Core/TerrainManager.h"
#include "Core/TerrainSubsystem.h"
#include "Destruct/DestructTypes.h"
#include "Engine/World.h"
#include "Async/ParallelFor.h"
#include "Async/Async.h"
void UModifierManager::Initialize(class ATerrainManager* InTerrainManager)
{
	TerrainManager = InTerrainManager;
}
void UModifierManager::Tick(float DeltaTime)
{
	//Super::Tick(DeltaTime);

	if (RetryList.IsEmpty()) return;
	//RetryList.Empty();
	TArray<FPendingDigOperation> RemainingRetries;

	for (FPendingDigOperation& Op : RetryList)
	{
		TSet<FIntVector> FailedCoords;
		// 아직 실패한 청크만 대상으로 재시도
		const int32 RetryResult = DigWithSphere(Op.Sphere, Op.Strength, FailedCoords,&Op.FailedCoords);
		
		
		// 일부라도 실패했으면 다시 시도해야 함
		if (RetryResult > 0 && !FailedCoords.IsEmpty())
		{
			RemainingRetries.Add(FPendingDigOperation(Op.Sphere, FailedCoords, Op.Strength));
		}
	}

	RetryList = MoveTemp(RemainingRetries);
}

int32 UModifierManager::DigWithShape(const UShapeComponent* ShapeComponent, float Strength)
{
	FSphere Sphere(ShapeComponent->GetComponentLocation(),Cast<USphereComponent>(ShapeComponent)->GetScaledSphereRadius());
	return DigWithSphere(Sphere,Strength);
}

int32 UModifierManager::DigWithSphere(const FSphere& Sphere, float Strength)
{
	TSet<FIntVector> FailedCoords;
	int32 Result = DigWithSphere(Sphere,Strength,FailedCoords);
	RetryList.Add({Sphere,FailedCoords,Strength});
	return Result;
}

int32 UModifierManager::DigWithSphere(const FSphere& Sphere, float Strength,TSet<FIntVector>& FailedCoords,const TSet<FIntVector>* MaskingCoords)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UModifierManager::DigWithShape)
	if (!TerrainManager)
		return 0;

	UChunkManager* ChunkManager = TerrainManager->ChunkManager;
	if (!ChunkManager)
		return 0;

	/*// 1. Shape Bounds 계산
	const FSphere Sphere(ShapeComponent->GetComponentLocation(),
	                     Cast<USphereComponent>(ShapeComponent)
		                     ? Cast<USphereComponent>(ShapeComponent)->GetScaledSphereRadius()
		                     : 100.0f);*/
	const FBox SphereBox = FBox(Sphere.Center - FVector(Sphere.W), Sphere.Center + FVector(Sphere.W));
	// 2. 포함되는 Coord 계산, 나중에 별도 함수로 분리


	int TotalModifiedCount = 0;
	TArray<int32> ModifiedCountThd;
	TArray<FIntVector> AllCoords = ChunkManager->GetChunkCoordsInAABB(SphereBox).Array();
	TArray<FIntVector> AffectedCoords;
	
	AffectedCoords.Reserve(AllCoords.Num());

	for (const FIntVector& Coord : AllCoords)
	{
		if (!MaskingCoords || MaskingCoords->Contains(Coord))
		{
			AffectedCoords.Add(Coord);
		}
	}
	ModifiedCountThd.SetNum(AffectedCoords.Num());

	FCriticalSection DestructionQueueLock;
	TMap<FIntVector, TQueue<FDestructionEvent, EQueueMode::Mpsc>*> ChunkEventQueues;
	//TODO: 여기 다른 Shape도 추가하기
	//일단 Sphere로 하드코딩
	// 3. 각 Chunk에 대해 Shape 내부에 포함되는 Voxel을 수정 (병렬화)
	FCriticalSection FailedSection;
	ParallelFor(AffectedCoords.Num(), [&](int32 Index)
	{
		int32 LocalCount = 0;
		FIntVector ChunkCoord = AffectedCoords[Index];
		ATerrainChunk* Chunk = nullptr;
		if (ChunkManager->ActiveChunks.Contains(ChunkCoord))
		{
			Chunk = ChunkManager->ActiveChunks[ChunkCoord];
		}
		/*if (Chunk)
		{
			auto ChunkState = Chunk->GetChunkState();
			//Complete 시에만 부술지, 아니면 Idle/Filling 아니면 다 부술지 생각해봐야함
			//if (ChunkState < ETerrainChunkState::Modified)return;
		}*/
		FChunkData* ChunkData = ChunkManager->GetChunkData(ChunkCoord);
		if (!ChunkData)
		{
			{
				FScopeLock Lock(&FailedSection);
				FailedCoords.Add(ChunkCoord);
			}
			return;
		}
		//로컬 변수 수정 후 옮기기
		FChunkData LocalChunkData = *ChunkData;
		TQueue<FDestructionEvent, EQueueMode::Mpsc>* EventQueue = new TQueue<FDestructionEvent, EQueueMode::Mpsc>();
		if (LocalChunkData.ModifyVoxelsBySphere(Sphere, ChunkManager->VoxelSize, ChunkCoord, Strength,
		                                       EventQueue))
		{
			//Chunk->MarkRebuildNeeded();
			if (Chunk)Chunk->SetChunkState(ETerrainChunkState::Modified);
			{
				FScopeLock Lock(&DestructionQueueLock);
				ChunkEventQueues.Add(ChunkCoord, EventQueue);
			}
			//안전하게 복사하는 다른 방법 있는지
			{
				FScopeLock Lock(&ChunkManager->ChunkDataMap.GetCriticalSection());
				*ChunkData = LocalChunkData;
			}
			{
				//파괴된 Coord 목록 저장
				FScopeLock Lock(&TerrainManager->ChunkManager->ChunkCoordsToSaveLock);
				TerrainManager->ChunkManager->ChunkCoordsToSave.Add(ChunkCoord);
			}
			LocalCount++;
		}
		else
		{
			delete EventQueue;
		}

		ModifiedCountThd[Index] = LocalCount;
	}); // 필요시 true로 변경

	for (int32 Count : ModifiedCountThd)
	{
		TotalModifiedCount += Count;
	}

	// 4. 통계 누적 등 (예: StatsManager)
	if (TotalModifiedCount > 0)
	{
		if (UWorld* World = TerrainManager->GetWorld())
		{
			if (UTerrainSubsystem* Subsystem = World->GetSubsystem<UTerrainSubsystem>())
			{
				if (UGameplayStatsManager* Stats = Subsystem->GetStatsManager())
				{
					Stats->Destruction.RegisterDig(TotalModifiedCount, ChunkManager->VoxelSize);
				}
			}
		}
	}

	AsyncTask(ENamedThreads::GameThread, [this,ChunkEventQueues,Sphere]()
	{
		TArray<FDestructionEventContext> EventContexts;
		//TArray<FDestructionEvent> AllEvents;

		for (const auto& Pair : ChunkEventQueues)
		{
			const FIntVector ChunkCoord = Pair.Key;
			TQueue<FDestructionEvent, EQueueMode::Mpsc>* Queue = Pair.Value;
			if (!Queue)
			{
				continue; // nullptr 보호
			}
			FDestructionEventContext Context;
			Context.ChunkCoord = ChunkCoord;
			Context.ShapeWorldCenter = Sphere.Center;
			Context.ShapeRadius = Sphere.W;
			TMap<uint8, TArray<FVector>> MaterialToPositions;

			//Context.Events.Add(Event); // FDestructionEvent
			//TArray<FDestructionEvent> Events;
			FDestructionEvent E;
			while (Queue->Dequeue(E))
			{
				Context.Events.Add(E);
				MaterialToPositions.FindOrAdd(E.MaterialType).Add(E.VoxelWorldPosition);
			}
			delete Queue;
			// TMap -> TArray<FPerMaterialVoxels> 변환
			for (const auto& MatPair : MaterialToPositions)
			{
				FPerMaterialVoxels Entry;
				Entry.MaterialType = MatPair.Key;
				Entry.VoxelPositions = MatPair.Value;
				Context.PerMaterialVoxelPositions.Add(Entry);
			}
			if (Context.Events.Num() > 0)
			{
				/*UE_LOG(LogChunk, Warning, TEXT("BroadCast:(%d,%d,%d)%d"), ChunkCoord.X, ChunkCoord.Y, ChunkCoord.Z,
				       Context.Events.Num());*/
				OnVoxelDestruction.Broadcast(Context);
				EventContexts.Add(Context);
			}
		}
		if (EventContexts.Num() > 0)
		{
			//UE_LOG(LogChunk, Warning, TEXT("Global BroadCast:%d"), EventContexts.Num());
			OnVoxelDestructionGlobal.Broadcast(EventContexts);
		}
	});

	//UE_LOG(LogTemp, Log, TEXT("TotalModifiedCount = %d"), TotalModifiedCount);
	//PROFILE_END("DigWithShape");
	return TotalModifiedCount;
}
