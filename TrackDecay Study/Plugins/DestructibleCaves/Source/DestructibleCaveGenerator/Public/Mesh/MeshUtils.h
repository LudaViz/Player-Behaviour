// Copyright 2025 J2K2. All Rights Reserved.
#pragma once

#include "MeshData.h"
// #include "Mesher/TransvoxelTransition.h"
#include "Mesher/VoxelMesher.h"

/**
 * Util functions for cutting / concatenating meshes.
 * Currently only cares about vertex, index, normal, uv
 */
namespace DCGMeshUtils
{

void ConvertMeshShapeGeneratorToMeshData(const UE::Geometry::FMeshShapeGenerator* Generator,
										 FMeshData& OutMeshData)
{
	// 정점 변환
	const TArray<FVector3d>& InVertices = Generator->Vertices;
	OutMeshData.Vertices.Reset(InVertices.Num());
	for (const FVector3d& V : InVertices)
	{
		OutMeshData.Vertices.Add(FVector(V.X, V.Y, V.Z));
	}


	// 삼각형 인덱스 복사
	const TArray<UE::Geometry::FIndex3i>& InTriangles = Generator->Triangles;
	OutMeshData.Triangles.Reset(InTriangles.Num() * 3);
	for (const UE::Geometry::FIndex3i& Tri : InTriangles)
	{
		OutMeshData.Triangles.Add(Tri.A);
		OutMeshData.Triangles.Add(Tri.B);
		OutMeshData.Triangles.Add(Tri.C);
	}

#ifdef VOXEL_USE_GRADIENT_AS_NORMAL
	{
		const TArray<FVector3f>& InNormals = Generator->Normals;
		OutMeshData.Normals.Reset(InNormals.Num());
		for (const FVector3f& N : InNormals)
		{
			OutMeshData.Normals.Add(FVector(N.X, N.Y, N.Z));
		}
	}
#else
{
// 노멀은 직접 계산 (Generator.Normals는 사용 안 함)
OutMeshData.Normals.SetNumZeroed(OutMeshData.Vertices.Num());
for (int32 i = 0; i < OutMeshData.Triangles.Num(); i += 3)
{
	const int32 I0 = OutMeshData.Triangles[i];
	const int32 I1 = OutMeshData.Triangles[i + 1];
	const int32 I2 = OutMeshData.Triangles[i + 2];
		
	const FVector& V0 = OutMeshData.Vertices[I0];
	const FVector& V1 = OutMeshData.Vertices[I1];
	const FVector& V2 = OutMeshData.Vertices[I2];
	FVector Normal = FVector::CrossProduct(V2 - V0, V1 - V0).GetSafeNormal();
	
	OutMeshData.Normals[I0] += Normal;
	OutMeshData.Normals[I1] += Normal;
	OutMeshData.Normals[I2] += Normal;
}
for (FVector& N : OutMeshData.Normals)
{
	N.Normalize();
}
}
#endif

	const TArray<FVector2f>& InUVs = Generator->UVs;
	OutMeshData.UVs.Reset(InUVs.Num());
	for (const FVector2f& UV : InUVs)
	{
		OutMeshData.UVs.Add({UV.X, UV.Y});
	}
	OutMeshData.Tangents.Empty();
}

bool ContainsExclusive(const FVoxelMesher::FVoxelAlignedBox3d& Bound, const FVector3d& V)
{
	return (Bound.Min.X < V.X)
	&& (Bound.Min.Y < V.Y)
	&& (Bound.Min.Z < V.Z)
	&& (Bound.Max.X > V.X)
	&& (Bound.Max.Y > V.Y)
	&& (Bound.Max.Z > V.Z);
}
//
// void SeparateBorders(const FVoxelMesher* Generator, FChunkMeshSectionBorders& Regular)
// {
// 	const int Step = Generator->Step;
// 	FVoxelMesher::FVoxelAlignedBox3d Interior =
// 		FVoxelMesher::FVoxelAlignedBox3d(
// 			FTransvoxelRegular::FVoxelVector(Step),
// 			FTransvoxelRegular::FVoxelVector(CHUNK_SIZE - Step));
//
// 	for (int i = 0; i < std::size(Regular.Borders); i++)
// 	{
// 		FTransvoxelRegular::EBorderPosition BorderPosition = static_cast<FTransvoxelRegular::EBorderPosition>(i);
// 		FTransvoxelRegular::FVoxelVector TransitionMin, TransitionMax;
//
// 		switch (BorderPosition)
// 		{
// 		case 0: // XMinus (x == 0 ~ Step)
// 			TransitionMin = FTransvoxelRegular::FVoxelVector(0, 0, 0);
// 			TransitionMax = FTransvoxelRegular::FVoxelVector(Step, CHUNK_SIZE, CHUNK_SIZE);
// 			break;
// 		case 1: // YMinus (y == 0 ~ Step)
// 			TransitionMin = FTransvoxelRegular::FVoxelVector(0, 0, 0);
// 			TransitionMax = FTransvoxelRegular::FVoxelVector(CHUNK_SIZE, Step, CHUNK_SIZE);
// 			break;
// 		case 2: // ZMinus (z == 0 ~ Step)
// 			TransitionMin = FTransvoxelRegular::FVoxelVector(0, 0, 0);
// 			TransitionMax = FTransvoxelRegular::FVoxelVector(CHUNK_SIZE, CHUNK_SIZE, Step);
// 			break;
// 		case 3: // XPlus (x == CHUNK_SIZE - Step ~ CHUNK_SIZE)
// 			TransitionMin = FTransvoxelRegular::FVoxelVector(CHUNK_SIZE - Step, 0, 0);
// 			TransitionMax = FTransvoxelRegular::FVoxelVector(CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE);
// 			break;
// 		case 4: // YPlus (y == CHUNK_SIZE - Step ~ CHUNK_SIZE)
// 			TransitionMin = FTransvoxelRegular::FVoxelVector(0, CHUNK_SIZE - Step, 0);
// 			TransitionMax = FTransvoxelRegular::FVoxelVector(CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE);
// 			break;
// 		case 5: // ZPlus (z == CHUNK_SIZE - Step ~ CHUNK_SIZE)
// 			TransitionMin = FTransvoxelRegular::FVoxelVector(0, 0, CHUNK_SIZE - Step);
// 			TransitionMax = FTransvoxelRegular::FVoxelVector(CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE);
// 			break;
// 		}
// 		FVoxelMesher::FVoxelAlignedBox3d Bounds = FVoxelMesher::FVoxelAlignedBox3d(TransitionMin, TransitionMax);
// 	
// 		FMeshData MeshData;
// 		TMap<int32, int32> VertexMap; // 원본 인덱스 → 새 인덱스
// 		// 정점 변환
// 		const TArray<FVector3d>& InVertices = Generator->Vertices;
// 		const TArray<FVector3f>& InNormals = Generator->Normals;
// 		const TArray<FVector2f>& InUVs = Generator->UVs;
//
// 		MeshData.Vertices.Reset(InVertices.Num());
// 		MeshData.Normals.Reset(InNormals.Num());
// 		MeshData.UVs.Reset(InUVs.Num());
// 		for (int32 vi = 0; vi < InVertices.Num(); ++vi)
// 		{
// 			const FVector3d& V = InVertices[vi];
// 			if (!ContainsExclusive(Interior,V) && !ContainsExclusive(Bounds, V))
// 			{
// 				int32 NewIndex = MeshData.Vertices.Add(FVector(V.X, V.Y, V.Z));
// 				VertexMap.Add(vi, NewIndex);
//
// 				const FVector3f& Normal = InNormals[vi];
// 				MeshData.Normals.Add(FVector(Normal));
//
// 				const FVector2f& UV = InUVs[vi];
// 				MeshData.UVs.Add(FVector2D(UV.X, UV.Y));
// 			}
// 		}
//
// 		// 삼각형 변환 (경계면에 있는 버텍스만 포함하는 삼각형만 추가)
// 		for (const UE::Geometry::FIndex3i& Tri : Generator->Triangles)
// 		{
// 			int32 Orig0 = Tri.A;
// 			int32 Orig1 = Tri.B;
// 			int32 Orig2 = Tri.C;
//
// 			int32* New0 = VertexMap.Find(Orig0);
// 			int32* New1 = VertexMap.Find(Orig1);
// 			int32* New2 = VertexMap.Find(Orig2);
//
// 			// 세 꼭짓점 모두 경계면에 포함되어 있으면 삼각형 추가
// 			if (New0 && New1 && New2)
// 			{
// 				MeshData.Triangles.Add(*New0);
// 				MeshData.Triangles.Add(*New1);
// 				MeshData.Triangles.Add(*New2);
// 			}
// 		}
//
// 		Regular.Borders[i] = MeshData;
// 	}
// }
//
// void SeparateInterior(const FVoxelMesher* Generator, FMeshData& Interior)
// {
// 	const int Step = Generator->Step;
// 	FVoxelMesher::FVoxelAlignedBox3d Bounds =
// 		FVoxelMesher::FVoxelAlignedBox3d(
// 			FTransvoxelRegular::FVoxelVector(Step),
// 			FTransvoxelRegular::FVoxelVector(CHUNK_SIZE - Step));
//
// 	TMap<int32, int32> VertexMap; // 원본 인덱스 → 새 인덱스
// 	// 정점 변환
// 	const TArray<FVector3d>& InVertices = Generator->Vertices;
// 	const TArray<FVector3f>& InNormals = Generator->Normals;
// 	const TArray<FVector2f>& InUVs = Generator->UVs;
//
// 	Interior.Vertices.Reset(InVertices.Num());
// 	Interior.Normals.Reset(InNormals.Num());
// 	Interior.UVs.Reset(InUVs.Num());
// 	for (int32 vi = 0; vi < InVertices.Num(); ++vi)
// 	{
// 		const FVector3d& V = InVertices[vi];
// 		if (Bounds.Contains(V))
// 		{
// 			int32 NewIndex = Interior.Vertices.Add(FVector(V.X, V.Y, V.Z));
// 			VertexMap.Add(vi, NewIndex);
//
// 			const FVector3f& Normal = InNormals[vi];
// 			Interior.Normals.Add(FVector(Normal));
//
// 			const FVector2f& UV = InUVs[vi];
// 			Interior.UVs.Add(FVector2D(UV.X, UV.Y));
// 		}
// 	}
//
// 	// 삼각형 변환 (경계면에 있는 버텍스만 포함하는 삼각형만 추가)
// 	for (const UE::Geometry::FIndex3i& Tri : Generator->Triangles)
// 	{
// 		int32 Orig0 = Tri.A;
// 		int32 Orig1 = Tri.B;
// 		int32 Orig2 = Tri.C;
//
// 		int32* New0 = VertexMap.Find(Orig0);
// 		int32* New1 = VertexMap.Find(Orig1);
// 		int32* New2 = VertexMap.Find(Orig2);
//
// 		// 세 꼭짓점 모두 경계면에 포함되어 있으면 삼각형 추가
// 		if (New0 && New1 && New2)
// 		{
// 			Interior.Triangles.Add(*New0);
// 			Interior.Triangles.Add(*New1);
// 			Interior.Triangles.Add(*New2);
// 		}
// 	}
// }
//
//
// void SeparateBordersFull(const FVoxelMesher* Generator, FMeshData& Interior)
// {
// 	const int Step = Generator->Step;
// 	FVoxelMesher::FVoxelAlignedBox3d Bounds =
// 		FVoxelMesher::FVoxelAlignedBox3d(
// 			FTransvoxelRegular::FVoxelVector(Step),
// 			FTransvoxelRegular::FVoxelVector(CHUNK_SIZE - Step));
//
// 	TMap<int32, int32> VertexMap; // 원본 인덱스 → 새 인덱스
// 	// 정점 변환
// 	const TArray<FVector3d>& InVertices = Generator->Vertices;
// 	const TArray<FVector3f>& InNormals = Generator->Normals;
// 	const TArray<FVector2f>& InUVs = Generator->UVs;
//
// 	Interior.Vertices.Reset(InVertices.Num());
// 	Interior.Normals.Reset(InNormals.Num());
// 	Interior.UVs.Reset(InUVs.Num());
// 	for (int32 vi = 0; vi < InVertices.Num(); ++vi)
// 	{
// 		const FVector3d& V = InVertices[vi];
// 		if (!ContainsExclusive(Bounds, V))
// 		{
// 			int32 NewIndex = Interior.Vertices.Add(FVector(V.X, V.Y, V.Z));
// 			VertexMap.Add(vi, NewIndex);
//
// 			const FVector3f& Normal = InNormals[vi];
// 			Interior.Normals.Add(FVector(Normal));
//
// 			const FVector2f& UV = InUVs[vi];
// 			Interior.UVs.Add(FVector2D(UV.X, UV.Y));
// 		}
// 	}
//
// 	// 삼각형 변환 (경계면에 있는 버텍스만 포함하는 삼각형만 추가)
// 	for (const UE::Geometry::FIndex3i& Tri : Generator->Triangles)
// 	{
// 		int32 Orig0 = Tri.A;
// 		int32 Orig1 = Tri.B;
// 		int32 Orig2 = Tri.C;
//
// 		int32* New0 = VertexMap.Find(Orig0);
// 		int32* New1 = VertexMap.Find(Orig1);
// 		int32* New2 = VertexMap.Find(Orig2);
//
// 		// 세 꼭짓점 모두 경계면에 포함되어 있으면 삼각형 추가
// 		if (New0 && New1 && New2)
// 		{
// 			Interior.Triangles.Add(*New0);
// 			Interior.Triangles.Add(*New1);
// 			Interior.Triangles.Add(*New2);
// 		}
// 	}
// }
//
// void ConcatenateMeshes(const FMeshData& InMesh1, const FMeshData& InMesh2, FMeshData& OutMesh)
// {
// 	OutMesh = InMesh1;
//
// 	int VertexOffset = OutMesh.Vertices.Num();
//
// 	for (int32 vi = 0; vi < InMesh2.Vertices.Num(); ++vi)
// 	{
// 		const FVector& V = InMesh2.Vertices[vi];
// 		OutMesh.Vertices.Add(V);
//
// 		const FVector& Normal = InMesh2.Normals[vi];
// 		OutMesh.Normals.Add(Normal);
//
// 		const FVector2D& UV = InMesh2.UVs[vi];
// 		OutMesh.UVs.Add(UV);
// 	}
//
// 	for (int32 ii = 0; ii < InMesh2.Triangles.Num(); ++ii)
// 	{
// 		int32 NewIndex = InMesh2.Triangles[ii] + VertexOffset;
// 		OutMesh.Triangles.Add(NewIndex);
// 	}
// }
};