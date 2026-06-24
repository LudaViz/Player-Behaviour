// Copyright 2025 J2K2. All Rights Reserved.

#include "Core/ChunkManager.h"
#include "Chunk/ChunkPool.h"
#include "Core/TerrainManager.h"
#include "Core/Scheduler/ChunkScheduler.h"
#include "DrawDebugHelpers.h"
#include "Async/ParallelFor.h"
#include "Misc/FileHelper.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Utils/ChunkUtils.h"

UChunkManager::UChunkManager()
{
}

void UChunkManager::Initialize(ATerrainManager* InManager)
{
	OwnerManager = InManager;
	//OwnerManager->GetRootComponent()->SetWorldScale3D(FVector(VoxelSize));
	ChunkScheduler = NewObject<UChunkScheduler>(this);
	ChunkScheduler->SetTerrainManger(OwnerManager);
	// 멤버 초기화
	ChunkPool = NewObject<UChunkPool>(this);
	if (ChunkPool)
	{
		ChunkPool->Initialize(InitialPoolSize, INT_MAX, pow(RenderDistance * 2 + 1, 3), 2,
		                      ATerrainChunk::StaticClass());
	}
	ChunkOctree = nullptr; // 필요시 Octree 구현 및 초기화
	// ChunkSize = 32 * ChunkSizeLevel;
	FillDistance = RenderDistance + 2;
	//FillDistance = RenderDistance;
	//LoadChunksFromDisk("TestWorld");
	//UE_LOG(LogChunk, Warning, TEXT("FillDist=%d"), FillDistance)
}

void UChunkManager::ProcessChunks()
{
	ChunkPool->TickResize();
}

FChunkData* UChunkManager::GetChunkData(const FIntVector& ChunkCoord)
{
	if (FChunkData** Found = ChunkDataMap.Find(ChunkCoord))
	{
		return *Found; // 포인터 반환
	}
	return nullptr;
}

void UChunkManager::MarkAsRebuildNeeded(const FIntVector& ChunkCoord)
{
}

ATerrainChunk* UChunkManager::LoadChunk(const FIntVector& ChunkCoord)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("LoadChunk")
	if (!ChunkPool) return nullptr;

	// 이미 활성 청크가 있다면 반환
	if (ActiveChunks.Contains(ChunkCoord))
		return ActiveChunks[ChunkCoord];

	if (ATerrainChunk* ChunkActor = ChunkPool->RentChunk())
	{
		return ChunkActor;
	}

	// 풀에 여유가 없으면 PendingChunks에 추가
	// PendingChunks.Enqueue(ChunkCoord);
	UE_LOG(LogDCG, Warning, TEXT("Pool is empty."))
	return nullptr;
}

void UChunkManager::UnloadChunk(const FIntVector& ChunkCoord)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UChunkManager::UnloadChunk)
	// 액터 비활성화 및 풀 반환
	if (ATerrainChunk** ChunkPtr = ActiveChunks.Find(ChunkCoord))
	{
		if (ATerrainChunk* Chunk = *ChunkPtr)
		{
			//Chunk->SetChunkState(ETerrainChunkState::Unloaded);
			Chunk->SetActive(false);
			if (ChunkPool) ChunkPool->ReturnChunk(Chunk);
		}
		ActiveChunks.Remove(ChunkCoord);
	}
}

void UChunkManager::LoadInitialChunks(const FVector& InitialLocation, class UNoiseGenerator* NoiseGenerator)
{
	if (!bInitialChunkLoad) return;
	UpdateChunks(InitialLocation, NoiseGenerator);
}

void UChunkManager::UpdateChunkStreaming(const FVector& ReferenceLocation, UNoiseGenerator* NoiseGenerator)
{
	if (!bUpdateStreaming)
	{
		return;
	}

	UpdateChunks(ReferenceLocation, NoiseGenerator);
}

FIntVector UChunkManager::WorldToChunkCoord(const FVector& WorldLocation) const
{
	// 월드 좌표 → 청크 좌표 변환 (음수 좌표도 정렬되도록)
	return FIntVector(
		FMath::FloorToInt(WorldLocation.X / (CHUNK_SIZE * VoxelSize)),
		FMath::FloorToInt(WorldLocation.Y / (CHUNK_SIZE * VoxelSize)),
		FMath::FloorToInt(WorldLocation.Z / (CHUNK_SIZE * VoxelSize))
	);
}

FVector UChunkManager::ChunkToWorldLocation(const FIntVector& ChunkCoord) const
{
	// 청크 좌표 → 월드 좌표 변환 (청크의 시작점 위치)
	return FVector(
		ChunkCoord.X * CHUNK_SIZE * VoxelSize,
		ChunkCoord.Y * CHUNK_SIZE * VoxelSize,
		ChunkCoord.Z * CHUNK_SIZE * VoxelSize
	);
}

