// Copyright 2025 J2K2. All Rights Reserved.

#include "Core/TerrainManager.h"
#include "MaterialDomain.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "Chunk/ChunkPool.h"
#include "Core/ChunkManager.h"
#include "Core/ModifierManager.h"
#include "Core/NoiseGenerator.h"
#include "Core/Noise/TerrainData.h"
#include "Core/Scheduler/ChunkScheduler.h"
#include "Mesh/LODCalculator.h"
#include "Materials/Material.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "UObject/UObjectGlobals.h"

// Sets default values
ATerrainManager::ATerrainManager()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;

	// 컴포넌트 생성 및 초기화
	ChunkManager = CreateDefaultSubobject<UChunkManager>(TEXT("ChunkManager"));
	NoiseGenerator = CreateDefaultSubobject<UNoiseGenerator>(TEXT("NoiseGenerator"));
	LODCalculator = CreateDefaultSubobject<ULODCalculator>(TEXT("LODCalculator"));
	ModifierManager = CreateDefaultSubobject<UModifierManager>(TEXT("ModifierManager"));
	
	//Procedural 부착용
	//RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	/*TerrainMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMeshComponent"));
	SetRootComponent(TerrainMeshComponent);
	// 최적화 옵션 (선택)
	TerrainMeshComponent->SetCastShadow(false);

	// 최적화 옵션
	TerrainMeshComponent->bUseComplexAsSimpleCollision = false;
	TerrainMeshComponent->bUseAsyncCooking = true;
	TerrainMeshComponent->SetMobility(EComponentMobility::Static); // 이건 나중에 액터 위치 변경할때 풀어야함.*/
}

ATerrainManager::~ATerrainManager()
{
	//UE_LOG(LogChunk, Error, TEXT("~ATerrainManager"));
}

// Called when the game starts or when spawned
void ATerrainManager::BeginPlay()
{
	Super::BeginPlay();

	TimeBudgetState.Initialize(TimeBudgetConfig);

	if (!TerrainDataInfo.DataTable || TerrainDataInfo.DataTable->GetRowStruct() != FTerrainData::StaticStruct())
	{
		UE_LOG(LogDCG, Error, TEXT("TerrainDataInfo is invalid!"));
		bIsInitialized = false;
		return;
	}

	const FString ContextString(TEXT("ATerrainManager::BeginPlay"));
	TArray<FName> RowNames = TerrainDataInfo.DataTable->GetRowNames();
	TerrainDatas.Empty();

	for (const FName& RowName : RowNames)
	{
		FTerrainData* RowData = TerrainDataInfo.DataTable->FindRow<FTerrainData>(RowName, ContextString);
		if (RowData)
		{
			RowData->RowName = RowName;
			TerrainDatas.Add(RowData);
		}
	}

	TerrainDataCount = TerrainDatas.Num();

	if (ChunkManager)
	{
		// Clean up any editor preview ghost state
		ChunkManager->Release();
		ChunkManager->ActiveChunks.Empty();

		if (ChunkManager->ChunkPool) ChunkManager->ChunkPool->CleanupPool();
		ChunkManager->ChunkPool = nullptr;

		SyncSettingsToChunkManager();
		ChunkManager->Initialize(this);
	}

	if (ModifierManager)
	{
		ModifierManager = NewObject<UModifierManager>(this);
		ModifierManager->Initialize(this);
	}

	if (NoiseGenerator)
	{
		NoiseGenerator = NewObject<UNoiseGenerator>(this);
		NoiseGenerator->Initialize(TerrainDatas);
	}

	if (ChunkManager)
	{
		ChunkManager->LoadInitialChunks(GetActorLocation(), NoiseGenerator);
	}

	bIsInitialized = true;
}

void ATerrainManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	//ChunkManager->SaveChunksToDisk("TestWorld");
	ChunkManager->Release();
	//UE_LOG(LogTemp, Warning, TEXT("ATerrainManager::EndPlay. Reason = %d"), (int32)EndPlayReason);
}

