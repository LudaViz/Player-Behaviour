// Copyright 2025 J2K2. All Rights Reserved.

#include "Chunk/TerrainChunk.h"

#include "MaterialDomain.h"
#include "Chunk/ChunkData.h"
#include "Core/ChunkManager.h"
#include "Core/TerrainManager.h"
#include "Mesh/CustomProceduralMeshComponent.h"
#include "Mesh/LODCalculator.h"
#include "Materials/Material.h"

DCG_DISABLE_OPTIMIZATION

// Sets default values
ATerrainChunk::ATerrainChunk()
{
	// 프로시저럴 메시 컴포넌트 생성 및 등록
	ProceduralMesh = CreateDefaultSubobject<UCustomProceduralMeshComponent>(TEXT("ProceduralMesh"));
	SetRootComponent(ProceduralMesh); // 또는 다른 RootComp에 Attach

	ProceduralMesh->SetCanEverAffectNavigation(true);

	// 청크 데이터와 마칭 큐브 생성기는 nullptr로 초기화
	MarchingCubesGenerator = nullptr;
	LODCalculator = nullptr;
}
void ATerrainChunk::SetActive(bool bActive)
{
	bActiveInGame = bActive;
	SetActorHiddenInGame(!bActive);
	SetActorEnableCollision(bActive);
	SetActorTickEnabled(bActive);
}

void ATerrainChunk::InitializeChunk(const FIntVector& InChunkCoord,  FCaveMarchingCubes* InMarchingCubesGenerator, ATerrainManager* InManager,const FVector& InLocation, const FVector& InScale)
{
	ChunkCoordinate = InChunkCoord;
	MarchingCubesGenerator = InMarchingCubesGenerator;
	Manager = InManager;
	LODCalculator = Manager->LODCalculator;
	//ProceduralMesh->SetMobility(EComponentMobility::Movable);
	//ProceduralMesh->Activate(true);
	ProceduralMesh->SetMobility(EComponentMobility::Movable);
	SetActorLocation(InLocation);
	SetActorScale3D(InScale);
	// 최적화 옵션 (선택)
	ProceduralMesh->SetCastShadow(false);

	ProceduralMesh->BoundsScale=Manager->ChunkManager->BoundsScale;
	// 최적화 옵션
	ProceduralMesh->bUseComplexAsSimpleCollision = true;
	ProceduralMesh->bUseAsyncCooking = false;
	//ProceduralMesh->SetMobility(EComponentMobility::Static); // 이건 나중에 액터 위치 변경할때 풀어야함.

	ProceduralMesh->SetMobility(EComponentMobility::Static);
	ProceduralMesh->SetCollisionObjectType(ECC_WorldStatic);
	ProceduralMesh->SetCollisionResponseToAllChannels(ECR_Block);

	InitLODCache();
}

//나중에 적용 안하고 반환만 하게 한 후에 밖에서 적용하도록?
void ATerrainChunk::PrepareMesh(int32 LODLevel, const FChunkData* ChunkData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ATerrainChunk::PrepareMesh)
	if (ChunkState != ETerrainChunkState::NeedsPrepare) return;
	if (!MarchingCubesGenerator) return;
	SetChunkState(ETerrainChunkState::Preparing);
	PrepareTaskGeneration = (PrepareTaskGeneration+1)%100000;
	const int32 MyGeneration = PrepareTaskGeneration;
	const FIntVector MyCoord = ChunkCoordinate;

	LODCalculator->CalculateLOD(ChunkCoordinate, Manager->GetReferenceChunkCoord(),
		CurrentLOD, CurrentLODFace);
	
	//만약 LOD를 바꿔야 한다면 다른 State 추가
	//ChangeLOD 등의 State 시에는 반환만
	/*if (bNeedsRebuild)
	{
		LODMeshCache.Empty();
		bNeedsRebuild=false;.
	}
	else
	{
		// 이미 캐시된 메시가 있으면 따로 생성하지 않음.
		if (LODMeshCache.Contains(LODLevel))
		{
			return;
		}
	}*/

	FChunkLODLevel MeshData;
	MarchingCubesGenerator->GenerateMesh(ChunkData, CurrentLOD, MeshData);
	if (/*LODMeshCache.IsEmpty()||*/ChunkState == ETerrainChunkState::Preparing&&PrepareTaskGeneration==MyGeneration&&MyCoord==ChunkCoordinate)
	{
		FScopeLock Lock(&MeshDataLock);
		LODMeshCache.LODLevels[CurrentLOD] = MeshData;
		SetChunkState(ETerrainChunkState::NeedsUpload);
	}
}