TSet<FIntVector> UChunkManager::GetChunkCoordsInAABB(FBox Box) const
{
	TSet<FIntVector> AffectedCoordsSet;
	FIntVector MinChunkCoord = WorldToChunkCoord(Box.Min);
	FIntVector MaxChunkCoord = WorldToChunkCoord(Box.Max);

	for (int32 z = MinChunkCoord.Z; z <= MaxChunkCoord.Z; ++z)
		for (int32 y = MinChunkCoord.Y; y <= MaxChunkCoord.Y; ++y)
			for (int32 x = MinChunkCoord.X; x <= MaxChunkCoord.X; ++x)
			{
				FIntVector ChunkCoord(x, y, z);
				FBox ChunkBound = FBox(
					ChunkToWorldLocation(ChunkCoord),
					ChunkToWorldLocation(ChunkCoord + FIntVector(1, 1, 1))
				);

				if (ChunkBound.Intersect(Box))
					AffectedCoordsSet.Add(ChunkCoord);
			}

	return AffectedCoordsSet;
}

void UChunkManager::GetChunkCoordsInRange(TSet<FIntVector>& OutCoords, const FVector& Center, int32 DistanceInChunks)
{
	// 중심 위치에서 ChunkCoord 계산
	const FIntVector CenterChunk = WorldToChunkCoord(Center);

	// 거리 범위 반복
	for (int32 z = -DistanceInChunks; z <= DistanceInChunks; ++z)
		for (int32 y = -DistanceInChunks; y <= DistanceInChunks; ++y)
			for (int32 x = -DistanceInChunks; x <= DistanceInChunks; ++x)
			{
				FIntVector ChunkCoord = CenterChunk + FIntVector(x, y, z);
				OutCoords.Add(ChunkCoord);
			}
}

TArray<ATerrainChunk*> UChunkManager::GetChunksInRadius(const FVector& Center, float Radius)
{
	TArray<ATerrainChunk*> Result;
	FSphere Sphere(Center, Radius);

	// Octree 사용 시 Octree로 조회
	// if (ChunkOctree) { ChunkOctree->Query(Center, Radius, Result); return Result; }

	// Octree 미사용 시 순회

	ActiveChunks.ForEach([&](const FIntVector& Key, ATerrainChunk* Chunk)
	{
		if (!Chunk) return;

		FVector Origin = ChunkToWorldLocation(Chunk->ChunkCoordinate);
		FVector Extent = FVector(CHUNK_SIZE * VoxelSize);

		FBox ChunkBox(Origin - Extent, Origin + Extent);
		// 일단 AABB끼리 검사. 나중에 Sphere-Box 비교 찾으면 바꾸면 될듯
		if (FBoxSphereBounds::SpheresIntersect(FBoxSphereBounds(ChunkBox), FBoxSphereBounds(Sphere)))
		{
			Result.Add(Chunk);
		}
		// FVector ChunkLocation = Chunk->GetActorLocation();
		// if (FVector::Dist(ChunkLocation, Center) <= Radius)
		// {
		//     Result.Add(Chunk);
		// }
	});
	return Result;
}

TArray<ATerrainChunk*> UChunkManager::GetChunksInBox(const FBox& Box)
{
	//PROFILE_BEGIN("GetChunksInBox")
	TArray<ATerrainChunk*> Result;

	ActiveChunks.ForEach([&](const FIntVector& Key, ATerrainChunk* Chunk)
	{
		if (!Chunk) return;

		//Chunk->GetActorBounds(false, Origin, Extent);
		FVector Origin = ChunkToWorldLocation(Chunk->ChunkCoordinate);
		FVector Extent = FVector(CHUNK_SIZE * VoxelSize);
		FBox ChunkBox(Origin - Extent, Origin + Extent);

		// 두 AABB가 겹치는지 검사
		if (Box.Intersect(ChunkBox))
		{
			Result.Add(Chunk);
		}
	});
	////PROFILE_END("GetChunksInBox")
	return Result;
}