// Called every frame
void ATerrainManager::Tick(float DeltaTime)
{
#if WITH_EDITOR
	// If the cooldown timer is active, count it down
	if (RegenerationCooldownTimer > 0.0f)
	{
		RegenerationCooldownTimer -= DeltaTime;

		// When it hits zero, the user has stopped editing. Safely regenerate!
		if (RegenerationCooldownTimer <= 0.0f)
		{
			RegenerationCooldownTimer = 0.0f;
			ForceEditorRegeneration();
		}
	}
#endif

	if (!bIsInitialized)
		return;

	Super::Tick(DeltaTime);
	TimeBudgetState.BeginFrame();

	APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;

	// Fix: Add a fallback for the Editor Viewport
	if (PlayerController && PlayerController->GetPawn())
	{
		ReferenceLocation = PlayerController->GetPawn()->GetActorLocation();
	}
	else
	{
		// When in the editor, center the generation around the TerrainManager actor
		ReferenceLocation = GetActorLocation();
	}

	if (PlayerController && PlayerController->PlayerCameraManager)
	{
		const FVector CamLoc = PlayerController->PlayerCameraManager->GetCameraLocation();
		const FVector CamForward = PlayerController->PlayerCameraManager->GetActorForwardVector();
	}

	ChunkManager->ChunkScheduler->UpdateCameraViewProjection(GetWorld());

	ModifierManager->Tick(DeltaTime);

	ProcessChunkRegeneration();

	ChunkManager->ProcessChunks();
	ChunkManager->UpdateChunkStreaming(ReferenceLocation, NoiseGenerator);

	// LOD 
	UpdateLOD();

	if (bLogTimeBudget) TimeBudgetState.LogSectionTimes();
	if (bDrawBounds) DrawDebugBounds();
	if (bDrawDebugPoint) DrawDebugPoints();
	if (bDrawDebugGradient) DrawDebugGradients();

	//FlushDestructionFX();
}

void ATerrainManager::UpdateLOD()
{
	// if (ChunkManager && LODCalculator)
	// {
	// 	for (ATerrainChunk* Chunk : ChunkManager->ChunkPool->RentedChunks)
	// 	{
	// 		// 메시를 새로 생성 필요
	// 		if (Chunk->ChunkState == EChunkState::LoadingMesh)
	// 		{
	// 			Chunk->PrepareMesh(Chunk->CurrentLOD);
	// 			Chunk->GenerateMesh();
	// 			Chunk->ChunkState = EChunkState::Loaded;
	// 		}
	// 	}
	// 	// 거리 기반으로 LOD 계산 및 적용
	// 	// 예시: ChunkManager->UpdateChunksLOD(ReferenceLocation, RenderDistance, LODCalculator);
	// }
}

FIntVector ATerrainManager::GetReferenceChunkCoord() const
{
	return FIntVector(ReferenceLocation / (CHUNK_SIZE * ChunkManager->VoxelSize));
}

void ATerrainManager::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	check(ChunkManager); // 필수 컴포넌트 누락 확인
	check(NoiseGenerator);
	check(LODCalculator);
	check(ModifierManager);
}

void ATerrainManager::SaveChunkToDisk(const FIntVector& ChunkCoord)
{
	if (ChunkManager)
	{
		// 특정 청크를 디스크에 저장
		// 예시: ChunkManager->SaveChunkData(ChunkCoord);
	}
}

void ATerrainManager::LoadChunkFromDisk(const FIntVector& ChunkCoord)
{
	if (ChunkManager)
	{
		// 디스크에서 청크 데이터 로드
		// 예시: ChunkManager->LoadChunkData(ChunkCoord);
	}
}

void ATerrainManager::DrawDebugBounds()
{
	if (ChunkManager)
	{
		ChunkManager->DrawDebugBounds(GetWorld(), FColor::Green);
	}
}

void ATerrainManager::DrawDebugPoints()
{
	if (ChunkManager)
	{
		ChunkManager->DrawDebugPoints(GetWorld(), PointSamplingStride, MaterialTypeForDebug, IsoSurfaceLevelForDebug);
	}
}

void ATerrainManager::DrawDebugGradients()
{
	if (ChunkManager)
	{
		ChunkManager->DrawDebugGradients(GetWorld(), GradientSamplingStride);
	}
}

