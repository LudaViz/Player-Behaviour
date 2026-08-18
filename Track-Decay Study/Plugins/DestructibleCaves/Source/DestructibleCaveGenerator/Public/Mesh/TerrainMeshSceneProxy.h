// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshBuilder.h"
#include "TerrainMeshComponent.h"

/**
 * ProceduralMeshComponent.cpp 참고
 * * FProceduralMeshSceneProxy의 형태에 LOD를 추가한 형태.
 * SceneProxy -> LODResources -> MeshSection
 *
 * Static Mesh의 경우에는 MeshSection과 VertexFactory를 분리했지만, 여기서는 통합해서 사용함.
 *
 * SceneProxy의 구조
 * StaticMesh의 경우
 * FStaticMeshSceneProxy
 *			|
 * FStaticMeshRenderData 
 *			|
 * FStaticMeshVertexFactoriesArray	 /  FStaticMeshLODResourcesArray
 * (FStaticMeshVertexFactories)			(FStaticMeshLODResources)  
 *			|									|
 * FLocalVertexFactory					FStaticMeshSectionArray			/ FStaticMeshVertexBuffers,
 * (버텍스 버퍼 생성 desc)				(FStaticMeshSection)			  FRawStaticIndexBuffer
 *												|
 *										material idx, startIdx, ...      
 *
 *
 * ProcMesh의 경우
 * FProceduralMeshSceneProxy
 *			|
 * TArray<FProcMeshProxySection*>
 *			|
 * FStaticMeshVertexBuffers			/ FLocalVertexFactory
 *			|
 * FStaticMeshVertexBuffer, FPositionVertexBuffer
 * (GPU 버퍼)
 *
 *
 * 메시는 FProcMeshSection대신 FMeshData를 사용 예정
 */


/**
 * FProcMeshProxySection 참고
 * 머터리얼 당 하나의 섹션을 가집니다.
 */
class DESTRUCTIBLECAVEGENERATOR_API FTerrainMeshSection
{
public:
	UMaterialInterface* Material;
	FStaticMeshVertexBuffers VertexBuffers;
	FDynamicMeshIndexBuffer32 IndexBuffer;
	FLocalVertexFactory VertexFactory;

#if RHI_RAYTRACING
	FRayTracingGeometry RaytracingGeometry;
#endif

	FTerrainMeshSection(ERHIFeatureLevel::Type FeatureLevel)
	: Material(nullptr)
	, VertexFactory(FeatureLevel, "FTerrainMeshSection")
	{}

	~FTerrainMeshSection()
	{
		
	}
};

/**
 * 4개의 LOD 기준으로 생성
 */
class DESTRUCTIBLECAVEGENERATOR_API FTerrainMeshSectionArray : public TArray<FTerrainMeshSection, TInlineAllocator<4>>
{
	using Super = TArray<FTerrainMeshSection, TInlineAllocator<1>>;
public:
	using Super::Super;
};

/**
 * FStaticMeshLODResources 참고
 */
class DESTRUCTIBLECAVEGENERATOR_API FTerrainMeshLODResources : public FRefCountBase
{
public:
	FTerrainMeshSectionArray Sections;

	DESTRUCTIBLECAVEGENERATOR_API FTerrainMeshLODResources(bool bAddRef = true);

	DESTRUCTIBLECAVEGENERATOR_API ~FTerrainMeshLODResources();
};

using FTerrainMeshLODResourcesArray = TIndirectArray<FStaticMeshLODResources>;

/**
 * FStaticMeshSceneProxy는 FStaticMeshRenderData를 한번 감싸고 있지만, FProceduralMeshComponent는 그렇지 않음
 * 두 곳을 모두 참고해서 작성함.
 */
class DESTRUCTIBLECAVEGENERATOR_API FTerrainMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
    SIZE_T GetTypeHash() const override
    {
        static size_t UniquePointer;
        return reinterpret_cast<size_t>(&UniquePointer);
    }

    FTerrainMeshSceneProxy(const UTerrainMeshComponent* Component)
        : FPrimitiveSceneProxy(Component)
    {
        // LOD 및 Section 데이터 복사 및 초기화
        const int32 NumLODs = Component->GetNumLODs();
        LODResources.Empty();
        for (int32 LODIdx = 0; LODIdx < NumLODs; ++LODIdx)
        {
            FTerrainMeshLODResources* LODRes = new FTerrainMeshLODResources();

            const int32 NumSections = Component->GetNumSections(LODIdx);
            for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
            {
                // 실데이터에서 가져오는 부분은 직접 구현 필요
                FTerrainMeshSection* NewSection = new FTerrainMeshSection(GetScene().GetFeatureLevel());
                // TODO: VertexBuffers/IndexBuffer/Material 등 복사
                // 예시:
                // NewSection->Material = Component->GetMaterial(SectionIdx);
                // NewSection->VertexBuffers.InitFromDynamicVertex(...);
                // NewSection->IndexBuffer.Indices = ...;
                // BeginInitResource(&NewSection->VertexBuffers.PositionVertexBuffer);
                // BeginInitResource(&NewSection->VertexBuffers.StaticMeshVertexBuffer);
                // BeginInitResource(&NewSection->VertexBuffers.ColorVertexBuffer);
                // BeginInitResource(&NewSection->IndexBuffer);
                // BeginInitResource(&NewSection->VertexFactory);

#if RHI_RAYTRACING
                // RayTracingGeometry 초기화 필요시 구현
#endif

                LODRes->Sections.Add(NewSection);
            }
            LODResources.Add(LODRes);
        }
    }

    virtual ~FTerrainMeshSceneProxy() override
    {
        // LODResources는 TIndirectArray로 SectionArray의 Section은 SectionArray 소멸자에서 delete 됨
    }

    FTerrainMeshLODResourcesArray LODResources;

    virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily,
        uint32 VisibilityMap, class FMeshElementCollector& Collector) const override
    {
        // 어떤 LOD를 쓸지 결정
        int32 LODToRender = 0; // TODO: 실제 LOD 선택 로직 구현 필요

        if (!LODResources.IsValidIndex(LODToRender)) return;
        const FTerrainMeshLODResources& LODRes = LODResources[LODToRender];

        for (const FTerrainMeshSection* Section : LODRes.Sections)
        {
            if (!Section) continue;

            FMaterialRenderProxy* MaterialProxy = Section->Material ? Section->Material->GetRenderProxy() : nullptr;
            for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
            {
                if (VisibilityMap & (1 << ViewIndex))
                {
                    FMeshBatch& Mesh = Collector.AllocateMesh();
                    FMeshBatchElement& BatchElement = Mesh.Elements[0];

                    BatchElement.IndexBuffer = &Section->IndexBuffer;
                    Mesh.VertexFactory = &Section->VertexFactory;
                    Mesh.MaterialRenderProxy = MaterialProxy;
                    Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
                    Mesh.Type = PT_TriangleList;
                    Mesh.DepthPriorityGroup = SDPG_World;

                    BatchElement.FirstIndex = 0;
                    BatchElement.NumPrimitives = Section->IndexBuffer.GetNumIndices() / 3;
                    BatchElement.MinVertexIndex = 0;
                    BatchElement.MaxVertexIndex = Section->VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;

                    Collector.AddMesh(ViewIndex, Mesh);
                }
            }
        }
    }
};