void UChunkManager::UpdateChunks(const FVector& ReferenceLocation, UNoiseGenerator* NoiseGenerator)
{
	//PROFILE_BEGIN("UpdateChunks")
	switch (ChunkLoadMode)
	{
	case EChunkLoadMode::BlockingSerial:
		UpdateChunksBlockingSerial(ReferenceLocation, NoiseGenerator);
		break;

	case EChunkLoadMode::BlockingParallel:
		UpdateChunksBlockingParallel(ReferenceLocation, NoiseGenerator);
		break;

	case EChunkLoadMode::Task:
		UpdateChunksTask(ReferenceLocation, NoiseGenerator);
		break;
	case EChunkLoadMode::ScheduledTask:
		UpdateChunksScheduled(ReferenceLocation, NoiseGenerator);
		break;
	// case EChunkLoadMode::Runnable:
	//     UpdateChunksBlockingRunnable(ReferenceLocation, NoiseGenerator);
	//     break;
	//
	// case EChunkLoadMode::GPUBlocking:
	//     UpdateChunksBlockingGPUBlocking(ReferenceLocation, NoiseGenerator);
	//     break;
	//
	// case EChunkLoadMode::GPUTask:
	//     UpdateChunksBlockingGPUTask(ReferenceLocation, NoiseGenerator);
	//     break;
	//
	// case EChunkLoadMode::GPURunnable:
	//     UpdateChunksBlockingGPURunnable(ReferenceLocation, NoiseGenerator);
	//     break;

	default:
		UE_LOG(LogDCG, Error, TEXT("Invalid EChunkLoadMode specified."));
		break;
	}
	////PROFILE_END("UpdateChunks")
}
#pragma region LegacyUpdateChunks
void UChunkManager::UpdateChunksBlockingSerial(const FVector& ReferenceLocation, UNoiseGenerator* NoiseGenerator)
{
	/*TSet<FIntVector> RequiredChunks = GetRequiredChunks(ReferenceLocation, RenderDistance);
	TSet<FIntVector> FillChunks = GetRequiredChunks(ReferenceLocation, FillDistance);*/

	TArray<ATerrainChunk*> ChunksToGenerateMesh;
	TArray<ATerrainChunk*> ChunksToUpdateMesh;

	LoadAndUnloadChunks(NoiseGenerator, ChunksToGenerateMesh, ChunksToUpdateMesh,ReferenceLocation);
	ChunksToGenerateMesh.Append(ChunksToUpdateMesh);
	for (ATerrainChunk* Chunk : ChunksToGenerateMesh)
	{
		if (FChunkData* ChunkData = GetChunkData(Chunk->ChunkCoordinate))
		{
			Chunk->PrepareMesh(0, ChunkData);
			Chunk->UploadMesh();
			//Chunk->GenerateMesh();
			//Chunk->SetActorRelativeScale3D(FVector(VoxelSize, VoxelSize, VoxelSize));
		}
	}
}

void UChunkManager::UpdateChunksBlockingParallel(const FVector& ReferenceLocation, UNoiseGenerator* NoiseGenerator)
{
	/*TSet<FIntVector> RequiredChunks = GetRequiredChunks(ReferenceLocation, RenderDistance);
	TSet<FIntVector> FillChunks = GetRequiredChunks(ReferenceLocation, FillDistance);*/

	TArray<ATerrainChunk*> ChunksToGenerateMesh;
	TArray<ATerrainChunk*> ChunksToUpdateMesh;

	LoadAndUnloadChunks(NoiseGenerator, ChunksToGenerateMesh, ChunksToUpdateMesh,ReferenceLocation);
	ChunksToGenerateMesh.Append(ChunksToUpdateMesh);

	const uint32 NumChunksToGenerateMesh = ChunksToGenerateMesh.Num();
	if (NumChunksToGenerateMesh == 0)
	{
		return;
	}

	// NumThreads == 0 일 경우 청크 당 쓰레드 한 개 할당
	const uint32 NumFetchThreads = NumThreads != 0 ? NumThreads : NumChunksToGenerateMesh;
	const uint32 NumChunksPerThread = NumThreads != 0 ? NumChunksToGenerateMesh / NumFetchThreads + 1 : 1;
	ParallelFor(NumFetchThreads, [&](const int ThreadIdx)
	{
		uint32 StartChunkIdx = ThreadIdx * NumChunksPerThread;
		uint32 EndChunkIdx = FMath::Min(StartChunkIdx + NumChunksPerThread, NumChunksToGenerateMesh);

		for (uint32 ChunkIdx = StartChunkIdx; ChunkIdx < EndChunkIdx; ++ChunkIdx)
		{
			if (ATerrainChunk* Chunk = ChunksToGenerateMesh[ChunkIdx])
			{
				if (FChunkData* ChunkData = GetChunkData(Chunk->ChunkCoordinate))
				{
					Chunk->PrepareMesh(0, ChunkData);
				}
			}
		}
	});


	//PROFILE_BEGIN("Parallel-GenMesh")
	for (ATerrainChunk* Chunk : ChunksToGenerateMesh)
	{
		//PROFILE_SCOPE_TIME("Genmesh-SingleTime")
		Chunk->UploadMesh();
		//Chunk->SetActorRelativeScale3D(FVector(VoxelSize, VoxelSize, VoxelSize));
	}
	//PROFILE_END("Parallel-GenMesh")
}