UMaterialInterface* ATerrainManager::GetMaterial(uint8 Type) const
{
	return TerrainDatas.IsValidIndex(Type)
		       ? TerrainDatas[Type]->Material
		       : UMaterial::GetDefaultMaterial(EMaterialDomain::MD_Surface);
}

uint8 ATerrainManager::GetNumMaterials() const
{
	return TerrainDataCount;
}

void ATerrainManager::RefreshTerrain(bool bForceRegeneration)
{
	if (!ChunkManager || !ChunkManager->ChunkPool)
	{
		UE_LOG(LogDCG, Warning, TEXT("ATerrainManager::RefreshTerrain - ChunkManager or ChunkPool is null"));
		return;
	}

	if (bForceRegeneration)
	{
		// 모든 활성화된 청크들을 재생성 대상으로 마킹
		TArray<FIntVector> AllChunks;
		for (ATerrainChunk* Chunk : ChunkManager->ChunkPool->GetRentedChunks())
		{
			if (Chunk)
			{
				AllChunks.Add(Chunk->ChunkCoordinate);
			}
		}

		MarkChunksForRegeneration(AllChunks);
		UE_LOG(LogDCG, Log, TEXT("ATerrainManager::RefreshTerrain - Marked %d chunks for regeneration"),
		       AllChunks.Num());
	}
}

void ATerrainManager::RefreshChunksInRadius(const FVector& Center, float Radius)
{
	if (!ChunkManager)
	{
		return;
	}

	TArray<FIntVector> ChunksToRefresh;
	const float ChunkSize = 32.0f * 100.0f; // 청크 크기 (cm)
	const int32 ChunkRadius = FMath::CeilToInt(Radius / ChunkSize);

	FIntVector CenterChunk = FIntVector(
		FMath::FloorToInt(Center.X / ChunkSize),
		FMath::FloorToInt(Center.Y / ChunkSize),
		FMath::FloorToInt(Center.Z / ChunkSize)
	);

	// 영향 받는 청크들 수집
	for (int32 x = -ChunkRadius; x <= ChunkRadius; ++x)
	{
		for (int32 y = -ChunkRadius; y <= ChunkRadius; ++y)
		{
			for (int32 z = -ChunkRadius; z <= ChunkRadius; ++z)
			{
				FIntVector ChunkCoord = CenterChunk + FIntVector(x, y, z);
				FVector ChunkCenter = FVector(ChunkCoord) * ChunkSize + FVector(ChunkSize * 0.5f);

				if (FVector::Dist(Center, ChunkCenter) <= Radius)
				{
					ChunksToRefresh.Add(ChunkCoord);
				}
			}
		}
	}

	MarkChunksForRegeneration(ChunksToRefresh);
	UE_LOG(LogDCG, Log, TEXT("ATerrainManager::RefreshChunksInRadius - Marked %d chunks for regeneration"),
	       ChunksToRefresh.Num());
}

void ATerrainManager::RefreshChunksInBounds(const FBox& Bounds)
{
	if (!ChunkManager)
	{
		return;
	}

	TArray<FIntVector> ChunksToRefresh;
	const float ChunkSize = 32.0f * 100.0f;

	FIntVector MinChunk = FIntVector(
		FMath::FloorToInt(Bounds.Min.X / ChunkSize),
		FMath::FloorToInt(Bounds.Min.Y / ChunkSize),
		FMath::FloorToInt(Bounds.Min.Z / ChunkSize)
	);

	FIntVector MaxChunk = FIntVector(
		FMath::CeilToInt(Bounds.Max.X / ChunkSize),
		FMath::CeilToInt(Bounds.Max.Y / ChunkSize),
		FMath::CeilToInt(Bounds.Max.Z / ChunkSize)
	);

	for (int32 x = MinChunk.X; x <= MaxChunk.X; ++x)
	{
		for (int32 y = MinChunk.Y; y <= MaxChunk.Y; ++y)
		{
			for (int32 z = MinChunk.Z; z <= MaxChunk.Z; ++z)
			{
				ChunksToRefresh.Add(FIntVector(x, y, z));
			}
		}
	}

	MarkChunksForRegeneration(ChunksToRefresh);
	UE_LOG(LogDCG, Log, TEXT("ATerrainManager::RefreshChunksInBounds - Marked %d chunks for regeneration"),
	       ChunksToRefresh.Num());
}

