// Copyright 2025 J2K2. All Rights Reserved.

#include "Core/Scheduler/ChunkScheduler.h"

//#include "ProfileTimerSubsystem.h"
#include "Chunk/TerrainChunk.h"
#include "Core/ChunkManager.h"
#include "Core/TerrainManager.h"
#include "Core/Scheduler/TimeBudgetSystem.h"
#include "Kismet/GameplayStatics.h"
#include "SceneManagement.h"

FChunkTask::FChunkTask(ATerrainChunk* InChunk, float InPriority, double InCost)
	: Chunk(InChunk), Priority(InPriority), EstimatedCost(InCost)
{
	ChunkCoord = InChunk->ChunkCoordinate;
}

bool FChunkTask::MatchesChunk() const
{
	//return true;
	if (!Chunk)return false;
	return Chunk->ChunkCoordinate == ChunkCoord;
}

void UChunkScheduler::SetTerrainManger(ATerrainManager* InManager)
{
	TerrainManager = InManager;
	ChunkManager = InManager->ChunkManager;
}

void UChunkScheduler::PrepareNewChunks(TArray<ATerrainChunk*>& ChunksToGenerateMesh, const FVector& PlayerLocation)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UChunkScheduler::PrepareNewChunks);
	//PROFILE_BEGIN("PrepareNewChunks")
	// 0. 완료된 Task 처리
	TArray<UE::Tasks::TTask<FChunkTask>> CompletedTasks;
	TArray<UE::Tasks::TTask<FChunkTask>> TempQueue;
	UE::Tasks::TTask<FChunkTask> TaskDone;
	while (NewMeshTasks.Dequeue(TaskDone))
	{
		//if (!TaskDone.IsValid())continue;
		if (TaskDone.IsCompleted())
		{
			CompletedTasks.Add(MoveTemp(TaskDone));
		}
		else
		{
			TempQueue.Add(MoveTemp(TaskDone));
		}
	}
	// 다시 미완성된 태스크들을 큐에 삽입
	for (auto& Task : TempQueue)
			NewMeshTasks.Enqueue(MoveTemp(Task));
	{
		// 1.확실히 Completed된 상태 Gen큐에 삽입
		for (UE::Tasks::TTask<FChunkTask>& Task : CompletedTasks)
		{
			FChunkTask Result = Task.GetResult();
			if (Result.MatchesChunk())
			{
				Result.EstimatedCost = EstimateCost(Result.Chunk);
				GenQueue_Background.Enqueue(MoveTemp(Result));
			}
			//if (Task.IsValid()) GenQueue_Background.Enqueue(MoveTemp(Task.GetResult()));
		}
	}
	//2. Prepare 태스크로 처리
	for (ATerrainChunk* Chunk : ChunksToGenerateMesh)
	{
		//2차 확인. 아마 필요는 없을 것
		if (!Chunk || Chunk->GetChunkState() != ETerrainChunkState::NeedsPrepare)continue;
		const float Priority = EvaluatePriority(Chunk);
		const double Cost = EstimateCost(Chunk);
		FChunkTask ChunkTask(Chunk, Priority, Cost);
		UE::Tasks::TTask<FChunkTask> Task =
			UE::Tasks::Launch(UE_SOURCE_LOCATION,
			                  [ChunkManager = this->ChunkManager, ChunkTask
			                  ]() -> FChunkTask
			                  {
				                  TRACE_CPUPROFILER_EVENT_SCOPE(PrepareNewChunksTask);
				                  if (!ChunkTask.MatchesChunk())return FChunkTask();
				                  if (FChunkData* ChunkData = ChunkManager->
					                  GetChunkData(ChunkTask.ChunkCoord))
				                  {
					                  ChunkTask.Chunk->PrepareMesh(0, ChunkData);
					                  return ChunkTask;
				                  }
				                  return FChunkTask();
				                  //return static_cast<ATerrainChunk*>(nullptr);
			                  }/*,LowLevelTasks::ETaskPriority::BackgroundHigh*/);
		NewMeshTasks.Enqueue(MoveTemp(Task));
	}

	//PROFILE_END("PrepareNewChunks")
}