void UChunkManager::UpdateChunksTask(const FVector& ReferenceLocation, UNoiseGenerator* NoiseGenerator)
{
	//PROFILE_BEGIN("UpdateChunksTask")
	// 완료된 Task 처리
	// 0. 완료된 Task 처리
	uint32 NumTaskDone = 0;
	TArray<UE::Tasks::TTask<ATerrainChunk*>> CompletedTasks;

	//PROFILE_BEGIN("UpdateChunksTask-GetResult")
	// 분리된 처리 루프: IsCompleted 된 것만 추출
	while (!PrepareMeshTasks.IsEmpty())
	{
		UE::Tasks::TTask<ATerrainChunk*> TaskDone;
		if (!PrepareMeshTasks.Peek(TaskDone))
			break;

		if (!TaskDone.IsCompleted())
			break;

		PrepareMeshTasks.Pop();
		CompletedTasks.Add(MoveTemp(TaskDone));
		if (NumMeshGeneratePerTick > 0 && ++NumTaskDone >= NumMeshGeneratePerTick)
			break;
	}

	//PROFILE_END("UpdateChunksTask-GetResult")
	//PROFILE_BEGIN("UpdateChunksTask-GenerateMesh")
	// 안전하게 GetResult (확실히 Completed된 상태)
	for (UE::Tasks::TTask<ATerrainChunk*>& Task : CompletedTasks)
	{
		if (ATerrainChunk* ChunkDone = Task.GetResult())
		{
			ChunkDone->UploadMesh();
			//ChunkDone->SetActorRelativeScale3D(FVector(VoxelSize, VoxelSize, VoxelSize));
		}
	}
	//PROFILE_END("UpdateChunksTask-GenerateMesh")

	//PROFILE_BEGIN("UpdateChunksTask-GetRequiredChunks")
	// 1. 기준 좌표
	/*TSet<FIntVector> RequiredChunks = GetRequiredChunks(ReferenceLocation, RenderDistance);
	TSet<FIntVector> FillChunks = GetRequiredChunks(ReferenceLocation, FillDistance);*/

	TArray<ATerrainChunk*> ChunksToGenerateMesh;
	TArray<ATerrainChunk*> ChunksToUpdateMesh;

	LoadAndUnloadChunks(NoiseGenerator, ChunksToGenerateMesh, ChunksToUpdateMesh,ReferenceLocation);
	ChunksToGenerateMesh.Append(ChunksToUpdateMesh);

	const uint32 NumChunksToGenerateMesh = ChunksToGenerateMesh.Num();
	/*if (NumChunksToGenerateMesh == 0)
	{
	    //PROFILE_END("UpdateChunksTask")
	    return;
	}*/

	// TTask 기반 비동기 메시 준비
	TArray<TUniquePtr<UE::Tasks::TTask<void>>> Tasks;
	Tasks.Reserve(NumChunksToGenerateMesh);

	for (ATerrainChunk* Chunk : ChunksToGenerateMesh)
	{
		// Chunk->SetActorRelativeScale3D(FVector(VoxelSize,VoxelSize,VoxelSize));
		UE::Tasks::TTask<ATerrainChunk*> Task =
			UE::Tasks::Launch(UE_SOURCE_LOCATION,
			                  [this, Chunk]
			                  {
				                  if (FChunkData* ChunkData = GetChunkData(Chunk->ChunkCoordinate))
				                  {
					                  Chunk->PrepareMesh(0, ChunkData);
					                  return Chunk;
				                  }
				                  return static_cast<ATerrainChunk*>(nullptr);
			                  });
		PrepareMeshTasks.Enqueue(MoveTemp(Task));
	}
	//PROFILE_END("UpdateChunksTask-GetRequiredChunks")
	//PROFILE_END("UpdateChunksTask")
}
#pragma endregion
void UChunkManager::UpdateChunksScheduled(const FVector& ReferenceLocation, UNoiseGenerator* NoiseGenerator)
{
	/*TSet<FIntVector> RequiredChunks = GetRequiredChunks(ReferenceLocation, RenderDistance);
	TSet<FIntVector> FillChunks = GetRequiredChunks(ReferenceLocation, FillDistance);*/

	TArray<ATerrainChunk*> ChunksToGenerateMesh;
	TArray<ATerrainChunk*> ChunksToUpdateMesh;

	LoadAndUnloadChunks(NoiseGenerator, ChunksToGenerateMesh, ChunksToUpdateMesh,ReferenceLocation);
	ChunkScheduler->PrepareExistingChunks(ChunksToUpdateMesh, ReferenceLocation);
	ChunkScheduler->PrepareNewChunks(ChunksToGenerateMesh, ReferenceLocation);

	//앞으로 이동
	//1프레임 지연되는 대신 Wait하는 병목이 사라질듯
	ChunkScheduler->ProcessGenQueue();
}