void ATerrainManager::MarkChunksForRegeneration(const TArray<FIntVector>& ChunkCoords)
{
	for (const FIntVector& ChunkCoord : ChunkCoords)
	{
		ChunksToRegenerate.Add(ChunkCoord);
	}

	UE_LOG(LogDCG, Log, TEXT("ATerrainManager::MarkChunksForRegeneration - Added %d chunks to regeneration queue"),
	       ChunkCoords.Num());
}

void ATerrainManager::ProcessChunkRegeneration()
{
	if (ChunksToRegenerate.Num() == 0 || !ChunkManager)
	{
		return;
	}

	// 프레임당 최대 처리 개수 제한
	constexpr int32 MaxChunksPerFrame = 3;
	int32 ProcessedCount = 0;

	TArray<FIntVector> ProcessedChunks;
	for (auto It = ChunksToRegenerate.CreateIterator(); It && ProcessedCount < MaxChunksPerFrame; ++It)
	{
		FIntVector ChunkCoord = *It;

		// 청크 재생성 (실제 ChunkManager 인터페이스에 따라 구현)
		// ChunkManager->ForceReloadChunk(ChunkCoord, NoiseGenerator);
		ProcessedChunks.Add(ChunkCoord);

		It.RemoveCurrent();
		++ProcessedCount;
	}

	if (ProcessedChunks.Num() > 0)
	{
		OnTerrainRefreshed.Broadcast(ProcessedChunks);
		UE_LOG(LogDCG, Log, TEXT("ATerrainManager::ProcessChunkRegeneration - Processed %d chunks"),
		       ProcessedChunks.Num());
	}
}

/*void ATerrainManager::SpawnDestructionParticle(uint8 MaterialType, const FVector& Location/*, const FVector& Velocity#1#)
{
	if (!MaterialTypeToParticleMap.Find(MaterialType) || !MaterialTypeToParticleMap[MaterialType])
		return;

	UNiagaraSystem* NiagaraFX = MaterialTypeToParticleMap[MaterialType];
	UMaterialInterface* Mat = MaterialList.IsValidIndex(MaterialType) ? MaterialList[MaterialType] : nullptr;

	UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		NiagaraFX,
		Location,
		FRotator::ZeroRotator,
		FVector(1.f),
		true,  // AutoDestroy
		true,  // AutoActivate
		ENCPoolMethod::None,
		true   // PreCullCheck
	);

	if (NiagaraComp && Mat)
	{
		NiagaraComp->SetVariableMaterial(FName("User.ChunkMaterial"), Mat);
		//NiagaraComp->SetVariableVec3(FName("User.Velocity"), Velocity); // 필요 시
	}
}*/
void ATerrainManager::InitDestructionFX()
{
	for (uint8 i = 0; i < TerrainDataCount; ++i)
	{
		if (!DefaultDestructionSystem) continue;


		/*UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
			DefaultDestructionSystem,
			GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset,
			false);*/
		UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			reinterpret_cast<UObject*> (GetWorld()),
			DefaultDestructionSystem,
			GetActorLocation(),
			GetActorRotation()
		);

		if (NiagaraComp)
		{
			//NiagaraComp->SetAutoActivate(false);
			NiagaraComp->SetVariableMaterial(FName("User.ChunkMaterial"), TerrainDatas[i]->Material);
			NiagaraEmitters.Add(i, NiagaraComp);
		}
	}
}