//#define PrepareParellel
void UChunkScheduler::PrepareExistingChunks(TArray<ATerrainChunk*>& ChunksToUpdateMesh, const FVector& PlayerLocation)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UChunkScheduler::PrepareExistingChunks);
	if (/*ChunksToUpdateMesh.IsEmpty() || */!ChunkManager)
		return;
	//PROFILE_BEGIN("PrepareExistingChunks")
	ChunksToUpdateMesh.Sort([this](const ATerrainChunk& A, const ATerrainChunk& B)
	{
		return EvaluatePriority(&A) < EvaluatePriority(&B);
	});
	TArray<ATerrainChunk*> CompletedChunks;
	for (auto It = UpdateMeshTaskMap.CreateIterator(); It; ++It)
	{
		UE::Tasks::TTask<FChunkTask>& Task = It.Value();
		//TODO
		//테스트 용이니까 지우기
		//if (!Task.IsCompleted())Task.Wait();
		if (Task.IsCompleted())
		{
			FChunkTask Result = Task.GetResult();
			if (Result.MatchesChunk() && !IsChunkDestroyed(Result.Chunk))
			{
				Result.EstimatedCost = EstimateCost(Result.Chunk);
				if (NeedsPhysicsInteraction(Result.Chunk/*, PlayerLocation*/))
				{
					GenQueue_Physics.Enqueue(Result);
				}
				else
				{
					GenQueue_Render.Enqueue(Result);
				}
			}
			CompletedChunks.Add(It.Key());
		}
	}
	// Remove completed tasks
	for (ATerrainChunk* Chunk : CompletedChunks)
	{
		UpdateMeshTaskMap.Remove(Chunk);
	}

	// Launch new tasks
	for (ATerrainChunk* Chunk : ChunksToUpdateMesh)
	{
		if (!Chunk || IsChunkDestroyed(Chunk)) continue;
		//if (UpdateMeshTaskMap.Contains(Chunk)) continue; // already running

		const float Priority = EvaluatePriority(Chunk);
		const double Cost = EstimateCost(Chunk);
		FChunkTask ChunkTask(Chunk, Priority, Cost);
		UE::Tasks::TTask<FChunkTask> Task =
			UE::Tasks::Launch(UE_SOURCE_LOCATION,
			                  [ChunkManager = this->ChunkManager, ChunkTask
			                  ]() -> FChunkTask
			                  {
				                  TRACE_CPUPROFILER_EVENT_SCOPE(
					                  PrepareExistingChunksTask);
				                  if (!ChunkTask.MatchesChunk())return FChunkTask();
				                  if (FChunkData* ChunkData = ChunkManager->
					                  GetChunkData(ChunkTask.ChunkCoord))
				                  {
					                  ChunkTask.Chunk->PrepareMesh(0, ChunkData);
					                  return ChunkTask;
				                  }
				                  return FChunkTask();
			                  });
		UpdateMeshTaskMap.Add(Chunk, MoveTemp(Task));
	}
	/*//PROFILE_BEGIN("GenQueue_Physics")
	FChunkTask PhysicsTask;
	int32 Count = 0;
	while (GenQueue_Physics.Dequeue(PhysicsTask))
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GenQueue_Physics);
		if (PhysicsTask.MatchesChunk() && !IsChunkDestroyed(PhysicsTask.Chunk))
		{
			//TODO: 
			//중복 체크 넣어야 함
			//이미 Upload 내에서 State 체크는 하고 있어서 중복 업로드는 안될 것
			if (UE::Tasks::TTask<FChunkTask>* Pending = UpdateMeshTaskMap.Find(PhysicsTask.Chunk))
			{
				if (!Pending->IsCompleted())
				{
					Pending->Wait();
				}
			}
			//TODO
			//피직스 관련을 Task 보내고 나중에 Wait는 하도록
			PhysicsTask.Chunk->ProceduralMesh->bUseAsyncCooking=false;
			PhysicsTask.Chunk->UploadMesh();
			PhysicsTask.Chunk->ProceduralMesh->bUseAsyncCooking=true;
			++Count;
		}
	}
	//PROFILE_END("GenQueue_Physics")*/
	//PROFILE_END("PrepareExistingChunks")
}


void UChunkScheduler::ProcessGenQueue()
{
	//추후 정렬은 여기서 할 수도
	ProcessGenQueue_Physics();
	ProcessGenQueue_Render();
	ProcessGenQueue_Background();
}