TSet<FIntVector> UChunkManager::GetRequiredChunks(const FVector& ReferenceLocation, const int32 Distance) const
{
	// 1. 현재 기준 좌표
	FIntVector CenterChunk = WorldToChunkCoord(ReferenceLocation);

	/*if (!SortByDistance)
	{*/
	// 2. 필요 청크 집합 계산
	TSet<FIntVector> RequiredChunks;

#ifdef DCG_IGNORE_Z_DISTANCE
	for (int32 x = -Distance; x <= Distance; ++x)
		for (int32 y = -Distance; y <= Distance; ++y)
			for (int32 z = -1; z <= 1; ++z)
			{
				RequiredChunks.Add(CenterChunk + FIntVector(x, y, z));
			}
#else
	for (int32 x = -Distance; x <= Distance; ++x)
		for (int32 y = -Distance; y <= Distance; ++y)
			for (int32 z = -Distance; z <= Distance; ++z)
			{
				RequiredChunks.Add(CenterChunk + FIntVector(x, y, z));
			}
#endif


	return RequiredChunks;
}


void UChunkManager::LoadAndUnloadChunks(const UNoiseGenerator* Generator,
                                        TArray<ATerrainChunk*>& ChunksToGenerateMesh,
                                        TArray<ATerrainChunk*>& ChunksToUpdateMesh,
                                        const FVector& ReferenceLocation)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UChunkManager::LoadAndUnloadChunks);

	const FIntVector CenterChunk = WorldToChunkCoord(ReferenceLocation);
	const int32 MaxDistance = FMath::Max(RenderDistance, FillDistance);

	//----------------------------------------
	// 1. Unload (기존 방식 유지)
	//----------------------------------------
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(LoadAndUnloadChunks_Unload);
		TArray<FIntVector> ActiveChunkCoordArray;
		ActiveChunks.GetKeys(ActiveChunkCoordArray);

		OwnerManager->TimeBudgetState.BeginSection(ETimeBudgetSection::ChunkManager_UnloadChunks);
		for (const FIntVector& Coord : ActiveChunkCoordArray)
		{
			if (OwnerManager->TimeBudgetState.ShouldContinue(ETimeBudgetSection::ChunkManager_UnloadChunks)){
				const int32 Dist = FMath::Max3(FMath::Abs(Coord.X - CenterChunk.X),
				                               FMath::Abs(Coord.Y - CenterChunk.Y),
				                               FMath::Abs(Coord.Z - CenterChunk.Z));
				if (Dist > RenderDistance)
				{
					UnloadChunk(Coord);
				}
			}else
			{
				break;
			}
		}
		OwnerManager->TimeBudgetState.EndSection(ETimeBudgetSection::ChunkManager_UnloadChunks);
	}

	//----------------------------------------
	// 2. 중심부터 거리 기반 순회 → Load / Fill / Update 분기
	//----------------------------------------
	TArray<FIntVector> Shell;
	OwnerManager->TimeBudgetState.BeginSection(ETimeBudgetSection::ChunkManager_LoadChunks);
	OwnerManager->TimeBudgetState.PauseSection(ETimeBudgetSection::ChunkManager_LoadChunks);
	for (int32 d = 0; d <= MaxDistance; ++d)
	{
		FChunkUtils::GenerateCubeShellOffsets(d, Shell); // 중심부터 거리순으로 Offsets 생성

		for (const FIntVector& Offset : Shell)
		{
			FIntVector Coord = CenterChunk + Offset;

			const bool bInLoadRange = d <= RenderDistance;
			const bool bInFillRange = d <= FillDistance;

			const bool bActive = ActiveChunks.Contains(Coord);
			const bool bHasData = ChunkDataMap.Contains(Coord);

			if (bInLoadRange)
			{
				if (!bActive)
				{
					if (bHasData)
					{
						// 시간 제한 체크
						if (OwnerManager->TimeBudgetState.ShouldContinue(ETimeBudgetSection::ChunkManager_LoadChunks))
						{
							OwnerManager->TimeBudgetState.ResumeSection(ETimeBudgetSection::ChunkManager_LoadChunks);

							if (ATerrainChunk* NewChunk = LoadChunk(Coord))
							{
								NewChunk->ChunkCoordinate = Coord;
								ActiveChunks.Add(Coord, NewChunk);

								const FVector WorldLocation = ChunkToWorldLocation(Coord);
								NewChunk->InitializeChunk(Coord, &MarchingCubes, OwnerManager, WorldLocation, FVector(VoxelSize));
								//NewChunk->CurrentLOD = 0;
								NewChunk->SetActive(true);
								NewChunk->SetChunkState(ETerrainChunkState::NeedsPrepare);
								ChunksToGenerateMesh.Add(NewChunk);
							}
							OwnerManager->TimeBudgetState.PauseSection(ETimeBudgetSection::ChunkManager_LoadChunks);
						}
					}
					else if (bInFillRange && !FillingChunks.Contains(Coord))
					{
						ChunksToFill.Add(Coord);
					}
				}
				else
				{
					ATerrainChunk* Chunk = ActiveChunks[Coord];
					if (Chunk->GetChunkState() == ETerrainChunkState::Modified)
					{
						Chunk->SetChunkState(ETerrainChunkState::NeedsPrepare);
						ChunksToUpdateMesh.Add(Chunk);
					}
				}
			}
			else if (bInFillRange && !bHasData && !FillingChunks.Contains(Coord))
			{
				ChunksToFill.Add(Coord);
			}
		}
	}
	OwnerManager->TimeBudgetState.EndSection(ETimeBudgetSection::ChunkManager_LoadChunks);

	//----------------------------------------
	// 3. Fill 실행
	//----------------------------------------
	FillChunks(Generator);
}