void ATerrainChunk::UploadMesh(/*bool bEnableWireframe*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ATerrainChunk::UploadMesh)
	if (ChunkState != ETerrainChunkState::NeedsUpload) return;
	SetChunkState(ETerrainChunkState::Uploading);
	
	const FChunkLODLevel& MultiMeshData = GetCurrentLODMeshData();
	FScopeLock Lock(&MeshDataLock);

	// Without this, old LOD geometry stacks underneath the new geometry, causing the Z-fighting!
	ProceduralMesh->ClearAllMeshSections();

	// TODO : transition Border 메시도 업로드 
	int32 SectionIndex = 0;
	for (const TPair<mat_type, FChunkMaterialSection>& Pair : MultiMeshData.MaterialSections)
	{
		const mat_type MaterialType = Pair.Key;
		const FChunkMaterialSection& MaterialEntry = Pair.Value;
		
		if (UMaterialInterface* Material = Manager->GetMaterial(MaterialType))
		{
			// if (bEnableWireframe)
				// ProceduralMesh->SetMaterial(SectionIndex,GEngine->WireframeMaterial);
			// else
				ProceduralMesh->SetMaterial(SectionIndex, Material);
		}
		const FMeshData& Interior = MaterialEntry.Interior; // TODO : LOD
		ProceduralMesh->CreateMeshSection_LinearColor(
			SectionIndex,
			Interior.Vertices,
			Interior.Triangles,
			Interior.Normals,
			Interior.UVs,
			Interior.Colors,
			Interior.Tangents,
			true
		);
		SectionIndex++;

		if (CurrentLODFace == -1)
		{
			const FMeshData& RegularFace = MaterialEntry.BorderRegular;
			if (UMaterialInterface* Material = Manager->GetMaterial(MaterialType))
			{
				// if (bEnableWireframe)
					// ProceduralMesh->SetMaterial(SectionIndex,GEngine->WireframeMaterial);
				// else
					ProceduralMesh->SetMaterial(SectionIndex, Material);
			}
			ProceduralMesh->CreateMeshSection_LinearColor(
				SectionIndex,
				RegularFace.Vertices,
				RegularFace.Triangles,
				RegularFace.Normals,
				RegularFace.UVs,
				RegularFace.Colors,
				RegularFace.Tangents,
				true
			);
			SectionIndex++;
		}
		else
		{
			const FMeshData& RegularFace = MaterialEntry.BorderTransitions.Borders[CurrentLODFace];
			if (UMaterialInterface* Material = Manager->GetMaterial(MaterialType))
			{
				// if (bEnableWireframe)
				// ProceduralMesh->SetMaterial(SectionIndex,GEngine->WireframeMaterial);
				// else
					ProceduralMesh->SetMaterial(SectionIndex, Material);
			}
			ProceduralMesh->CreateMeshSection_LinearColor(
				SectionIndex,
				RegularFace.Vertices,
				RegularFace.Triangles,
				RegularFace.Normals,
				RegularFace.UVs,
				RegularFace.Colors,
				RegularFace.Tangents,
				true
			);
			SectionIndex++;
		}
	}

	int32 NumExistingSections = ProceduralMesh->GetNumSections();
	for (int32 i = SectionIndex; i < NumExistingSections; ++i)
	{
		ProceduralMesh->ClearMeshSection(i);
	}

	// This forcibly rips the mesh out of the world and drops it back in. 
	// This updates the bounding box and forces the Dynamic NavMesh to instantly recognize the chunk.
	ProceduralMesh->UpdateBounds();
	ProceduralMesh->UnregisterComponent();
	ProceduralMesh->RegisterComponent();

	SetChunkState(ETerrainChunkState::Complete);
}

void ATerrainChunk::UpdateLOD(int32 NewLOD, int32 NewFace)
{
	// When LOD changes, update both interior and border
	if (NewLOD != CurrentLOD)
	{
		CurrentLOD = NewLOD;
		UploadMesh();
	}
	// When LOD remains but Face changes, update only borders
	else if (NewFace != CurrentLODFace && CurrentLOD != 0)
	{
		CurrentLODFace = NewFace;
		UploadMeshFace();
	}
}

void ATerrainChunk::ClearMesh()
{
	ProceduralMesh->bUseAsyncCooking=false;

	// This forces the Details Panel to drop the ghost materials (Elements 2, 3, etc.)
	int32 NumSections = ProceduralMesh->GetNumSections();
	for (int32 i = 0; i < NumSections; ++i)
	{
		ProceduralMesh->ClearMeshSection(i);
	}

	ProceduralMesh->ClearAllMeshSections();
	ProceduralMesh->ClearCollisionConvexMeshes();

	ProceduralMesh->ClearAllMeshSections();
	ProceduralMesh->ClearCollisionConvexMeshes();
	//ProceduralMesh->CleanUpOverrideMaterials();
	// 
	// Ensure Async Cooking stays OFF so the collision bakes instantly and triggers the NavMesh
	ProceduralMesh->bUseAsyncCooking = false;
}


void ATerrainChunk::ResetChunk()
{
	// Lock the mesh data before emptying it!
	// If a background thread is writing to this while we lower the Render Distance, it crashes instantly.
	FScopeLock Lock(&MeshDataLock);
	LODMeshCache.LODLevels.Empty();
	ClearMesh();
	SetChunkState(ETerrainChunkState::Unloaded);
	//PrepareTaskGeneration=0;
	//ProceduralMesh->Activate(false);
	SetActive(false);
}

UMaterialInterface* ATerrainChunk::GetMaterial(uint8 MaterialType) const
{
	if (OverrideMaterialMap.Contains(MaterialType))
		return OverrideMaterialMap[MaterialType];

	if (Manager.IsValid())	return Manager->GetMaterial(MaterialType);
	UE_LOG(LogDCG,Error,TEXT("TerrainChunk::Tick : Material type %d not found"),MaterialType);
	return UMaterial::GetDefaultMaterial(EMaterialDomain::MD_Surface);
}

int32 ATerrainChunk::GetVerticesNum(int32 LODLevel)
{
	if (LODMeshCache.LODLevels.IsEmpty()||LODLevel >= LODMeshCache.LODLevels.Num())
	{
		return -1;
	}

	if (LODLevel == -1)
	{
		LODLevel = CurrentLOD;
	}
	
	const FChunkLODLevel& MeshData = LODMeshCache.LODLevels[LODLevel];
	if (MeshData.MaterialSections.IsEmpty())
		return 0;

	int32 VerticesNum = 0;
	for (const TPair<unsigned char, FChunkMaterialSection>& Section : MeshData.MaterialSections)
	{
		const FChunkMaterialSection& LODData = Section.Value;
		VerticesNum += LODData.Interior.Vertices.Num();
		VerticesNum += LODData.BorderRegular.Vertices.Num();
	}
	return VerticesNum;
}

const FChunkLODLevel& ATerrainChunk:: GetLODMeshData(int32 LODLevel) const
{
	return LODMeshCache.LODLevels[LODLevel];
}

const FChunkLODLevel& ATerrainChunk::GetCurrentLODMeshData()
{
	return GetLODMeshData(CurrentLOD);
}

void ATerrainChunk::UploadMeshFace()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ATerrainChunk::UploadMeshBorder)
	if (ChunkState != ETerrainChunkState::NeedsUpload) return;
	SetChunkState(ETerrainChunkState::Uploading);
	
	const FChunkLODLevel& MultiMeshData = GetCurrentLODMeshData();
	FScopeLock Lock(&MeshDataLock);

	// Border's SectionIndex is odd number.
	int32 SectionIndex = 1;
	for (const TPair<mat_type, FChunkMaterialSection>& Pair : MultiMeshData.MaterialSections)
	{
		const mat_type MaterialType = Pair.Key;
		const FChunkMaterialSection& MaterialEntry = Pair.Value;
		
		const FMeshData& RegularFace = MaterialEntry.BorderTransitions.Borders[CurrentLODFace];
		if (UMaterialInterface* Material = Manager->GetMaterial(MaterialType))
		{
			// if (bEnableWireframe)
			// ProceduralMesh->SetMaterial(SectionIndex,GEngine->WireframeMaterial);
			// else
			ProceduralMesh->SetMaterial(SectionIndex, Material);
		}
		ProceduralMesh->CreateMeshSection_LinearColor(
			SectionIndex,
			RegularFace.Vertices,
			RegularFace.Triangles,
			RegularFace.Normals,
			RegularFace.UVs,
			RegularFace.Colors,
			RegularFace.Tangents,
			true
		);
		SectionIndex += 2;
	}

	int32 NumExistingSections = ProceduralMesh->GetNumSections();
	for (int32 i = SectionIndex; i < NumExistingSections; ++i)
	{
		ProceduralMesh->ClearMeshSection(i);
	}

	// This forcibly rips the mesh out of the world and drops it back in. 
	// This updates the bounding box and forces the Dynamic NavMesh to instantly recognize the chunk.
	ProceduralMesh->UpdateBounds();
	ProceduralMesh->UnregisterComponent();
	ProceduralMesh->RegisterComponent();

	SetChunkState(ETerrainChunkState::Complete);
}

void ATerrainChunk::InitLODCache()
{
	LODMeshCache.LODLevels.SetNum(CHUNK_MAX_LOD + 1);

	const int32 NumMat = Manager->GetNumMaterials();

	for (FChunkLODLevel& LODLevel : LODMeshCache.LODLevels)
	{
		for (mat_type MatType = 0; MatType < NumMat; MatType++)
		{
			LODLevel.MaterialSections.Add(MatType);
		}
	}
}