void UChunkScheduler::ProcessGenQueue_Physics()
{
	//PROFILE_BEGIN("GenQueue_Physics")
	FChunkTask PhysicsTask;
	int32 Count = 0;
	while (GenQueue_Physics.Dequeue(PhysicsTask))
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GenQueue_Physics);
		if (PhysicsTask.MatchesChunk() && !IsChunkDestroyed(PhysicsTask.Chunk))
		{
			//TODO: 
			//중복 체크 넣어야 함
			//이미 Upload 내에서 State 체크는 하고 있어서 중복 업로드는 안될 것
			if (UE::Tasks::TTask<FChunkTask>* Pending = UpdateMeshTaskMap.Find(PhysicsTask.Chunk))
			{
				if (!Pending->IsCompleted())
				{
					Pending->Wait();
				}
			}
			//TODO
			//피직스 관련을 Task 보내고 나중에 Wait는 하도록
			//PhysicsTask.Chunk->ProceduralMesh->bUseAsyncCooking=false;
			PhysicsTask.Chunk->UploadMesh();
			//PhysicsTask.Chunk->ProceduralMesh->bUseAsyncCooking=true;
			++Count;
		}
	}
	//PROFILE_END("GenQueue_Physics")
}

void UChunkScheduler::ProcessGenQueue_Render()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UChunkScheduler::ProcessGenQueue_Render);
	//FScopeLock Lock(&RenderQueueLock);
	//PROFILE_BEGIN("ProcessGenQueue_Render")

	TMap<ATerrainChunk*, FChunkTask> FrustumMap;
	TMap<ATerrainChunk*, FChunkTask> NonFrustumMap;
	{
		FChunkTask Task;
		while (GenQueue_Render.Dequeue(Task))
		{
			if (!Task.MatchesChunk() || IsChunkDestroyed(Task.Chunk))continue;
			if (EstimateCost(Task.Chunk) == 0)
			{
				Task.Chunk->ClearMesh();
			}else if (NeedsPhysicsInteraction(Task.Chunk))
			{
				continue;
				/*
				Task.Chunk->ProceduralMesh->bUseAsyncCooking=false;
				Task.Chunk->UploadMesh();
				Task.Chunk->ProceduralMesh->bUseAsyncCooking=true;*/
			}
			else if (IsInViewFrustum(Task.Chunk))
			{
				FrustumMap.Add(Task.Chunk, Task);
			}
			else
			{
				NonFrustumMap.Add(Task.Chunk, Task);
			}
		}
	}
	auto SortByDistance = [this](const FChunkTask& A, const FChunkTask& B)
	{
		return GetDistanceFromCamera(A.Chunk) < GetDistanceFromCamera(B.Chunk);
	};

	TArray<FChunkTask> SortedFrustumChunks;
	FrustumMap.GenerateValueArray(SortedFrustumChunks);
	SortedFrustumChunks.Sort(SortByDistance);

	TArray<FChunkTask> SortedNonFrustumChunks;
	NonFrustumMap.GenerateValueArray(SortedNonFrustumChunks);
	SortedNonFrustumChunks.Sort(SortByDistance);


	TArray<FChunkTask> RemainingFrustumChunks;
	TArray<FChunkTask> RemainingNonFrustumChunks;
	TerrainManager->TimeBudgetState.BeginSection(ETimeBudgetSection::Scheduler_RenderFrustum);
	for (FChunkTask& Task : SortedFrustumChunks)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Scheduler_RenderFrustum)
		if (!TerrainManager->TimeBudgetState.ShouldContinue(ETimeBudgetSection::Scheduler_RenderFrustum))
		{
			if (Task.MatchesChunk()) RemainingFrustumChunks.Add(Task);
			continue;
		}
		if (Task.MatchesChunk() && !IsChunkDestroyed(Task.Chunk))
		{
			if (UE::Tasks::TTask<FChunkTask>* Pending = UpdateMeshTaskMap.Find(Task.Chunk))
			{
				if (!Pending->IsCompleted())
				{
					Pending->Wait();
				}
			}
			if (Task.MatchesChunk())Task.Chunk->UploadMesh(/*true*/);
		}
	}
	TerrainManager->TimeBudgetState.EndSection(ETimeBudgetSection::Scheduler_RenderFrustum);

	TerrainManager->TimeBudgetState.BeginSection(ETimeBudgetSection::Scheduler_RenderNonFrustum);
	for (FChunkTask& Task : SortedNonFrustumChunks)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Scheduler_RenderNonFrustum)
		if (!TerrainManager->TimeBudgetState.ShouldContinue(ETimeBudgetSection::Scheduler_RenderNonFrustum))
		{
			if (Task.MatchesChunk()) RemainingNonFrustumChunks.Add(Task);
			continue;
		}
		if (Task.Chunk && !IsChunkDestroyed(Task.Chunk))
		{
			if (UE::Tasks::TTask<FChunkTask>* Pending = UpdateMeshTaskMap.Find(Task.Chunk))
			{
				if (!Pending->IsCompleted())
				{
					Pending->Wait();
				}
			}
			if (Task.MatchesChunk())Task.Chunk->UploadMesh();
		}
	}
	TerrainManager->TimeBudgetState.EndSection(ETimeBudgetSection::Scheduler_RenderNonFrustum);

	for (const FChunkTask& Task : RemainingFrustumChunks)
	{
		if (Task.MatchesChunk())GenQueue_Render.Enqueue(Task);
	}
	for (const FChunkTask& Task : RemainingNonFrustumChunks)
	{
		if (Task.MatchesChunk())GenQueue_Render.Enqueue(Task);
	}
	//PROFILE_END("ProcessGenQueue_Render")
}