void UChunkManager::FillChunks( const UNoiseGenerator* Generator)
{
	//이 부분은 별도 함수로 빼면 좋을듯
	//다만 전부 태스크로 던지다보니 스케줄러로 옮길 이유는 없을듯
	switch (ChunkLoadMode)
	{
	case EChunkLoadMode::BlockingSerial:
		FillChunkData_SingleThreaded(/*ChunksToFill, */Generator);
		break;
	default:
		if (!ChunksToFill.IsEmpty())
		{
			LaunchChunkDataTasks(/*ChunksToFill, */Generator);
		}
		ProcessPendingChunkFillTasks();
		break;
	}
	ChunksToFill.Empty();
	//FillChunkData_SingleThreaded(ChunksToFill,Generator,ChunksToUpdateMesh);
}

FString UChunkManager::GetChunkSaveFilePath(const FString& MapName)
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Chunks"), MapName + TEXT("_WorldChunks.bin"));
}

void UChunkManager::SaveChunksToDisk(const FString& MapName)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UChunkManager::SaveModifiedChunksToDisk);

	TArray<FIntVector> ChunksToSaveCopy;
	{
		FScopeLock Lock(&ChunkCoordsToSaveLock);
		ChunksToSaveCopy = ChunkCoordsToSave.Array();
		ChunkCoordsToSave.Reset();
	}

	TArray<uint8> TotalBytes;
	FMemoryWriter Writer(TotalBytes, true);

	const uint32 Version = 1;
	Writer << const_cast<uint32&>(Version);

	int32 ChunkCount = ChunksToSaveCopy.Num();
	Writer << ChunkCount;

	for (FIntVector& Coord : ChunksToSaveCopy)
	{
		if (FChunkData** ChunkDataPtr = ChunkDataMap.Find(Coord))
		{
			TArray<uint8> Bytes;
			(*ChunkDataPtr)->SerializeToBytes(Bytes);

			Writer << Coord.X;
			Writer << Coord.Y;
			Writer << Coord.Z;
			int32 DataSize = Bytes.Num();
			Writer << DataSize;
			Writer.Serialize(Bytes.GetData(), DataSize);
		}
	}

	const FString SavePath = GetChunkSaveFilePath(MapName);
	FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Chunks"));
	IFileManager::Get().MakeDirectory(*SaveDir, true);
	if (FFileHelper::SaveArrayToFile(TotalBytes, *SavePath))
	{
		UE_LOG(LogDCG, Log, TEXT("Saved %d chunks to %s"), ChunkCount, *SavePath);
	}
	else
	{
		UE_LOG(LogDCG, Warning, TEXT("Failed to save chunks to %s"), *SavePath);
	}
}

void UChunkManager::LoadChunksFromDisk(const FString& MapName)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UChunkManager::LoadChunksFromDisk);
	const FString LoadPath = GetChunkSaveFilePath(MapName);
	TArray<uint8> TotalBytes;
	if (!FFileHelper::LoadFileToArray(TotalBytes, *LoadPath))
	{
		UE_LOG(LogDCG, Warning, TEXT("No saved chunk file found at %s"), *LoadPath);
		return;
	}

	FMemoryReader Reader(TotalBytes, true);

	uint32 Version;
	Reader << Version;

	int32 ChunkCount = 0;
	Reader << ChunkCount;

	for (int32 i = 0; i < ChunkCount; ++i)
	{
		FIntVector Coord;
		int32 DataSize = 0;

		Reader << Coord.X;
		Reader << Coord.Y;
		Reader << Coord.Z;
		Reader << DataSize;

		TArray<uint8> ChunkBytes;
		ChunkBytes.SetNumUninitialized(DataSize);
		Reader.Serialize(ChunkBytes.GetData(), DataSize);

		FChunkData* NewData = new FChunkData();
		NewData->DeserializeFromBytes(ChunkBytes);

		ChunkDataMap.Add(Coord, NewData);
		FScopeLock Lock(&ChunkCoordsToSaveLock);
		ChunkCoordsToSave.Add(Coord);
	}
	UE_LOG(LogDCG, Log, TEXT("Loaded %d chunks from disk."), ChunkCount);
}