/*void ATerrainManager::QueueDestructionFX(uint8 MaterialType, const FVector& WorldPos)
{
	MaterialDestructionPositions.FindOrAdd(MaterialType).Add(WorldPos);
}

void ATerrainManager::FlushDestructionFX()
{
	for (auto& Pair : MaterialDestructionPositions)
	{
		if (Pair.Value.IsEmpty())continue;
		uint8 MatType = Pair.Key;
		TArray<FVector>& Locations = Pair.Value;

		if (UNiagaraComponent* NiagaraComp = NiagaraEmitters.FindRef(MatType))
		{
			NiagaraComp->SetVariableInt("User.SpawnCount",Locations.Num()/100);
			UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayPosition(
				NiagaraComp, "User.ExplosionLocations", Locations);
			FVector Center = ReferenceLocation;
			FVector Offset = FVector(100,100,100);
			NiagaraComp->SetSystemFixedBounds(FBox(Center-Offset,Center+Offset));
			NiagaraComp->Activate(true); // Burst 발생
		}
	}

	MaterialDestructionPositions.Reset(); // 다음 틱을 위해 초기화
}*/
void ATerrainManager::TriggerDestructionFXImmediate(uint8 MaterialType, const TArray<FVector>& Locations,const int SpawnCount)
{
	if (!Locations.Num()||SpawnCount<=0)
		return;

	if (UNiagaraComponent* NiagaraComp = NiagaraEmitters.FindRef(MaterialType))
	{
		// Niagara 변수 설정
		NiagaraComp->SetVariableInt("User.SpawnCount", SpawnCount==0?Locations.Num():SpawnCount); // 또는 다른 비율

		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayPosition(
			NiagaraComp, "User.ExplosionLocations", Locations);

		// Bound 업데이트
		FBox Bounds(ForceInitToZero);
		for (const FVector& Pos : Locations)
		{
			Bounds += Pos;
		}
		if (Bounds.IsValid)
		{
			FVector Center = Bounds.GetCenter();
			FVector Extent = Bounds.GetExtent() + FVector(50.f); // 약간의 여유
			NiagaraComp->SetSystemFixedBounds(FBox(Center - Extent, Center + Extent));
		}

		// Burst
		NiagaraComp->Activate(true);
	}
}

#if WITH_EDITOR
void ATerrainManager::OnObjectModified(UObject* Object)
{
	// Start a 0.5 second cooldown. If the user keeps sliding values, this keeps resetting.
	if (bLiveEditorPreview && TerrainDataInfo.DataTable && Object == TerrainDataInfo.DataTable)
	{
		RegenerationCooldownTimer = 0.5f;
	}
}

void ATerrainManager::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (!PropertyChangedEvent.Property) return;

	// Start cooldown if the preview toggle is clicked, OR if a new DataTable is assigned!
	if (bLiveEditorPreview)
	{
		RegenerationCooldownTimer = 0.5f;
	}
}
#endif