void UChunkScheduler::ProcessGenQueue_Background()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UChunkScheduler::ProcessGenQueue_Background);

	TArray<FChunkTask> AllTasks;
	{
		FChunkTask Task;
		while (GenQueue_Background.Dequeue(Task))
		{
			if (Task.MatchesChunk())
				AllTasks.Add(Task);
		}
	}

	//------------------------------------------------------
	// 1. High Priority 먼저 수행 (우선순위 높은 순)
	//------------------------------------------------------
	AllTasks.Sort([](const FChunkTask& A, const FChunkTask& B)
	{
		return A.Priority < B.Priority;
	});

	TerrainManager->TimeBudgetState.BeginSection(ETimeBudgetSection::Scheduler_BackgroundHigh);
	TArray<FChunkTask> RemainingTasks;
	for (FChunkTask& Task : AllTasks)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Scheduler_BackgroundHigh);
		if (!Task.MatchesChunk())
			continue;

		if (!TerrainManager->TimeBudgetState.ShouldContinue(ETimeBudgetSection::Scheduler_BackgroundHigh))
		{
			RemainingTasks.Add(MoveTemp(Task));
			continue;
		}

		if (!IsChunkDestroyed(Task.Chunk))
		{
			if (Task.EstimatedCost == 0) Task.Chunk->ClearMesh();
			else Task.Chunk->UploadMesh();
		}
	}
	TerrainManager->TimeBudgetState.EndSection(ETimeBudgetSection::Scheduler_BackgroundHigh);

	//------------------------------------------------------
	// 2. 남은 Task를 Low Priority로 수행 (비용 적은 순)
	//------------------------------------------------------
	RemainingTasks.Sort([](const FChunkTask& A, const FChunkTask& B)
	{
		return A.EstimatedCost < B.EstimatedCost;
	});

	TerrainManager->TimeBudgetState.BeginSection(ETimeBudgetSection::Scheduler_BackgroundLow);
	TArray<FChunkTask> DeferredTasks;
	for (FChunkTask& Task : RemainingTasks)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Scheduler_BackgroundLow);
		if (!Task.MatchesChunk())
			continue;

		if (!TerrainManager->TimeBudgetState.ShouldContinue(ETimeBudgetSection::Scheduler_BackgroundLow))
		{
			DeferredTasks.Add(MoveTemp(Task));
			continue;
		}

		if (!IsChunkDestroyed(Task.Chunk))
		{
			if (Task.EstimatedCost == 0) Task.Chunk->ClearMesh();
			else Task.Chunk->UploadMesh();
		}
	}
	TerrainManager->TimeBudgetState.EndSection(ETimeBudgetSection::Scheduler_BackgroundLow);

	//------------------------------------------------------
	// 3. 다음 프레임으로 이월
	//------------------------------------------------------
	for (FChunkTask& Task : DeferredTasks)
	{
		if (Task.MatchesChunk())
			GenQueue_Background.Enqueue(MoveTemp(Task));
	}
}