void UChunkManager::LaunchChunkDataTasks(
	/*const TSet<FIntVector>& ChunksToFill,*/
	const UNoiseGenerator* Generator)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UChunkManager::LaunchChunkDataTasks)

	using FChunkTask = UE::Tasks::TTask<FChunkFillResult>;

	// 1. 비동기 Task 생성
	for (const FIntVector& Coord : ChunksToFill)
	{
		if (FillingChunks.Contains(Coord) || ChunkDataMap.Contains(Coord))
		{
			continue;
		}
		//ChunkActor->SetChunkState(ETerrainChunkState::Filling);
		FChunkTask Task = UE::Tasks::Launch(UE_SOURCE_LOCATION, [Generator,Coord]()-> FChunkFillResult
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FillNoise_Task);
			if (!Generator)return {};
			FChunkData* NewChunkData = new FChunkData();
			Generator->FillChunkData(NewChunkData, Coord);
			return FChunkFillResult(NewChunkData, Coord);
		}, LowLevelTasks::ETaskPriority::BackgroundLow);

		PendingChunkFillTasks.Add(MoveTemp(Task));
		FillingChunks.Add(Coord);
	}
}

void UChunkManager::ProcessPendingChunkFillTasks()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UChunkManager::ProcessPendingChunkFillTasks)
	if (PendingChunkFillTasks.IsEmpty()) return;

	//TArray<FChunkFillResult> CompletedResults;
	for (int32 i = PendingChunkFillTasks.Num() - 1; i >= 0; --i)
	{
		auto& Task = PendingChunkFillTasks[i];
		if (Task.IsCompleted())
		{
			FChunkFillResult Result = Task.GetResult();
			ChunkDataMap.Add(Result.ChunkCoord, Result.GeneratedData);
			//CompletedResults.Add(Task.GetResult());
			PendingChunkFillTasks.RemoveAt(i);
			FillingChunks.Remove(Result.ChunkCoord);
		}
	}
}

void UChunkManager::FillChunkData_SingleThreaded(
	/*const TSet<FIntVector>& ChunksToFill,*/
	const UNoiseGenerator* Generator
	/*TArray<ATerrainChunk*>& OutChunksToGenerateMesh*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UChunkManager::GenerateChunkData_SingleThreaded);

	for (const FIntVector& ChunkCoord : ChunksToFill)
	{
		if (!Generator) continue;

		// 노이즈 데이터 생성
		FChunkData* NewChunkData = new FChunkData();
		Generator->FillChunkData(NewChunkData, ChunkCoord);

		// 데이터 맵 등록
		ChunkDataMap.Add(ChunkCoord, NewChunkData);
	}
}

void UChunkManager::Release()
{
	UE_LOG(LogDCG, Log, TEXT("UChunkManager::Release() - Starting full cleanup process..."));
	ShutdownAsyncTasks();
	if (IsValid(ChunkScheduler))
	{
		ChunkScheduler->Release();
		ChunkScheduler = nullptr;
	}
	ReleaseChunkDataMap();
}

/**
 * 실행 중인 모든 비동기 Task가 완료될 때까지 기다리고,
 * Task가 할당한 동적 메모리를 해제합니다.
 */
void UChunkManager::ShutdownAsyncTasks()
{
	UE_LOG(LogDCG, Log, TEXT("Shutting down asynchronous tasks..."));
	
	// 'Chunk Data Fill' Task 정리
	if (PendingChunkFillTasks.Num() > 0)
	{
		UE_LOG(LogDCG, Log, TEXT("Waiting for %d pending chunk data fill tasks..."), PendingChunkFillTasks.Num());
		for (UE::Tasks::TTask<FChunkFillResult>& Task : PendingChunkFillTasks)
		{
			if (Task.IsValid() && !Task.IsCompleted())
			{
				Task.Wait(); // Task가 완료될 때까지 대기합니다.
			}
            
			// Task 완료 후, Task가 할당한 메모리를 반드시 해제합니다.
			if (Task.IsValid())
			{
				FChunkFillResult Result = Task.GetResult();
				if (Result.GeneratedData)
				{
					delete Result.GeneratedData;
					Result.GeneratedData = nullptr;
				}
			}
		}
	}
	PendingChunkFillTasks.Empty();
	FillingChunks.Empty();

	// 'Prepare Mesh' Task 정리
	if (!PrepareMeshTasks.IsEmpty())
	{
		UE_LOG(LogDCG, Log, TEXT("Waiting for pending mesh preparation tasks..."));
		UE::Tasks::TTask<ATerrainChunk*> PrepareTask;
		while (PrepareMeshTasks.Dequeue(PrepareTask))
		{
			if (PrepareTask.IsValid() && !PrepareTask.IsCompleted())
			{
				PrepareTask.Wait(); // Task 완료 대기
			}
			// 결과물인 ATerrainChunk*는 UObject이므로 GC가 관리합니다. delete하지 않습니다.
		}
	}
}

void UChunkManager::ReleaseChunkDataMap()
{
	ChunkDataMap.ForEach([](const FIntVector& Key, FChunkData* Data)
	{
		if (Data)
		{
			delete Data; // 또는 Data->ConditionalBeginDestroy() 등
		}
	});
	ChunkDataMap.Empty();
}


void UChunkManager::DrawDebugBounds(UWorld* World, FColor Color)
{
	if (!World) return;

	for (const TPair<FIntVector, ATerrainChunk*>& Pair : ActiveChunks.GetData())
	{
		const FIntVector& ChunkCoord = Pair.Key;

		// 원하는 작업 수행
		if (ATerrainChunk* Chunk = Pair.Value)
		{
			const FVector Origin = FVector(ChunkCoord) * CHUNK_SIZE * VoxelSize;
			const FVector BoxExtent = FVector(CHUNK_SIZE * VoxelSize * 0.5f);

			DrawDebugBox(
				World,
				Origin + BoxExtent, // 중심 위치
				BoxExtent, // 반지름
				Color
			);
		}
	}
}

void UChunkManager::DrawDebugPoints(UWorld* World, int Stride, mat_type MatType, float IsoLevel)
{
	if (!World) return;

	float StrideSqrt = FMath::Sqrt(static_cast<float>(Stride));

	for (const TPair<FIntVector, ATerrainChunk*>& Pair : ActiveChunks.GetData())
	{
		const FIntVector& ChunkCoord = Pair.Key;

		if (ATerrainChunk* Chunk = Pair.Value)
		{
			const FVector Origin = FVector(ChunkCoord) * CHUNK_SIZE * VoxelSize;
			FChunkData* Data = ChunkDataMap[ChunkCoord];
			if (Data == nullptr)
			{
				continue;
			}
			if (MatType >= Data->DensityMap.Num())
			{
				return;
			}
			auto Densities = Data->DensityMap[MatType];

			for (auto DensityPair : Densities)
			{
				FIntVector LocalCoord = DensityPair.Key;
				if (LocalCoord.X % Stride == 0 && LocalCoord.Y % Stride == 0 && LocalCoord.Z % Stride == 0)
				{
					FIntVector Offset = LocalCoord * VoxelSize;
					float* Valptr = DensityPair.Value;
					float Val = *Valptr;

					FLinearColor Col;
					if (Val <= IsoLevel)
					{
						// Blue (-) → Green (IsoValue)
						float Alpha = FMath::Clamp((Val - CHUNK_DENSITY_MIN) / (IsoLevel - CHUNK_DENSITY_MIN), 0.f,
						                           1.f);
						Col = FLinearColor::LerpUsingHSV(FLinearColor::Blue, FLinearColor::Green, Alpha);
					}
					else
					{
						// Green (IsoValue) → Red (+)
						float Alpha = FMath::Clamp((Val - IsoLevel) / (CHUNK_DENSITY_MAX - IsoLevel), 0.f, 1.f);
						Col = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, Alpha);
					}

					DrawDebugPoint(
						World,
						Origin + FVector(Offset),
						4.f * StrideSqrt,
						Col.ToFColor(true)
					);
				}
			}
		}
	}
}