void ATerrainManager::ForceEditorRegeneration()
{
	if (!ChunkManager || !NoiseGenerator || !TerrainDataInfo.DataTable) return;

	// 1. SAFELY wait for all background tasks to finish, and wipe old memory maps
	// NOTE: This sets ChunkManager->ChunkScheduler to nullptr!
	ChunkManager->Release();

	// 2. Return active chunks to the pool instead of destroying them!
	// This eliminates the massive lag spike caused by spawning new actors every time you edit a value.
	TArray<ATerrainChunk*> ChunksToReturn;
	ChunkManager->ActiveChunks.ForEach([&](const FIntVector& Key, ATerrainChunk* Chunk) {
		if (Chunk) ChunksToReturn.Add(Chunk);
		});
	ChunkManager->ActiveChunks.Empty();

	for (ATerrainChunk* Chunk : ChunksToReturn)
	{
		Chunk->ClearMesh();
		if (ChunkManager->ChunkPool)
		{
			ChunkManager->ChunkPool->ReturnChunk(Chunk);
		}
	}

	// 3. Clear out older cache snapshots
	TerrainDatas.Empty();

	// 4. Load DataTable rows directly
	const FString ContextString(TEXT("ATerrainManager::ForceEditorRegeneration"));
	TArray<FName> RowNames = TerrainDataInfo.DataTable->GetRowNames();

	for (const FName& RowName : RowNames)
	{
		FTerrainData* RowData = TerrainDataInfo.DataTable->FindRow<FTerrainData>(RowName, ContextString);
		if (RowData)
		{
			RowData->RowName = RowName;
			TerrainDatas.Add(RowData);
		}
	}
	TerrainDataCount = TerrainDatas.Num();

	// 5. Push your new root settings down to the component
	SyncSettingsToChunkManager();

	// 6. Either Initialize a fresh pool (first run), or just recreate the Scheduler (reusing the pool)
	if (!ChunkManager->ChunkPool)
	{
		ChunkManager->Initialize(this);
	}
	else
	{
		// Release() destroyed the scheduler, so we must recreate it to process the queue
		ChunkManager->ChunkScheduler = NewObject<UChunkScheduler>(ChunkManager);
		ChunkManager->ChunkScheduler->SetTerrainManger(this);
		ChunkManager->FillDistance = ChunkManager->RenderDistance + 2;
	}

	// The plugin's internal arrays append data every time Initialize() is called. 
	// By creating fresh objects, we stop the materials from duplicating and stacking!
	if (ModifierManager)
	{
		ModifierManager = NewObject<UModifierManager>(this);
		ModifierManager->Initialize(this);
	}

	if (NoiseGenerator)
	{
		NoiseGenerator = NewObject<UNoiseGenerator>(this);
		NoiseGenerator->Initialize(TerrainDatas);
	}

	TimeBudgetState.Initialize(TimeBudgetConfig);

	// 7. Cache original loading settings and use asynchronous Scheduled Tasks
	EChunkLoadMode OriginalRuntimeMode = ChunkManager->ChunkLoadMode;
	bool bOriginalInitLoad = ChunkManager->bInitialChunkLoad;

	ChunkManager->ChunkLoadMode = EChunkLoadMode::ScheduledTask;
	ChunkManager->bInitialChunkLoad = true;

	// 8. Bake the immediate surrounding layout
	ChunkManager->LoadInitialChunks(GetActorLocation(), NoiseGenerator);

	// 9. Restore optimized structural streaming settings for game runtime
	ChunkManager->ChunkLoadMode = OriginalRuntimeMode;
	ChunkManager->bInitialChunkLoad = bOriginalInitLoad;

	// 10. Unlock the Tick function so the async queues can process the meshes
	bIsInitialized = true;
}

void ATerrainManager::SyncSettingsToChunkManager()
{
	if (ChunkManager)
	{
		ChunkManager->VoxelSize = VoxelSize;
		ChunkManager->BoundsScale = BoundsScale;
		ChunkManager->RenderDistance = RenderDistance;
		ChunkManager->bUpdateStreaming = bUpdateStreaming;
		ChunkManager->bInitialChunkLoad = bInitialChunkLoad;
		ChunkManager->InitialPoolSize = InitialPoolSize;
		ChunkManager->ChunkLoadMode = ChunkLoadMode;

		// Safely cast to uint32 for the plugin's native data types
		ChunkManager->InitialChunkLoadNum = (uint32)FMath::Max(0, InitialChunkLoadNum);
		ChunkManager->NumThreads = (uint32)FMath::Max(0, NumThreads);
		ChunkManager->NumMeshGeneratePerTick = (uint32)FMath::Max(0, NumMeshGeneratePerTick);
	}
}

bool ATerrainManager::ShouldTickIfViewportsOnly() const
{
	// Only process background ticks in the editor if Live Preview is enabled
	return bLiveEditorPreview;
}

void ATerrainManager::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITOR
	if (!OnObjectModifiedHandle.IsValid())
	{
		OnObjectModifiedHandle = FCoreUObjectDelegates::OnObjectModified.AddUObject(this, &ATerrainManager::OnObjectModified);
	}

	if (bLiveEditorPreview && GetWorld() && !GetWorld()->IsPlayInEditor())
	{
		RegenerationCooldownTimer = 0.5f;
	}
#endif
}

void ATerrainManager::Destroyed()
{
#if WITH_EDITOR
	if (OnObjectModifiedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectModified.Remove(OnObjectModifiedHandle);
		OnObjectModifiedHandle.Reset();
	}
#endif

	if (ChunkManager)
	{
		ChunkManager->Release();
		if (ChunkManager->ChunkPool)
		{
			ChunkManager->ChunkPool->CleanupPool();
		}
	}

	Super::Destroyed();
}