/*const float VIEW_ANGLE_THRESHOLD = 0.5f; // 약 60도
// 시야 밖이라도 렌더링을 준비해야 하는 근접 거리
const float CLOSE_DISTANCE_THRESHOLD = 1500.f; // VoxelSize 100 기준 약 1.5 청크 거리*/
float UChunkScheduler::EvaluatePriority(const ATerrainChunk* Chunk) const
{
	if (!Chunk) return FLT_MAX;
	return GetDistanceFromCamera(Chunk);
	/*
	const FVector ChunkLocation = Chunk->GetActorLocation();
	const FVector ToChunk = ChunkLocation - CachedCameraLocation;
	*/

	// 1. 거리 점수 (기본 점수)
	/*const float DistanceScore = ToChunk.Size();
	return DistanceScore;*/
	/*// 2. 시야각 보너스 (정면일수록 큰 값을 빼서 우선순위를 높임)
	const float Dot = FVector::DotProduct(CachedCameraForward, ToChunk.GetSafeNormal());
    
	// Dot 값이 0~1 사이일 때 (정면 방향일 때) 보너스를 줌
	// 보너스는 거리의 일정 비율로 적용하여, 먼 곳에서도 시야 보너스가 의미있게 만듦
	const float AngleBonus = FMath::Max(0.f, Dot) * (DistanceScore * 0.5f); // 정면일수록 거리의 50%만큼 보너스

	// 최종 우선순위 = 거리 - 시야각 보너스
	float FinalPriority = DistanceScore - AngleBonus;
    
	return FinalPriority;*/
}

bool UChunkScheduler::IsInViewFrustum(ATerrainChunk* Chunk) const
{
	if (!Chunk)
	{
		return false;
	}
	if (!Chunk || !ChunkManager)
	{
		return false;
	}

	// 1. 박스의 반경(Extent) 계산
	// ChunkManager->VoxelSize: 복셀 하나의 크기
	// CHUNK_SIZE: 한 청크에 들어가는 복셀 개수 (예: 32)
	const float HalfSize = CHUNK_SIZE * ChunkManager->VoxelSize * 0.5f;
	const FVector BoxExtent(HalfSize);

	// 2. 박스의 중심(Center) 좌표 계산
	// ChunkToWorldLocation이 청크의 최소점(Min Corner)을 반환한다고 가정
	const FVector BoxMinCorner = ChunkManager->ChunkToWorldLocation(Chunk->ChunkCoordinate);
	const FVector BoxCenter = BoxMinCorner + BoxExtent; // 중심점 = 최소점 + 반경

	// 3. 정확한 중심점과 반경으로 교차 검사
	return ViewFrustum.IntersectBox(BoxCenter, BoxExtent);

	/*FVector Origin = ChunkManager->ChunkToWorldLocation(Chunk->ChunkCoordinate);
	FVector Extent = FVector(CHUNK_SIZE * ChunkManager->VoxelSize);
	// 엔진의 최적화된 교차 검사 함수 사용
	return ViewFrustum.IntersectBox(Origin, Extent);*/
}

