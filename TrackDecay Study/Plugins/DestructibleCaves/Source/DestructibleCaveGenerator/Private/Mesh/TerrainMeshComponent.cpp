// Copyright 2025 J2K2. All Rights Reserved.

// 작업중

// // Fill out your copyright notice in the Description page of Project Settings.
//
//
// #include "Mesh/TerrainMeshComponent.h"
//
// #include "Mesh/TerrainMeshSceneProxy.h"
// #include "PhysicsEngine/BodySetup.h"
//
// #include UE_INLINE_GENERATED_CPP_BY_NAME(TerrainMeshComponent) // ???
//
// UTerrainMeshComponent::UTerrainMeshComponent(const FObjectInitializer& ObjectInitializer)
// 	: Super(ObjectInitializer)
// {
// 	// 기본값 설정
// 	// SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
// 	// bUseAsOccluder = false;
// 	PrimaryComponentTick.bCanEverTick = false;
// }
//
// void UTerrainMeshComponent::SetMeshData(int32 LODLevel, const FMultiMeshData& InMeshData)
// {
// 	LODMeshCache[LODLevel] = InMeshData;
// }
//
// void UTerrainMeshComponent::CreateMeshForLOD(int32 LODLevel)
// {
// 	// Reset this section (in case it already existed)
// 	FTerrainMeshSection& NewSection = TerrainMeshSections[SectionIndex];
// 	NewSection.Reset();
//
// 	// Copy data to vertex buffer
// 	const int32 NumVerts = Vertices.Num();
// 	NewSection.ProcVertexBuffer.Reset();
// 	NewSection.ProcVertexBuffer.AddUninitialized(NumVerts);
// 	for (int32 VertIdx = 0; VertIdx < NumVerts; VertIdx++)
// 	{
// 		FProcMeshVertex& Vertex = NewSection.ProcVertexBuffer[VertIdx];
//
// 		Vertex.Position = Vertices[VertIdx];
// 		Vertex.Normal = (Normals.Num() == NumVerts) ? Normals[VertIdx] : FVector(0.f, 0.f, 1.f);
// 		Vertex.UV0 = (UV0.Num() == NumVerts) ? UV0[VertIdx] : FVector2D(0.f, 0.f);
// 		Vertex.UV1 = (UV1.Num() == NumVerts) ? UV1[VertIdx] : FVector2D(0.f, 0.f);
// 		Vertex.UV2 = (UV2.Num() == NumVerts) ? UV2[VertIdx] : FVector2D(0.f, 0.f);
// 		Vertex.UV3 = (UV3.Num() == NumVerts) ? UV3[VertIdx] : FVector2D(0.f, 0.f);
// 		Vertex.Color = (VertexColors.Num() == NumVerts) ? VertexColors[VertIdx] : FColor(255, 255, 255);
// 		Vertex.Tangent = (Tangents.Num() == NumVerts) ? Tangents[VertIdx] : FProcMeshTangent();
//
// 		// Update bounding box
// 		NewSection.SectionLocalBox += Vertex.Position;
// 	}
//
// 	// Get triangle indices, clamping to vertex range
// 	const int32 MaxIndex = NumVerts - 1;
// 	const auto GetTriIndices = [&Triangles, MaxIndex](int32 Idx)
// 	{
// 		return TTuple<int32, int32, int32>(FMath::Min(Triangles[Idx    ], MaxIndex),
// 										   FMath::Min(Triangles[Idx + 1], MaxIndex),
// 			                               FMath::Min(Triangles[Idx + 2], MaxIndex));
// 	};
//
// 	const int32 NumTriIndices = (Triangles.Num() / 3) * 3; // Ensure number of triangle indices is multiple of three
//
// 	// Detect degenerate triangles, i.e. non-unique vertex indices within the same triangle
// 	int32 NumDegenerateTriangles = 0;
// 	for (int32 IndexIdx = 0; IndexIdx < NumTriIndices; IndexIdx += 3)
// 	{
// 		int32 a, b, c;
// 		Tie(a, b, c) = GetTriIndices(IndexIdx);
// 		NumDegenerateTriangles += a == b || a == c || b == c; //-V614
// 	}
// 	if (NumDegenerateTriangles > 0)
// 	{
// 		UE_LOG(LogProceduralComponent, Warning, TEXT("Detected %d degenerate triangle%s with non-unique vertex indices for created mesh section in '%s'; degenerate triangles will be dropped."),
// 			   NumDegenerateTriangles, NumDegenerateTriangles > 1 ? TEXT("s") : TEXT(""), *GetFullName());
// 	}
//
// 	// Copy index buffer for non-degenerate triangles
// 	NewSection.ProcIndexBuffer.Reset();
// 	NewSection.ProcIndexBuffer.AddUninitialized(NumTriIndices - NumDegenerateTriangles * 3);
// 	int32 CopyIndexIdx = 0;
// 	for (int32 IndexIdx = 0; IndexIdx < NumTriIndices; IndexIdx += 3)
// 	{
// 		int32 a, b, c;
// 		Tie(a, b, c) = GetTriIndices(IndexIdx);
//
// 		if (a != b && a != c && b != c) //-V614
// 		{
// 			NewSection.ProcIndexBuffer[CopyIndexIdx++] = a;
// 			NewSection.ProcIndexBuffer[CopyIndexIdx++] = b;
// 			NewSection.ProcIndexBuffer[CopyIndexIdx++] = c;
// 		}
// 		else
// 		{
// 			--NumDegenerateTriangles;
// 		}
// 	}
// 	check(NumDegenerateTriangles == 0);
// 	check(CopyIndexIdx == NewSection.ProcIndexBuffer.Num());
//
// 	NewSection.bEnableCollision = bCreateCollision;
//
// 	UpdateLocalBounds(); // Update overall bounds
// 	UpdateCollision(); // Mark collision as dirty
// 	MarkRenderStateDirty(); // New section requires recreating scene proxy
// }
//
// bool UTerrainMeshComponent::GetTriMeshSizeEstimates(struct FTriMeshCollisionDataEstimates& OutTriMeshEstimates, bool bInUseAllTriData) const
// {
// 	for (const FProcMeshSection& Section : ProcMeshSections)
// 	{
// 		if (Section.bEnableCollision)
// 		{
// 			OutTriMeshEstimates.VerticeCount += Section.ProcVertexBuffer.Num();
// 		}
// 	}
//
// 	return true;
// }
//
// bool UTerrainMeshComponent::GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData)
// {
// 	int32 VertexBase = 0; // Base vertex index for current section
//
// 	// See if we should copy UVs
// 	bool bCopyUVs = UPhysicsSettings::Get()->bSupportUVFromHitResults; 
// 	if (bCopyUVs)
// 	{
// 		CollisionData->UVs.AddZeroed(1); // only one UV channel
// 	}
//
// 	// For each section..
// 	for (int32 SectionIdx = 0; SectionIdx < ProcMeshSections.Num(); SectionIdx++)
// 	{
// 		FProcMeshSection& Section = ProcMeshSections[SectionIdx];
// 		// Do we have collision enabled?
// 		if (Section.bEnableCollision)
// 		{
// 			// Copy vert data
// 			for (int32 VertIdx = 0; VertIdx < Section.ProcVertexBuffer.Num(); VertIdx++)
// 			{
// 				CollisionData->Vertices.Add((FVector3f)Section.ProcVertexBuffer[VertIdx].Position);
//
// 				// Copy UV if desired
// 				if (bCopyUVs)
// 				{
// 					CollisionData->UVs[0].Add(Section.ProcVertexBuffer[VertIdx].UV0);
// 				}
// 			}
//
// 			// Copy triangle data
// 			const int32 NumTriangles = Section.ProcIndexBuffer.Num() / 3;
// 			for (int32 TriIdx = 0; TriIdx < NumTriangles; TriIdx++)
// 			{
// 				// Need to add base offset for indices
// 				FTriIndices Triangle;
// 				Triangle.v0 = Section.ProcIndexBuffer[(TriIdx * 3) + 0] + VertexBase;
// 				Triangle.v1 = Section.ProcIndexBuffer[(TriIdx * 3) + 1] + VertexBase;
// 				Triangle.v2 = Section.ProcIndexBuffer[(TriIdx * 3) + 2] + VertexBase;
// 				CollisionData->Indices.Add(Triangle);
//
// 				// Also store material info
// 				CollisionData->MaterialIndices.Add(SectionIdx);
// 			}
//
// 			// Remember the base index that new verts will be added from in next section
// 			VertexBase = CollisionData->Vertices.Num();
// 		}
// 	}
//
// 	CollisionData->bFlipNormals = true;
// 	CollisionData->bDeformableMesh = true;
// 	CollisionData->bFastCook = true;
//
// 	return true;
// }
//
// bool UTerrainMeshComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
// {
// 	for (const FProcMeshSection& Section : ProcMeshSections)
// 	{
// 		if (Section.ProcIndexBuffer.Num() >= 3 && Section.bEnableCollision)
// 		{
// 			return true;
// 		}
// 	}
//
// 	return false;
// }
//
// void UTerrainMeshComponent::UpdateLocalBounds()
// {
// 	FBox LocalBox(ForceInit);
//
// 	for (const FProcMeshSection& Section : ProcMeshSections)
// 	{
// 		LocalBox += Section.SectionLocalBox;
// 	}
//
// 	LocalBounds = LocalBox.IsValid ? FBoxSphereBounds(LocalBox) : FBoxSphereBounds(FVector::ZeroVector, FVector::ZeroVector, 0); // fallback to reset box sphere bounds
//
// 	// Update global bounds
// 	UpdateBounds();
// 	// Need to send to render thread
// 	MarkRenderTransformDirty();
// }
//
// void UTerrainMeshComponent::CreateProcMeshBodySetup()
// {
// 	if (ProcMeshBodySetup == nullptr)
// 	{
// 		ProcMeshBodySetup = CreateBodySetupHelper();
// 	}
// }
//
// void UTerrainMeshComponent::UpdateCollision()
// {
// 	UWorld* World = GetWorld();
// 	const bool bUseAsyncCook = World && World->IsGameWorld() && bUseAsyncCooking;
//
// 	if(bUseAsyncCook)
// 	{
// 		// Abort all previous ones still standing
// 		for (UBodySetup* OldBody : AsyncBodySetupQueue)
// 		{
// 			OldBody->AbortPhysicsMeshAsyncCreation();
// 		}
//
// 		AsyncBodySetupQueue.Add(CreateBodySetupHelper());
// 	}
// 	else
// 	{
// 		AsyncBodySetupQueue.Empty();	//If for some reason we modified the async at runtime, just clear any pending async body setups
// 		CreateProcMeshBodySetup();
// 	}
// 	
// 	UBodySetup* UseBodySetup = bUseAsyncCook ? AsyncBodySetupQueue.Last() : ProcMeshBodySetup;
//
// 	// Fill in simple collision convex elements
// 	UseBodySetup->AggGeom.ConvexElems = CollisionConvexElems;
//
// 	// Set trace flag
// 	UseBodySetup->CollisionTraceFlag = bUseComplexAsSimpleCollision ? CTF_UseComplexAsSimple : CTF_UseDefault;
//
// 	if(bUseAsyncCook)
// 	{
// 		UseBodySetup->CreatePhysicsMeshesAsync(FOnAsyncPhysicsCookFinished::CreateUObject(this, &UTerrainMeshComponent::FinishPhysicsAsyncCook, UseBodySetup));
// 	}
// 	else
// 	{
// 		// New GUID as collision has changed
// 		UseBodySetup->BodySetupGuid = FGuid::NewGuid();
// 		// Also we want cooked data for this
// 		UseBodySetup->bHasCookedCollisionData = true;
// 		UseBodySetup->InvalidatePhysicsData();
// 		UseBodySetup->CreatePhysicsMeshes();
// 		RecreatePhysicsState();
// 	}
// }
//
// void UTerrainMeshComponent::FinishPhysicsAsyncCook(bool bSuccess, UBodySetup* FinishedBodySetup)
// {
// 	TArray<UBodySetup*> NewQueue;
// 	NewQueue.Reserve(AsyncBodySetupQueue.Num());
//
// 	int32 FoundIdx;
// 	if(AsyncBodySetupQueue.Find(FinishedBodySetup, FoundIdx))
// 	{
// 		if (bSuccess)
// 		{
// 			//The new body was found in the array meaning it's newer so use it
// 			ProcMeshBodySetup = FinishedBodySetup;
// 			RecreatePhysicsState();
//
// 			//remove any async body setups that were requested before this one
// 			for (int32 AsyncIdx = FoundIdx + 1; AsyncIdx < AsyncBodySetupQueue.Num(); ++AsyncIdx)
// 			{
// 				NewQueue.Add(AsyncBodySetupQueue[AsyncIdx]);
// 			}
//
// 			AsyncBodySetupQueue = NewQueue;
// 		}
// 		else
// 		{
// 			AsyncBodySetupQueue.RemoveAt(FoundIdx);
// 		}
// 	}
// }
//
// UBodySetup* UTerrainMeshComponent::CreateBodySetupHelper()
// {
// 	// The body setup in a template needs to be public since the property is Tnstanced and thus is the archetype of the instance meaning there is a direct reference
// 	UBodySetup* NewBodySetup = NewObject<UBodySetup>(this, NAME_None, (IsTemplate() ? RF_Public | RF_ArchetypeObject : RF_NoFlags));
// 	NewBodySetup->BodySetupGuid = FGuid::NewGuid();
//
// 	NewBodySetup->bGenerateMirroredCollision = false;
// 	NewBodySetup->bDoubleSidedGeometry = true;
// 	NewBodySetup->CollisionTraceFlag = bUseComplexAsSimpleCollision ? CTF_UseComplexAsSimple : CTF_UseDefault;
//
// 	return NewBodySetup;
// }
//
//
// FPrimitiveSceneProxy* UTerrainMeshComponent::CreateSceneProxy()
// {
// 	return Super::CreateSceneProxy();
// }
//
// class UBodySetup* UTerrainMeshComponent::GetBodySetup()
// {
// 	CreateProcMeshBodySetup();
// 	return ProcMeshBodySetup;
// }
//
// UMaterialInterface* UTerrainMeshComponent::GetMaterialFromCollisionFaceIndex(int32 FaceIndex, int32& SectionIndex) const
// {
// 	UMaterialInterface* Result = nullptr;
// 	SectionIndex = 0;
//
// 	if (FaceIndex >= 0)
// 	{
// 		// Look for element that corresponds to the supplied face
// 		int32 TotalFaceCount = 0;
// 		for (int32 SectionIdx = 0; SectionIdx < ProcMeshSections.Num(); SectionIdx++)
// 		{
// 			const FProcMeshSection& Section = ProcMeshSections[SectionIdx];
// 			int32 NumFaces = Section.ProcIndexBuffer.Num() / 3;
// 			TotalFaceCount += NumFaces;
//
// 			if (FaceIndex < TotalFaceCount)
// 			{
// 				// Grab the material
// 				Result = GetMaterial(SectionIdx);
// 				SectionIndex = SectionIdx;
// 				break;
// 			}
// 		}
// 	}
//
// 	return Result;
// }
//
// int32 UTerrainMeshComponent::GetNumMaterials() const
// {
// 	/**
// 	 * UProceduralMeshComponent에서는 Section당 하나로 했지만, 여기서는 일단 하나만 사용,,,,,
// 	 */
// 	// return ProcMeshSections.Num();
// 	return 1;
// }
//
// void UTerrainMeshComponent::PostLoad()
// {
// 	Super::PostLoad();
// }
//
// FBoxSphereBounds UTerrainMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
// {
// 	return Super::CalcBounds(LocalToWorld);
// }