void UChunkManager::DrawDebugGradients(UWorld* World, int Stride)
{
	if (!World) return;

	for (const TPair<FIntVector, ATerrainChunk*>& Pair : ActiveChunks.GetData())
	{
		const FIntVector& ChunkCoord = Pair.Key;

		if (ATerrainChunk* Chunk = Pair.Value)
		{
			const FVector Origin = FVector(ChunkCoord) * CHUNK_SIZE * VoxelSize;
			FChunkData* Data = ChunkDataMap[ChunkCoord];
			if (Data == nullptr)
			{
				continue;
			}
			auto Densities = Data->DensityMap[0];

			for (auto DensityPair : Densities)
			{
				FIntVector LocalCoord = DensityPair.Key;
				if (LocalCoord.X % Stride == 0 && LocalCoord.Y % Stride == 0 && LocalCoord.Z % Stride == 0)
				{
					FIntVector Offset = LocalCoord * VoxelSize;
					FVector Start = Origin + FVector(Offset);
					float Length = VoxelSize * Stride / 4.f;
					auto Gradient = Densities.GetGradient(LocalCoord);
					FVector End = Start + Length * FVector(Gradient);

					// HSV 색상 매핑: 방향(Gradient)의 각도 → 색상(Hue)
					// 2D라면 atan2(Gradient.Y, Gradient.X), 3D라면 atan2(Gradient.Y, Gradient.X)나, Yaw 사용
					float Yaw = Gradient.Rotation().Yaw; // -180~180
					float Hue = FMath::Fmod(Yaw + 360.0f, 360.0f); // 0~360 변환

					FLinearColor HSVColor = FLinearColor::MakeFromHSV8((uint8)(Hue / 360.0f * 255.0f), 255, 255);
					FColor LineColor = HSVColor.ToFColor(true);

					DrawDebugDirectionalArrow(
						World,
						Start,
						End,
						5.f * Stride,
						LineColor
					);
				}
			}
		}
	}
}
