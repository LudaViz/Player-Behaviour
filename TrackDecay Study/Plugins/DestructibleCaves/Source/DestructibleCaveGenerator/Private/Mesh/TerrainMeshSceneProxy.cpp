// Copyright 2025 J2K2. All Rights Reserved.

// 작업중

// // Fill out your copyright notice in the Description page of Project Settings.
//
//
// #include "Mesh/TerrainMeshSceneProxy.h"
//
// #include "DynamicMeshBuilder.h"
// #include "Chaos/Deformable/MuscleActivationConstraints.h"
//
// //
// // FTerrainMeshSceneProxy::~FTerrainMeshSceneProxy()
// // {
// // }
// //
// // void FTerrainMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views,
// // 	const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const
// // {
// // 	// FPrimitiveSceneProxy::GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);
// // 	//
// // 	// for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
// // 	// {
// // 	// 	if (VisibilityMap & (1 << ViewIndex))
// // 	// 	{
// // 	// 		// 1. DynamicMeshBuilder로 메시 구성
// // 	// 		FDynamicMeshBuilder MeshBuilder(Views[ViewIndex]->GetFeatureLevel());
// // 	//
// // 	// 		for (const FVector& V : MeshData.Vertices)
// // 	// 			MeshBuilder.AddVertex(FDynamicMeshVertex(V));
// // 	//
// // 	// 		for (int32 i = 0; i < Indices.Num(); i += 3)
// // 	// 			MeshBuilder.AddTriangle(Indices[i], Indices[i+1], Indices[i+2]);
// // 	//
// // 	// 		// 2. 머티리얼 할당 (없으면 기본 머티리얼)
// // 	// 		UMaterialInterface* UseMaterial = Material ? Material : UMaterial::GetDefaultMaterial(MD_Surface);
// // 	//
// // 	// 		// 3. Collector에 메시 등록
// // 	// 		MeshBuilder.GetMesh(FMatrix::Identity, UseMaterial, /*bWireframe*/ false, /*bUseSelectionOutline*/ false, 
// // 	// 							Views[ViewIndex]->GetFeatureLevel(), Collector, GetDepthPriorityGroup());
// // 	// 	}
// // 	// }
// // }
