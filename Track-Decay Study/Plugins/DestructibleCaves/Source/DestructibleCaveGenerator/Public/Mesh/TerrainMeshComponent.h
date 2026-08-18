// Copyright 2025 J2K2. All Rights Reserved.

// 작업중

// // Fill out your copyright notice in the Description page of Project Settings.
//
// #pragma once
//
// #include "CoreMinimal.h"
// #include "MarchingCubes.h"
// #include "TerrainMeshSceneProxy.h"
// #include "TerrainMeshComponent.generated.h"
//
// /**
//  * UProceduralMeshComponent와 동일하지만 Game Thread의 의존성을 줄인 클래스
//  */
// UCLASS(hidecategories = (Object, LOD), meta = (BlueprintSpawnableComponent), ClassGroup = Rendering)
// class DESTRUCTIBLECAVEGENERATOR_API UTerrainMeshComponent : public UMeshComponent, public IInterface_CollisionDataProvider
// {
// 	GENERATED_BODY()
//
// public:
// 	UTerrainMeshComponent(const FObjectInitializer& ObjectInitializer);	
// 	
// 	void SetMeshData(int32 LODLevel, const FMultiMeshData& InMeshData);
//
// 	/**
// 	 * 내부적으로 UProceduralMeshComponent::CreateMeshSection 코드를 실행합니다.
// 	 * @param LODLevel 
// 	 */
// 	void CreateMeshForLOD(int32 LODLevel);
//
// 	/**
// 	 * 내부적으로 UProceduralMeshComponent::CreateMeshSection 코드를 실행합니다.
// 	 * TODO : 일부만 수정하도록 하기
// 	 */
// 	void UpdateMeshData(int32 LODLevel, const FMultiMeshData& InMeshData);
//
// 	/**
// 	 * 기존의 FMeshData를 제거
// 	 */
// 	void ClearMeshData(int32 LODLevel);
//
// 	/**
// 	 * 모든 FMeshData를 제거
// 	 */
// 	void ClearAllMeshData();
// 	
// 	int GetNumLODs() const { return LODMeshCache.Num(); }
// 	int GetNumSections(int32 LODLevel) const
// 	{
// 		return LODMeshCache[LODLevel].MaterialMeshMap.Num();
// 	}
//
// private:
// 	// LOD별 메시 데이터 캐시
// 	UPROPERTY(VisibleAnywhere)
// 	TMap<int32, FMultiMeshData> LODMeshCache;
//
// // Collision
// public:
// 	//~ Begin Interface_CollisionDataProvider Interface
// 	virtual bool GetTriMeshSizeEstimates(struct FTriMeshCollisionDataEstimates& OutTriMeshEstimates, bool bInUseAllTriData) const override;
// 	virtual bool GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;
// 	virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override;
// 	virtual bool WantsNegXTriMesh() override{ return false; }
// 	//~ End Interface_CollisionDataProvider Interface
//
// 	/** 
// 	 *	Controls whether the complex (Per poly) geometry should be treated as 'simple' collision. 
// 	 *	Should be set to false if this component is going to be given simple collision and simulated.
// 	 */
// 	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Procedural Mesh")
// 	bool bUseComplexAsSimpleCollision;
// 	
// 	/**
// 	*	Controls whether the physics cooking should be done off the game thread. This should be used when collision geometry doesn't have to be immediately up to date (For example streaming in far away objects)
// 	*/
// 	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Procedural Mesh")
// 	bool bUseAsyncCooking;
//
// 	/** Collision data */
// 	UPROPERTY(Instanced)
// 	TObjectPtr<class UBodySetup> ProcMeshBodySetup;
// 	
// private:
// 	/** Update LocalBounds member from the local box of each section */
// 	void UpdateLocalBounds();
// 	/** Ensure ProcMeshBodySetup is allocated and configured */
// 	void CreateProcMeshBodySetup();
// 	/** Mark collision data as dirty, and re-create on instance if necessary */
// 	void UpdateCollision();
// 	/** Once async physics cook is done, create needed state */
// 	void FinishPhysicsAsyncCook(bool bSuccess, UBodySetup* FinishedBodySetup);
//
// 	/** Helper to create new body setup objects */
// 	UBodySetup* CreateBodySetupHelper();
// 	
// 	/** Array of sections of mesh */
// 	TArray<FTerrainMeshSection> TerrainMeshSections;
//
// 	/** Convex shapes used for simple collision */
// 	UPROPERTY()
// 	TArray<FKConvexElem> CollisionConvexElems;
//
// 	/** Local space bounds of mesh */
// 	UPROPERTY()
// 	FBoxSphereBounds LocalBounds;
// 		
// 	/** Queue for async body setups that are being cooked */
// 	UPROPERTY(transient)
// 	TArray<TObjectPtr<UBodySetup>> AsyncBodySetupQueue;
//
// // overrides
// public:
// 	//~ Begin UPrimitiveComponent Interface.
// 	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
// 	virtual class UBodySetup* GetBodySetup() override;
// 	virtual UMaterialInterface* GetMaterialFromCollisionFaceIndex(int32 FaceIndex, int32& SectionIndex) const override;
// 	//~ End UPrimitiveComponent Interface.
//
// 	//~ Begin UMeshComponent Interface.
// 	virtual int32 GetNumMaterials() const override;
// 	//~ End UMeshComponent Interface.
//
// 	//~ Begin UObject Interface
// 	virtual void PostLoad() override;
// 	//~ End UObject Interface.
//
// 	//~ Begin USceneComponent Interface.
// 	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
// 	//~ Begin USceneComponent Interface.
// };