void UChunkScheduler::UpdateCameraViewProjection(UWorld* WorldContext)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UChunkScheduler::UpdateCameraViewProjection);
	APlayerController* PC = UGameplayStatics::GetPlayerController(reinterpret_cast<UObject*>(WorldContext), 0);
	if (!PC || !PC->PlayerCameraManager) return;

	// 1. View 정보 가져오기 (이전과 동일)
	const FVector ViewLocation = PC->PlayerCameraManager->GetCameraLocation();
	const FRotator ViewRotation = PC->PlayerCameraManager->GetCameraRotation();

	// --- 뷰 행렬 생성 방식 변경 ---

	// 2. 카메라의 Up 벡터와 Forward 벡터를 명확하게 구합니다.
	const FVector ForwardVector = ViewRotation.Vector();
	const FVector UpVector = ViewRotation.Quaternion().GetUpVector(); // 쿼터니언에서 Up 벡터를 얻는 것이 가장 안정적입니다.

	// 3. FLookAtMatrix를 사용하여 뷰 행렬 생성 (가장 중요!)
	// 파라미터: (카메라 위치, 바라볼 지점, 카메라의 Up 방향)
	const FMatrix ViewMatrix = FLookAtMatrix(ViewLocation, ViewLocation + ForwardVector, UpVector);

	// --- 이하 로직은 동일 ---

	// 4. Projection Matrix 계산
	const FMinimalViewInfo ViewInfo = PC->PlayerCameraManager->GetCameraCacheView();
	const FMatrix ProjectionMatrix = ViewInfo.CalculateProjectionMatrix();

	// 5. View-Projection Matrix 결합
	const FMatrix ViewProjectionMatrix = ViewMatrix * ProjectionMatrix;

	// 6. 결합된 행렬로 FConvexVolume (프러스텀) 생성
	GetViewFrustumBounds(ViewFrustum, ViewProjectionMatrix, true);

	// 캐싱하는 변수들도 업데이트
	CachedCameraLocation = ViewLocation;
	CachedCameraForward = ForwardVector;
	if (ChunkManager)
	{
		CachedCameraChunkCoord = ChunkManager->WorldToChunkCoord(CachedCameraLocation);
	}
	//디버깅용
	/*
	const FVector Forward = ViewRotation.Vector();
	const FVector Up = ViewRotation.Quaternion().GetUpVector();
	const FVector Right = ViewRotation.Quaternion().GetRightVector();
	const float ArrowLength = 200.f; // 화살표 길이
	// Forward 벡터 (빨간색)
	DrawDebugDirectionalArrow(GetWorld(), ViewLocation, ViewLocation + Forward * ArrowLength, 50.f, FColor::Red, false, -1.f, 0, 5.f);
	// Up 벡터 (녹색)
	DrawDebugDirectionalArrow(GetWorld(), ViewLocation, ViewLocation + Up * ArrowLength, 50.f, FColor::Green, false, -1.f, 0, 5.f);
	// Right 벡터 (파란색)
	DrawDebugDirectionalArrow(GetWorld(), ViewLocation, ViewLocation + Right * ArrowLength, 50.f, FColor::Blue, false, -1.f, 0, 5.f);
*/
}

int32 UChunkScheduler::GetChunkCoordDistance(const FIntVector& A, const FIntVector& B) const
{
	return FMath::Max3(FMath::Abs(A.X - B.X), FMath::Abs(A.Y - B.Y), FMath::Abs(A.Z - B.Z)); // Manhattan
	//return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y) + FMath::Abs(A.Z - B.Z); // Manhattan
}

int32 UChunkScheduler::GetDistanceFromCamera(const ATerrainChunk* Chunk) const
{
	if (!Chunk) return INT32_MAX;
	return GetChunkCoordDistance(Chunk->ChunkCoordinate, CachedCameraChunkCoord);
}


bool UChunkScheduler::NeedsPhysicsInteraction(ATerrainChunk* Chunk/*, const FVector& PlayerLocation*/) const
{
	//return false;
	if (!Chunk) return false;
	return GetDistanceFromCamera(Chunk) <= 1;
	/*const float PhysicsRange = 1000.f; // example threshold
	return FVector::DistSquared(Chunk->GetActorLocation(), PlayerLocation) < FMath::Square(PhysicsRange);*/
}

double UChunkScheduler::EstimateCost(ATerrainChunk* Chunk) const
{
	if (!Chunk || !IsValid(Chunk) || IsChunkDestroyed(Chunk))
		return DBL_MAX;
	return Chunk ? static_cast<double>(Chunk->GetVerticesNum()) : DBL_MAX;
}

bool UChunkScheduler::IsChunkDestroyed(ATerrainChunk* Chunk) const
{
	return !IsValid(Chunk)/* || Chunk->kill()*/;
}

void UChunkScheduler::Release()
{
	UE::Tasks::TTask<FChunkTask> NewTask;
	while (NewMeshTasks.Dequeue(NewTask))
	{
		if (NewTask.IsValid() && !NewTask.IsCompleted())
		{
			// Task가 완료될 때까지 이 스레드를 블로킹합니다.
			// ShutdownEvent가 트리거되었으므로 대부분 빠르게 종료됩니다.
			NewTask.Wait();
		}
	}
	for (auto& Elem : UpdateMeshTaskMap)
	{
		UE::Tasks::TTask<FChunkTask>& UpdateTask = Elem.Value;
		if (UpdateTask.IsValid() && !UpdateTask.IsCompleted())
		{
			UpdateTask.Wait();
		}
	}
	UpdateMeshTaskMap.Empty();
	GenQueue_Physics.Empty();
	GenQueue_Render.Empty();
	GenQueue_Background.Empty();
	ChunkManager = nullptr;
	TerrainManager = nullptr;
}
