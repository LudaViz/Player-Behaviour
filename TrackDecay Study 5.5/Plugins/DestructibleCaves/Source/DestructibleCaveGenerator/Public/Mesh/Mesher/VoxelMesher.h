// Copyright Epic Games, Inc. All Rights Reserved.
// Modified from UE original source code

#pragma once

#include "CoreMinimal.h"
#include "BoxTypes.h"
#include "IntBoxTypes.h"
#include "Chunk/ChunkData.h" // 언젠가 빼야함. CHUNK_MARGIN 매크로를 template로 받게 하기
#include "Generators/MeshShapeGenerator.h"
#include "Spatial/BlockedDenseGrid3.h"
#include "Util/IndexUtil.h"
#include "Async/ParallelFor.h"

#define VOXEL_USE_GRADIENT_AS_NORMAL
/**
 * UE의 FMarchingCubes를 그대로 가져와서 voxel에 맞게 재구성
 * normal을 계산하기 위해서 padding을 추가 가능
 * TODO : Vertex 저장하는 Hashing 제거하기 (chunk가 충분히 작기 때문에 무시 가능)
 */
struct FVoxelMesher : public UE::Geometry::FMeshShapeGenerator
{
public:
	using Real = double;
	using FVoxelAlignedBox3d = UE::Geometry::TAxisAlignedBox3<double>;
	using FVoxelAxisAlignedBox3i = UE::Geometry::FAxisAlignedBox3i;
	using FVoxelVector3i = UE::Geometry::FVector3i;
	using FVoxelBlockedDenseGrid3i = UE::Geometry::FBlockedDenseGrid3i;
	using FVoxelBlockedDenseGrid3f = UE::Geometry::FBlockedDenseGrid3f;
	using FVoxelVector = UE::Math::TVector<Real>;
	using FVoxelVector3f = UE::Math::TVector<float>;
	using FVoxelIndex3i = UE::Geometry::FIndex3i;
	/**
	*  this is the function we will evaluate
	*/
	TFunction<Real(FVoxelVector3i)> Implicit;

#ifdef VOXEL_USE_GRADIENT_AS_NORMAL
	// Gradient for normal
	TFunction<FVoxelVector3f(FVoxelVector3i)> ImplicitGradient = nullptr;
#endif
	/**
	*  mesh surface will be at this isovalue. Normally 0 unless you want
	*  offset surface or field is not a distance-field.
	*/
	double IsoValue = 0;

	/** bounding-box we will mesh inside of. We use the min-corner and
	 *  the width/height/depth, but do not clamp vertices to stay within max-corner,
	 *  we may spill one cell over
	 *  if {(0,0,0), (16,16,16)}, the vertices are within 0 <= xyz <= 16
	 *  Vertices for normal, which is only used internally, can be over Bounds
	*/
	FVoxelAxisAlignedBox3i Bounds;

	/**
	 *  Length of edges of cubes that are marching.
	 *  currently, # of cells along axis = (int)(bounds_dimension / CellSize) + 1
	 */
	int Step = 1;

	/**
	 *  Use multi-threading? Generally a good idea unless problem is very small or
	 *  you are multi-threading at a higher level (which may be more efficient)
	 */
	bool bParallelCompute = false;
	
	/** if this function returns true, we should abort calculation */
	TFunction<bool(void)> CancelF = []() { return false; };

	/** Calculate extra one voxel from each interval end */
	static constexpr bool bExtraVoxel =
#ifdef VOXEL_USE_GRADIENT_AS_NORMAL
		false;
#else
		(CHUNK_MARGIN != 0);
#endif
	/*
	 * Outputs
	 */

	// [CellMinDimensions, CellMaxDimensions) 범위의 triangle까지만 사용
	FVoxelVector3i CellMinDimensions;
	FVoxelVector3i CellMaxDimensions;


	FVoxelMesher()
	{
		Bounds = FVoxelAxisAlignedBox3i(FVoxelVector3i::Zero(), FVoxelVector3i(16,16,16));
		Step = 1;
	}

	virtual ~FVoxelMesher()
	{
	}

	bool Validate()
	{
		return Step > 0;
	}

	/**
	*  Run MC algorithm and generate Output mesh
	*/
	virtual FMeshShapeGenerator& Generate() override
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Geometry_MCMesh_Generate);

		if (!ensure(Validate()))
		{
			return *this;
		}

		SetDimensions();

		InitHashTables();
		ResetMesh();

		if (bParallelCompute) 
		{
			generate_parallel();
		} 
		else 
		{
			generate_basic();
		}

		// finalize mesh
		BuildMesh();

		return *this;
	}

protected:
	// we pass Cells around, this makes code cleaner
	struct FVoxelCell
	{
		// TODO we do not actually need to store i, we just need the min-corner!
		FVoxelVector3i i[8];    // indices of corners of cell
#ifdef VOXEL_USE_GRADIENT_AS_NORMAL
		FVoxelVector3f n[8];
#endif
		Real f[8];      // field values at corners
	};


	virtual void SetDimensions()
	{
		int NX = (Bounds.Width() / Step);
		int NY = (Bounds.Height() / Step);
		int NZ = (Bounds.Depth() / Step);

#ifdef VOXEL_USE_GRADIENT_AS_NORMAL
		CellMaxDimensions = FVoxelVector3i(NX-1, NY-1, NZ-1);
		CellMinDimensions = FVoxelVector3i::Zero();
#else
		if (bExtraVoxel)
		{
			CellMaxDimensions = FVoxelVector3i(NX, NY, NZ);
			CellMinDimensions = FVoxelVector3i(-1,-1,-1);
		}
		else
		{
			CellMaxDimensions = FVoxelVector3i(NX-1, NY-1, NZ-1);
			CellMinDimensions = FVoxelVector3i::Zero();
		}
#endif
	}

	virtual void corner_pos(const FVoxelVector3i& IJK, FVoxelVector3i& Pos)
	{
		Pos.X = Bounds.Min.X + Step * IJK.X;
		Pos.Y = Bounds.Min.Y + Step * IJK.Y;
		Pos.Z = Bounds.Min.Z + Step * IJK.Z;
	}
	virtual FVoxelVector3i corner_pos(const FVoxelVector3i& IJK)
	{
		return FVoxelVector3i(Bounds.Min.X + Step * IJK.X,
			Bounds.Min.Y + Step * IJK.Y,
			Bounds.Min.Z + Step * IJK.Z);
	}


	//
	// corner and edge hash functions, these pack the coordinate
	// integers into 16-bits, so max of 65536 in any dimension.
	//


	int64 corner_hash(const FVoxelVector3i& Idx)
	{
		return ((int64)Idx.X&0xFFFF) | (((int64)Idx.Y&0xFFFF) << 16) | (((int64)Idx.Z&0xFFFF) << 32);
	}
	int64 corner_hash(int X, int Y, int Z)
	{
		return ((int64)X & 0xFFFF) | (((int64)Y & 0xFFFF) << 16) | (((int64)Z & 0xFFFF) << 32);
	}

	const int64 EDGE_X = int64(1) << 60;
	const int64 EDGE_Y = int64(1) << 61;
	const int64 EDGE_Z = int64(1) << 62;

	int64 edge_hash(const FVoxelVector3i& Idx1, const FVoxelVector3i& Idx2)
	{
		if ( Idx1.X != Idx2.X )
		{
			int xlo = FMath::Min(Idx1.X, Idx2.X);
			return corner_hash(xlo, Idx1.Y, Idx1.Z) | EDGE_X;
		}
		else if ( Idx1.Y != Idx2.Y )
		{
			int ylo = FMath::Min(Idx1.Y, Idx2.Y);
			return corner_hash(Idx1.X, ylo, Idx1.Z) | EDGE_Y;
		}
		else
		{
			int zlo = FMath::Min(Idx1.Z, Idx2.Z);
			return corner_hash(Idx1.X, Idx1.Y, zlo) | EDGE_Z;
		}
	}



	//
	// Hash table for edge vertices
	//

	const int64 NumEdgeVertexSections = 16;
	TArray<TMap<int64, int>> EdgeVertexSections;
	TArray<FCriticalSection> EdgeVertexSectionLocks;
	
	int FindVertexID(int64 hash)
	{
		int32 SectionIndex = (int32)(hash % (NumEdgeVertexSections - 1));
		FScopeLock Lock(&EdgeVertexSectionLocks[SectionIndex]);
		int* Found = EdgeVertexSections[SectionIndex].Find(hash);
		return (Found != nullptr) ? *Found : IndexConstants::InvalidID;
	}

#ifdef VOXEL_USE_GRADIENT_AS_NORMAL
	int  AppendOrFindVertexID(int64 hash, FVoxelVector Pos, FVoxelVector3f Normal)
	{
		int32 SectionIndex = (int32)(hash % (NumEdgeVertexSections - 1));
		FScopeLock Lock(&EdgeVertexSectionLocks[SectionIndex]);
		int* FoundVID = EdgeVertexSections[SectionIndex].Find(hash);
		if (FoundVID != nullptr)
		{
			return *FoundVID;
		}
		int NewVID = append_vertex(Pos, Normal, hash);
		EdgeVertexSections[SectionIndex].Add(hash, NewVID);
		return NewVID;
	}
#else
	int  AppendOrFindVertexID(int64 hash, FVoxelVector Pos)
	{
		int32 SectionIndex = (int32)(hash % (NumEdgeVertexSections - 1));
		FScopeLock Lock(&EdgeVertexSectionLocks[SectionIndex]);
		int* FoundVID = EdgeVertexSections[SectionIndex].Find(hash);
		if (FoundVID != nullptr)
		{
			return *FoundVID;
		}
		int NewVID = append_vertex(Pos, hash);
		EdgeVertexSections[SectionIndex].Add(hash, NewVID);
		return NewVID;
	}
#endif

#ifdef VOXEL_USE_GRADIENT_AS_NORMAL
	virtual int edge_vertex_id(const FVoxelVector3i& Idx1, const FVoxelVector3i& Idx2, double F1, double F2,
		FVoxelVector3f N1, FVoxelVector3f N2);
#else
	virtual int edge_vertex_id(const FVoxelVector3i& Idx1, const FVoxelVector3i& Idx2, double F1, double F2);
#endif

	virtual double corner_value(const FVoxelVector3i& Idx) 
	{
		FVoxelVector3i V = corner_pos(Idx);
		return Implicit(V);
	}

#ifdef VOXEL_USE_GRADIENT_AS_NORMAL
	virtual FVoxelVector3f corner_normal(const FVoxelVector3i& Idx)
	{
		FVoxelVector3i V = corner_pos(Idx);
		return ImplicitGradient(V);
	}
#endif
	
	virtual void initialize_cell_values(FVoxelCell& Cell, bool Shift)
	{
		if (Shift)
		{
			Cell.f[1] = corner_value(Cell.i[1]);
			Cell.f[2] = corner_value(Cell.i[2]);
			Cell.f[5] = corner_value(Cell.i[5]);
			Cell.f[6] = corner_value(Cell.i[6]);

#ifdef VOXEL_USE_GRADIENT_AS_NORMAL
			Cell.n[1] = corner_normal(Cell.i[1]);
			Cell.n[2] = corner_normal(Cell.i[2]);
			Cell.n[5] = corner_normal(Cell.i[5]);
			Cell.n[6] = corner_normal(Cell.i[6]);
#endif
		}
		else
		{
			for (int i = 0; i < 8; ++i)
			{
				Cell.f[i] = corner_value(Cell.i[i]);
#ifdef VOXEL_USE_GRADIENT_AS_NORMAL
				Cell.n[i] = corner_normal(Cell.i[i]);
#endif
			}
		}
	}

	/**
	*  compute 3D corner-positions and field values for cell at index
	*/
	void initialize_cell(FVoxelCell& Cell, const FVoxelVector3i& Idx)
	{
		Cell.i[0] = FVoxelVector3i(Idx.X + 0, Idx.Y + 0, Idx.Z + 0);
		Cell.i[1] = FVoxelVector3i(Idx.X + 1, Idx.Y + 0, Idx.Z + 0);
		Cell.i[2] = FVoxelVector3i(Idx.X + 1, Idx.Y + 0, Idx.Z + 1);
		Cell.i[3] = FVoxelVector3i(Idx.X + 0, Idx.Y + 0, Idx.Z + 1);
		Cell.i[4] = FVoxelVector3i(Idx.X + 0, Idx.Y + 1, Idx.Z + 0);
		Cell.i[5] = FVoxelVector3i(Idx.X + 1, Idx.Y + 1, Idx.Z + 0);
		Cell.i[6] = FVoxelVector3i(Idx.X + 1, Idx.Y + 1, Idx.Z + 1);
		Cell.i[7] = FVoxelVector3i(Idx.X + 0, Idx.Y + 1, Idx.Z + 1);
		
		initialize_cell_values(Cell, false);
	}


	// assume we just want to slide cell at XIdx-1 to cell at XIdx, while keeping
	// yi and ZIdx constant. Then only x-coords change, and we have already 
	// computed half the values
	virtual void shift_cell_x(FVoxelCell& Cell, int XIdx)
	{
		Cell.f[0] = Cell.f[1];
		Cell.f[3] = Cell.f[2];
		Cell.f[4] = Cell.f[5];
		Cell.f[7] = Cell.f[6];

		Cell.i[0].X = XIdx; Cell.i[1].X = XIdx+1; Cell.i[2].X = XIdx+1; Cell.i[3].X = XIdx;
		Cell.i[4].X = XIdx; Cell.i[5].X = XIdx+1; Cell.i[6].X = XIdx+1; Cell.i[7].X = XIdx;

#ifdef VOXEL_USE_GRADIENT_AS_NORMAL
		Cell.n[0] = Cell.n[1];
		Cell.n[3] = Cell.n[2];
		Cell.n[4] = Cell.n[5];
		Cell.n[7] = Cell.n[6];
#endif
		
		initialize_cell_values(Cell, true);
	}


	void InitHashTables()
	{
		EdgeVertexSections.Reset();
		EdgeVertexSections.SetNum((int32)NumEdgeVertexSections);
		EdgeVertexSectionLocks.Reset();
		EdgeVertexSectionLocks.SetNum((int32)NumEdgeVertexSections);
	}


	bool parallel_mesh_access = false;


	/**
	*  processing z-slabs of cells in parallel
	*/
	virtual void generate_parallel()
	{
		parallel_mesh_access = true;

		// [TODO] maybe shouldn't alway use Z axis here?
		ParallelFor(CellMaxDimensions.Z - CellMinDimensions.Z + 1, [this](int32 ZIdx)
		{
			ZIdx += CellMinDimensions.Z;
			FVoxelCell Cell;
			int vertTArray[12];
			for (int yi = CellMinDimensions.Y; yi <= CellMaxDimensions.Y; ++yi)
			{
				if (CancelF())
				{
					return;
				}
				// compute full cell at x=0, then slide along x row, which saves half of value computes
				FVoxelVector3i Idx(CellMinDimensions.X, yi, ZIdx);
				initialize_cell(Cell, Idx);
				polygonize_cell(Cell, vertTArray);
				for (int XIdx = CellMinDimensions.X + 1; XIdx <= CellMaxDimensions.X; ++XIdx)
				{
					shift_cell_x(Cell, XIdx);
					polygonize_cell(Cell, vertTArray);
				}
			}
		});


		parallel_mesh_access = false;
	}

	/**
	*  fully sequential version, no threading
	*/
	virtual void generate_basic()
	{
		FVoxelCell Cell;
		int vertTArray[12];

		// when bExtraVoxel is true, sampling zIdx = 0 will get voxel at z = -1
		// also, CellDimensions.Z has ChunkSize + 2
		for (int ZIdx = CellMinDimensions.Z; ZIdx <= CellMaxDimensions.Z; ++ZIdx)
		{
			for (int yi = CellMinDimensions.Y; yi <= CellMaxDimensions.Y; ++yi)
			{
				if (CancelF())
				{
					return;
				}
				// compute full Cell at x=0, then slide along x row, which saves half of value computes
				FVoxelVector3i Idx(CellMinDimensions.X, yi, ZIdx);
				initialize_cell(Cell, Idx);
				polygonize_cell(Cell, vertTArray);
				for (int XIdx = CellMinDimensions.X + 1; XIdx <= CellMaxDimensions.X; ++XIdx)
				{
					shift_cell_x(Cell, XIdx);
					polygonize_cell(Cell, vertTArray);
				}

			}
		}
	}

	/**
	*  find edge crossings and generate triangles for this cell
	*/
	virtual bool polygonize_cell(FVoxelCell& Cell, int VertIndexArray[])
	{
		// construct bits of index into edge table, where bit for each
		// corner is 1 if that value is < isovalue.
		// This tell us which edges have sign-crossings, and the int value
		// of the bitmap is an index into the edge and triangle tables
		int cubeindex = 0, Shift = 1;
		for (int i = 0; i < 8; ++i)
		{
			if (Cell.f[i] < IsoValue)
			{
				cubeindex |= Shift;
			}
			Shift <<= 1;
		}

		// no crossings!
		if (EdgeTable[cubeindex] == 0)
		{
			return false;
		}

		// check each bit of value in edge table. If it is 1, we
		// have a crossing on that edge. Look up the indices of this
		// edge and find the intersection point along it
		Shift = 1;
		FVoxelVector pa = FVoxelVector::Zero(), pb = FVoxelVector::Zero();
		for (int i = 0; i <= 11; i++)
		{
			if ((EdgeTable[cubeindex] & Shift) != 0)
			{
				int a = EdgeIndices[i][0], b = EdgeIndices[i][1];
				VertIndexArray[i] = edge_vertex_id(Cell.i[a], Cell.i[b], Cell.f[a], Cell.f[b],
#ifdef VOXEL_USE_GRADIENT_AS_NORMAL
					Cell.n[a], Cell.n[b]
#endif
					);
			}
			Shift <<= 1;
		}

		int64 CellHash = corner_hash(Cell.i[0]);

		// now iterate through the set of triangles in TriTable for this cube,
		// and emit triangles using the vertices we found.
		int tri_count = 0;
		for (int i = 0; TriTable[cubeindex][i] != -1; i += 3)
		{
			int ta = TriTable[cubeindex][i];
			int tb = TriTable[cubeindex][i + 1];
			int tc = TriTable[cubeindex][i + 2];
			int a = VertIndexArray[ta], b = VertIndexArray[tb], c = VertIndexArray[tc];

			// if a corner is within tolerance of isovalue, then some triangles
			// will be degenerate, and we can skip them w/o resulting in cracks (right?)
			// !! this should never happen anymore...artifact of old hashtable impl
			if (!ensure(a != b && a != c && b != c))
			{
				continue;
			}

			append_triangle(a, b, c, CellHash);
			tri_count++;
		}

		return (tri_count > 0);
	}


	struct FIndexedVertex
	{
		int32 Index;
		FVector3d Position;
#ifdef VOXEL_USE_GRADIENT_AS_NORMAL
		FVector3f Normal;
#endif
	};
	std::atomic<int32> VertexCounter;

	int64 NumVertexSections = 16;
	TArray<FCriticalSection> VertexSectionLocks;
	TArray<TArray<FIndexedVertex>> VertexSectionLists;
	int GetVertexSectionIndex(int64 hash)
	{
		return (int32)(hash % (NumVertexSections - 1));
	}

	/**
	*  add vertex to mesh, with locking if we are computing in parallel
	*/
#ifdef VOXEL_USE_GRADIENT_AS_NORMAL
	int append_vertex(FVoxelVector V, FVoxelVector3f N, int64 CellHash)
	{
		int SectionIndex = GetVertexSectionIndex(CellHash);
		int32 NewIndex = VertexCounter++;

		if (parallel_mesh_access)
		{
			FScopeLock Lock(&VertexSectionLocks[SectionIndex]);
			VertexSectionLists[SectionIndex].Add(FIndexedVertex{ NewIndex, V, N});
		}
		else
		{
			VertexSectionLists[SectionIndex].Add(FIndexedVertex{ NewIndex, V, N});
		}

		return NewIndex;
	}
#else
	int append_vertex(FVoxelVector V, FVoxelVector3f N, int64 CellHash)
	{
		int SectionIndex = GetVertexSectionIndex(CellHash);
		int32 NewIndex = VertexCounter++;

		if (parallel_mesh_access)
		{
			FScopeLock Lock(&VertexSectionLocks[SectionIndex]);
			VertexSectionLists[SectionIndex].Add(FIndexedVertex{ NewIndex, V });
		}
		else
		{
			VertexSectionLists[SectionIndex].Add(FIndexedVertex{ NewIndex, V });
		}

		return NewIndex;
	}
#endif


	int64 NumTriangleSections = 16; // for small chunk size 
	TArray<FCriticalSection> TriangleSectionLocks;
	TArray<TArray<FVoxelIndex3i>> TriangleSectionLists;
	int GetTriangleSectionIndex(int64 hash)
	{
		return (int32)(hash % (NumTriangleSections - 1));
	}

	/**
	*  add triangle to mesh, with locking if we are computing in parallel
	*/
	void append_triangle(int A, int B, int C, int64 CellHash)
	{
		int SectionIndex = GetTriangleSectionIndex(CellHash);
		if (parallel_mesh_access)
		{
			FScopeLock Lock(&TriangleSectionLocks[SectionIndex]);
			TriangleSectionLists[SectionIndex].Add(FVoxelIndex3i(A, B, C));
		}
		else
		{
			TriangleSectionLists[SectionIndex].Add(FVoxelIndex3i(A, B, C));
		}
	}


	/**
	 * Reset internal mesh-assembly data structures
	 */
	void ResetMesh()
	{
		VertexSectionLocks.SetNum((int32)NumVertexSections);
		VertexSectionLists.Reset();
		VertexSectionLists.SetNum((int32)NumVertexSections);
		VertexCounter = 0;

		TriangleSectionLocks.SetNum((int32)NumTriangleSections);
		TriangleSectionLists.Reset();
		TriangleSectionLists.SetNum((int32)NumTriangleSections);
	}

	/**
	 * Populate FMeshShapeGenerator data structures from accumulated
	 * vertex/triangle sets
	 * remove vertices outside of bounds (inside of margin)
	 */
	void BuildMesh()
	{
	    TRACE_CPUPROFILER_EVENT_SCOPE(Geometry_MCMesh_BuildMesh);

#ifdef VOXEL_USE_GRADIENT_AS_NORMAL
		int32 NumVertices = VertexCounter;
		TArray<FVector3d> VertexBuffer;
		TArray<FVector3f> NormalBuffer;
		VertexBuffer.SetNum(NumVertices);
		NormalBuffer.SetNum(NumVertices);
		for (const TArray<FIndexedVertex>& VertexList : VertexSectionLists)
		{
			for (FIndexedVertex Vtx : VertexList)
			{
				VertexBuffer[Vtx.Index] = Vtx.Position;
				NormalBuffer[Vtx.Index] = Vtx.Normal;
			}
		}

		// 1. Bounds 안에 있는 vertex만 필터링, remap 생성
		TMap<int32, int32> OldToNewIndex;
		TArray<FVector3d> FilteredVertices;
		TArray<FVector3f> FilteredNormals;
		for (int32 k = 0; k < NumVertices; ++k)
		{
			UE::Geometry::FVector3i RoundDown =
				{ FMath::FloorToInt32(VertexBuffer[k].X),
				FMath::FloorToInt32(VertexBuffer[k].Y),
				FMath::FloorToInt32(VertexBuffer[k].Z)
				};
			if (Bounds.Contains(static_cast<UE::Geometry::FVector3i>(RoundDown)))
			{
				int32 NewIndex = FilteredVertices.Num();
				FilteredVertices.Add(VertexBuffer[k]);
				FilteredNormals.Add(NormalBuffer[k]);
				OldToNewIndex.Add(k, NewIndex);
			}
		}

#else
	    int32 NumVertices = VertexCounter;
	    TArray<FVector3d> VertexBuffer;
	    VertexBuffer.SetNum(NumVertices);
	    for (const TArray<FIndexedVertex>& VertexList : VertexSectionLists)
	    {
	        for (FIndexedVertex Vtx : VertexList)
	        {
	            VertexBuffer[Vtx.Index] = Vtx.Position;
	        }
	    }

		// 모든 vertex와 삼각형을 이용해 normal 계산
	    TArray<FVector3f> VertexNormals;
	    VertexNormals.SetNumZeroed(NumVertices);
	    for (const TArray<FVoxelIndex3i>& TriangleList : TriangleSectionLists)
	    {
	        for (FVoxelIndex3i Tri : TriangleList)
	        {
	            const FVector3f A = static_cast<FVector3f>(VertexBuffer[Tri.A]);
	            const FVector3f B = static_cast<FVector3f>(VertexBuffer[Tri.B]);
	            const FVector3f C = static_cast<FVector3f>(VertexBuffer[Tri.C]);
	            FVector3f Normal = FVector3f::CrossProduct(C - A, B - A).GetSafeNormal();

	            VertexNormals[Tri.A] += Normal;
	            VertexNormals[Tri.B] += Normal;
	            VertexNormals[Tri.C] += Normal;
	        }
	    }
	    for (FVector3f& N : VertexNormals)
	        N.Normalize();

		// 1. Bounds 안에 있는 vertex만 필터링, remap 생성
	    TMap<int32, int32> OldToNewIndex;
	    TArray<FVector3d> FilteredVertices;
	    TArray<FVector3f> FilteredNormals;
	    for (int32 k = 0; k < NumVertices; ++k)
	    {
	        if (Bounds.Contains(static_cast<UE::Geometry::FVector3i>(VertexBuffer[k])))
	        {
	            int32 NewIndex = FilteredVertices.Num();
	            FilteredVertices.Add(VertexBuffer[k]);
	            FilteredNormals.Add(VertexNormals[k]);
	            OldToNewIndex.Add(k, NewIndex);
	        }
	    }

#endif
		
	    // 2. Output: vertex, normal, triangle 모두 remapped index로만 추가
	    /*constexpr float BlendBias = 0.5f;
	    constexpr float BlendExponent = 1.0f;
	    for (int32 i = 0; i < FilteredVertices.Num(); ++i)
	    {
	        int32 vid = AppendVertex(FilteredVertices[i]);
	        AppendNormal(FilteredNormals[i], vid);

	    	// --- Triplanar Mapping ---
	    	const FVector3d& Pos = FilteredVertices[i];
	    	const FVector3f& Normal = FilteredNormals[i];

	    	double CS = (CHUNK_SIZE+1.0);

	    	// 각 축에 대해 투영된 UV 계산
	    	FVector2f UvX(FMath::Frac(Pos.Y / CS), FMath::Frac(Pos.Z / CS));
	    	FVector2f UvY(FMath::Frac(Pos.X / CS), FMath::Frac(Pos.Z / CS));
	    	FVector2f UvZ(FMath::Frac(Pos.X / CS), FMath::Frac(Pos.Y / CS));
	    	/*FVector2f UvX(Pos.Y, Pos.Z); // X축 투영
	    	FVector2f UvY(Pos.X, Pos.Z); // Y축 투영
	    	FVector2f UvZ(Pos.X, Pos.Y); // Z축 투영#1#

	    	// Normal의 절댓값을 사용해 축별 가중치 계산
	    	// 모든 축이 같은 가중치를 가지면 AbsNormal의 xyz가 모두 같을 때 일 때 동일한 uv가 발생.
	    	FVector3f AbsNormal = Normal.GetAbs();
	    	
	    	/*float BlendSharpness = 2.0f; // 이 값을 조절
	    	FVector3f Weights = FVector3f(
				FMath::Pow(AbsNormal.X, BlendSharpness),
				FMath::Pow(AbsNormal.Y, BlendSharpness),
				FMath::Pow(AbsNormal.Z, BlendSharpness)
			);#1#
	    	
	    	 float Exponent = 1.3f;
	    	 FVector3f Weights = FVector3f(
				FMath::Pow(AbsNormal.X, Exponent),
				FMath::Pow(AbsNormal.Y, Exponent*Exponent),
				FMath::Pow(AbsNormal.Z, Exponent*Exponent*Exponent)
			);
	    	float Sum = Weights.X + Weights.Y + Weights.Z;
	    	if (Sum > KINDA_SMALL_NUMBER) // Unreal Engine의 내장 매크로 (보통 1e-4f)
	    	{
	    		Weights /= Sum;
	    	}
	    	else
	    	{
	    		// Normal이 0일 때의 예외 처리
	    		// 어떤 축을 기본으로 할지 결정합니다. 보통 Z축(위/아래)이 안전합니다.
	    		Weights = FVector3f(0.f, 0.f, 1.f); 
	    	}

	    	//Weights /= Sum;
	    	
	    	// Triplanar UV 혼합
	    	FVector2f TriplanarUV =
				UvX * Weights.X +
				UvY * Weights.Y +
				UvZ * Weights.Z;
	    	AppendUV(TriplanarUV, vid);
			
	    	//TriplanarUV /= CHUNK_SIZE;

	    	// 필요시 스케일/오프셋 적용 가능
	    	//AppendUV(TriplanarUV, vid);
	    }*/

	    /*for (const TArray<FVoxelIndex3i>& TriangleList : TriangleSectionLists)
	    {
	        for (FVoxelIndex3i Tri : TriangleList)
	        {
	            int32* NewA = OldToNewIndex.Find(Tri.A);
	            int32* NewB = OldToNewIndex.Find(Tri.B);
	            int32* NewC = OldToNewIndex.Find(Tri.C);
	            if (NewA && NewB && NewC)
	            {
	                AppendTriangle(*NewA, *NewB, *NewC);
	            }
	            // Bounds를 벗어난 vertex가 포함된 triangle은 skip
	        }
	    }*/
	// --- 3. Box Mapping UV 계산 및 정점/삼각형 최종 추가 ---

    const double CS = CHUNK_SIZE+1.0;

    // 삼각형을 순회하며 Box-Mapping UV를 적용합니다.
    for (const TArray<FVoxelIndex3i>& TriangleList : TriangleSectionLists)
    {
        for (const FVoxelIndex3i& Tri : TriangleList)
        {
            // 3a. 삼각형의 정점들이 모두 필터링된 목록에 있는지 확인합니다.
            const int32* pOldA = OldToNewIndex.Find(Tri.A);
            const int32* pOldB = OldToNewIndex.Find(Tri.B);
            const int32* pOldC = OldToNewIndex.Find(Tri.C);

            // Bounds를 벗어난 정점이 포함된 삼각형은 건너뜁니다.
            if (!pOldA || !pOldB || !pOldC)
            {
                continue;
            }

            // 필터링된 정점의 인덱스와 위치를 가져옵니다.
            const int32 OldIndexA = *pOldA;
            const int32 OldIndexB = *pOldB;
            const int32 OldIndexC = *pOldC;

            const FVector3d& PosA = FilteredVertices[OldIndexA];
            const FVector3d& PosB = FilteredVertices[OldIndexB];
            const FVector3d& PosC = FilteredVertices[OldIndexC];

            // 3b. Face Normal 계산
            FVector3f FaceNormal = FVector3f::CrossProduct(
                static_cast<FVector3f>(PosC - PosA),
                static_cast<FVector3f>(PosB - PosA)
            ).GetSafeNormal(); // GetSafeNormal은 0벡터일 경우 (1,0,0) 같은 기본값을 반환하여 안전합니다.

            FVector3f AbsFaceNormal = FaceNormal.GetAbs();
            
            FVector2f UvA, UvB, UvC;

            // 3c. Hard Projection으로 UV 계산
            if (AbsFaceNormal.X > AbsFaceNormal.Y && AbsFaceNormal.X > AbsFaceNormal.Z) // X축 지배
            {
                UvA = FVector2f(FMath::Frac(PosA.Y / CS), FMath::Frac(PosA.Z / CS));
                UvB = FVector2f(FMath::Frac(PosB.Y / CS), FMath::Frac(PosB.Z / CS));
                UvC = FVector2f(FMath::Frac(PosC.Y / CS), FMath::Frac(PosC.Z / CS));
            }
            else if (AbsFaceNormal.Y > AbsFaceNormal.Z) // Y축 지배
            {
                UvA = FVector2f(FMath::Frac(PosA.X / CS), FMath::Frac(PosA.Z / CS));
                UvB = FVector2f(FMath::Frac(PosB.X / CS), FMath::Frac(PosB.Z / CS));
                UvC = FVector2f(FMath::Frac(PosC.X / CS), FMath::Frac(PosC.Z / CS));
            }
            else // Z축 지배 (또는 모든 값이 같을 때의 기본값)
            {
                UvA = FVector2f(FMath::Frac(PosA.X / CS), FMath::Frac(PosA.Y / CS));
                UvB = FVector2f(FMath::Frac(PosB.X / CS), FMath::Frac(PosB.Y / CS));
                UvC = FVector2f(FMath::Frac(PosC.X / CS), FMath::Frac(PosC.Y / CS));
            }

            // 3d. 정점 복제: 각 삼각형은 고유한 정점을 가집니다.
            // 이렇게 하면 UV가 다른 삼각형에 의해 덮어씌워지지 않습니다.
            int32 NewIndexA = AppendVertex(PosA);
            int32 NewIndexB = AppendVertex(PosB);
            int32 NewIndexC = AppendVertex(PosC);

            // 복제된 새 정점에 Normal과 UV를 할당합니다.
            AppendNormal(FilteredNormals[OldIndexA], NewIndexA);
            AppendNormal(FilteredNormals[OldIndexB], NewIndexB);
            AppendNormal(FilteredNormals[OldIndexC], NewIndexC);

            AppendUV(UvA, NewIndexA);
            AppendUV(UvB, NewIndexB);
            AppendUV(UvC, NewIndexC);
            
            // 최종 삼각형을 추가합니다.
            AppendTriangle(NewIndexA, NewIndexB, NewIndexC);
        }
    }
		
	}



	/**
	*  root-find the intersection along edge from f(P1)=ValP1 to f(P2)=ValP2
	*/
	void find_iso(const FVoxelVector3i& P1, const FVoxelVector3i& P2, Real ValP1, Real ValP2, FVoxelVector& PIso)
	{
		// Ok, this is a bit hacky but seems to work? If both isovalues
		// are the same, we just return the midpoint. If one is nearly zero, we can
		// but assume that's where the surface is. *However* if we return that point exactly,
		// we can get nonmanifold vertices, because multiple fans may connect there. 
		// Since FDynamicMesh3 disallows that, it results in holes. So we pull 
		// slightly towards the other point along this edge. This means we will get
		// repeated nearly-coincident vertices, but the mesh will be manifold.
		const Real dt = 0.999999;
		if (FMath::Abs(ValP1 - ValP2) < 0.00001)
		{
			PIso = FVoxelVector(P1 + P2) * 0.5;
			return;
		}
		if (FMath::Abs(IsoValue - ValP1) < 0.00001)
		{
			PIso = dt * FVoxelVector(P1) + (1.0 - dt) * FVoxelVector(P2);
			return;
		}
		if (FMath::Abs(IsoValue - ValP2) < 0.00001)
		{
			PIso = (dt) * FVoxelVector(P2) + (1.0 - dt) * FVoxelVector(P1);
			return;
		}

		// Note: if we don't maintain min/max order here, then numerical error means
		//   that hashing on point x/y/z doesn't work
		FVoxelVector3i a = P1, b = P2;
		Real fa = ValP1, fb = ValP2;
		if (ValP2 < ValP1)
		{
			a = P2; b = P1;
			fb = ValP1; fa = ValP2;
		}

		// final lerp
		Real mu = FMathd::Clamp((IsoValue - fa) / (fb - fa), 0.0, 1.0);
		PIso.X = a.X + mu * (b.X - a.X);
		PIso.Y = a.Y + mu * (b.Y - a.Y);
		PIso.Z = a.Z + mu * (b.Z - a.Z);
	}




	/*
	* Below here are standard marching-cubes tables. 
	*/


	constexpr static int EdgeIndices[12][2] = {
		{0,1}, {1,2}, {2,3}, {3,0}, {4,5}, {5,6}, {6,7}, {7,4}, {0,4}, {1,5}, {2,6}, {3,7}
	};

	constexpr static int EdgeTable[256] = {
		0x0  , 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
		0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
		0x190, 0x99 , 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
		0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
		0x230, 0x339, 0x33 , 0x13a, 0x636, 0x73f, 0x435, 0x53c,
		0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
		0x3a0, 0x2a9, 0x1a3, 0xaa , 0x7a6, 0x6af, 0x5a5, 0x4ac,
		0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
		0x460, 0x569, 0x663, 0x76a, 0x66 , 0x16f, 0x265, 0x36c,
		0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
		0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0xff , 0x3f5, 0x2fc,
		0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
		0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x55 , 0x15c,
		0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
		0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0xcc ,
		0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
		0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
		0xcc , 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
		0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
		0x15c, 0x55 , 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
		0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
		0x2fc, 0x3f5, 0xff , 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
		0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
		0x36c, 0x265, 0x16f, 0x66 , 0x76a, 0x663, 0x569, 0x460,
		0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
		0x4ac, 0x5a5, 0x6af, 0x7a6, 0xaa , 0x1a3, 0x2a9, 0x3a0,
		0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
		0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x33 , 0x339, 0x230,
		0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
		0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x99 , 0x190,
		0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
		0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x0   };


	constexpr static int TriTable[256][16] =
	{
		{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{1, 8, 3, 9, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 8, 3, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{9, 2, 10, 0, 2, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{2, 8, 3, 2, 10, 8, 10, 9, 8, -1, -1, -1, -1, -1, -1, -1},
		{3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 11, 2, 8, 11, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{1, 9, 0, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{1, 11, 2, 1, 9, 11, 9, 8, 11, -1, -1, -1, -1, -1, -1, -1},
		{3, 10, 1, 11, 10, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 10, 1, 0, 8, 10, 8, 11, 10, -1, -1, -1, -1, -1, -1, -1},
		{3, 9, 0, 3, 11, 9, 11, 10, 9, -1, -1, -1, -1, -1, -1, -1},
		{9, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{4, 3, 0, 7, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 1, 9, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{4, 1, 9, 4, 7, 1, 7, 3, 1, -1, -1, -1, -1, -1, -1, -1},
		{1, 2, 10, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{3, 4, 7, 3, 0, 4, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1},
		{9, 2, 10, 9, 0, 2, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
		{2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1, -1, -1, -1},
		{8, 4, 7, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{11, 4, 7, 11, 2, 4, 2, 0, 4, -1, -1, -1, -1, -1, -1, -1},
		{9, 0, 1, 8, 4, 7, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
		{4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1, -1, -1, -1},
		{3, 10, 1, 3, 11, 10, 7, 8, 4, -1, -1, -1, -1, -1, -1, -1},
		{1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1, -1, -1, -1},
		{4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1, -1, -1, -1},
		{4, 7, 11, 4, 11, 9, 9, 11, 10, -1, -1, -1, -1, -1, -1, -1},
		{9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{9, 5, 4, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 5, 4, 1, 5, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{8, 5, 4, 8, 3, 5, 3, 1, 5, -1, -1, -1, -1, -1, -1, -1},
		{1, 2, 10, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{3, 0, 8, 1, 2, 10, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
		{5, 2, 10, 5, 4, 2, 4, 0, 2, -1, -1, -1, -1, -1, -1, -1},
		{2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1, -1, -1, -1},
		{9, 5, 4, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 11, 2, 0, 8, 11, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
		{0, 5, 4, 0, 1, 5, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
		{2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1, -1, -1, -1},
		{10, 3, 11, 10, 1, 3, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1},
		{4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1, -1, -1, -1},
		{5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1, -1, -1, -1},
		{5, 4, 8, 5, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1},
		{9, 7, 8, 5, 7, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{9, 3, 0, 9, 5, 3, 5, 7, 3, -1, -1, -1, -1, -1, -1, -1},
		{0, 7, 8, 0, 1, 7, 1, 5, 7, -1, -1, -1, -1, -1, -1, -1},
		{1, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{9, 7, 8, 9, 5, 7, 10, 1, 2, -1, -1, -1, -1, -1, -1, -1},
		{10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1, -1, -1, -1},
		{8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1, -1, -1, -1},
		{2, 10, 5, 2, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1},
		{7, 9, 5, 7, 8, 9, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1},
		{9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1, -1, -1, -1},
		{2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1, -1, -1, -1},
		{11, 2, 1, 11, 1, 7, 7, 1, 5, -1, -1, -1, -1, -1, -1, -1},
		{9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1, -1, -1, -1},
		{5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1},
		{11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1},
		{11, 10, 5, 7, 11, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 8, 3, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{9, 0, 1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{1, 8, 3, 1, 9, 8, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
		{1, 6, 5, 2, 6, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{1, 6, 5, 1, 2, 6, 3, 0, 8, -1, -1, -1, -1, -1, -1, -1},
		{9, 6, 5, 9, 0, 6, 0, 2, 6, -1, -1, -1, -1, -1, -1, -1},
		{5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1, -1, -1, -1},
		{2, 3, 11, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{11, 0, 8, 11, 2, 0, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
		{0, 1, 9, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
		{5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1, -1, -1, -1},
		{6, 3, 11, 6, 5, 3, 5, 1, 3, -1, -1, -1, -1, -1, -1, -1},
		{0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1, -1, -1, -1},
		{3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1, -1, -1, -1},
		{6, 5, 9, 6, 9, 11, 11, 9, 8, -1, -1, -1, -1, -1, -1, -1},
		{5, 10, 6, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{4, 3, 0, 4, 7, 3, 6, 5, 10, -1, -1, -1, -1, -1, -1, -1},
		{1, 9, 0, 5, 10, 6, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
		{10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1, -1, -1, -1},
		{6, 1, 2, 6, 5, 1, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1},
		{1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1, -1, -1, -1},
		{8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1, -1, -1, -1},
		{7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1},
		{3, 11, 2, 7, 8, 4, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
		{5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1, -1, -1, -1},
		{0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1},
		{9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1},
		{8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1, -1, -1, -1},
		{5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1},
		{0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1},
		{6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1, -1, -1, -1},
		{10, 4, 9, 6, 4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{4, 10, 6, 4, 9, 10, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1},
		{10, 0, 1, 10, 6, 0, 6, 4, 0, -1, -1, -1, -1, -1, -1, -1},
		{8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1, -1, -1, -1},
		{1, 4, 9, 1, 2, 4, 2, 6, 4, -1, -1, -1, -1, -1, -1, -1},
		{3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1, -1, -1, -1},
		{0, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{8, 3, 2, 8, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1},
		{10, 4, 9, 10, 6, 4, 11, 2, 3, -1, -1, -1, -1, -1, -1, -1},
		{0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1, -1, -1, -1},
		{3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1, -1, -1, -1},
		{6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1},
		{9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1, -1, -1, -1},
		{8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1},
		{3, 11, 6, 3, 6, 0, 0, 6, 4, -1, -1, -1, -1, -1, -1, -1},
		{6, 4, 8, 11, 6, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{7, 10, 6, 7, 8, 10, 8, 9, 10, -1, -1, -1, -1, -1, -1, -1},
		{0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1, -1, -1, -1},
		{10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1, -1, -1, -1},
		{10, 6, 7, 10, 7, 1, 1, 7, 3, -1, -1, -1, -1, -1, -1, -1},
		{1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1, -1, -1, -1},
		{2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1},
		{7, 8, 0, 7, 0, 6, 6, 0, 2, -1, -1, -1, -1, -1, -1, -1},
		{7, 3, 2, 6, 7, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1, -1, -1, -1},
		{2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1},
		{1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1},
		{11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1, -1, -1, -1},
		{8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1},
		{0, 9, 1, 11, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1, -1, -1, -1},
		{7, 11, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{3, 0, 8, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 1, 9, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{8, 1, 9, 8, 3, 1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
		{10, 1, 2, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{1, 2, 10, 3, 0, 8, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
		{2, 9, 0, 2, 10, 9, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
		{6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1, -1, -1, -1},
		{7, 2, 3, 6, 2, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{7, 0, 8, 7, 6, 0, 6, 2, 0, -1, -1, -1, -1, -1, -1, -1},
		{2, 7, 6, 2, 3, 7, 0, 1, 9, -1, -1, -1, -1, -1, -1, -1},
		{1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1, -1, -1, -1},
		{10, 7, 6, 10, 1, 7, 1, 3, 7, -1, -1, -1, -1, -1, -1, -1},
		{10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1, -1, -1, -1},
		{0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1, -1, -1, -1},
		{7, 6, 10, 7, 10, 8, 8, 10, 9, -1, -1, -1, -1, -1, -1, -1},
		{6, 8, 4, 11, 8, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{3, 6, 11, 3, 0, 6, 0, 4, 6, -1, -1, -1, -1, -1, -1, -1},
		{8, 6, 11, 8, 4, 6, 9, 0, 1, -1, -1, -1, -1, -1, -1, -1},
		{9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1, -1, -1, -1},
		{6, 8, 4, 6, 11, 8, 2, 10, 1, -1, -1, -1, -1, -1, -1, -1},
		{1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1, -1, -1, -1},
		{4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1, -1, -1, -1},
		{10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1},
		{8, 2, 3, 8, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1},
		{0, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1, -1, -1, -1},
		{1, 9, 4, 1, 4, 2, 2, 4, 6, -1, -1, -1, -1, -1, -1, -1},
		{8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1, -1, -1, -1},
		{10, 1, 0, 10, 0, 6, 6, 0, 4, -1, -1, -1, -1, -1, -1, -1},
		{4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1},
		{10, 9, 4, 6, 10, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{4, 9, 5, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 8, 3, 4, 9, 5, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
		{5, 0, 1, 5, 4, 0, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
		{11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1, -1, -1, -1},
		{9, 5, 4, 10, 1, 2, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
		{6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1, -1, -1, -1},
		{7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1, -1, -1, -1},
		{3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1},
		{7, 2, 3, 7, 6, 2, 5, 4, 9, -1, -1, -1, -1, -1, -1, -1},
		{9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1, -1, -1, -1},
		{3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1, -1, -1, -1},
		{6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1},
		{9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1, -1, -1, -1},
		{1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1},
		{4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1},
		{7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1, -1, -1, -1},
		{6, 9, 5, 6, 11, 9, 11, 8, 9, -1, -1, -1, -1, -1, -1, -1},
		{3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1, -1, -1, -1},
		{0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1, -1, -1, -1},
		{6, 11, 3, 6, 3, 5, 5, 3, 1, -1, -1, -1, -1, -1, -1, -1},
		{1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1, -1, -1, -1},
		{0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1},
		{11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1},
		{6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1, -1, -1, -1},
		{5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1, -1, -1, -1},
		{9, 5, 6, 9, 6, 0, 0, 6, 2, -1, -1, -1, -1, -1, -1, -1},
		{1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1},
		{1, 5, 6, 2, 1, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1},
		{10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1, -1, -1, -1},
		{0, 3, 8, 5, 6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{10, 5, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{11, 5, 10, 7, 5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{11, 5, 10, 11, 7, 5, 8, 3, 0, -1, -1, -1, -1, -1, -1, -1},
		{5, 11, 7, 5, 10, 11, 1, 9, 0, -1, -1, -1, -1, -1, -1, -1},
		{10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1, -1, -1, -1},
		{11, 1, 2, 11, 7, 1, 7, 5, 1, -1, -1, -1, -1, -1, -1, -1},
		{0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1, -1, -1, -1},
		{9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1, -1, -1, -1},
		{7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1},
		{2, 5, 10, 2, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1},
		{8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1, -1, -1, -1},
		{9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1, -1, -1, -1},
		{9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1},
		{1, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 8, 7, 0, 7, 1, 1, 7, 5, -1, -1, -1, -1, -1, -1, -1},
		{9, 0, 3, 9, 3, 5, 5, 3, 7, -1, -1, -1, -1, -1, -1, -1},
		{9, 8, 7, 5, 9, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{5, 8, 4, 5, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1},
		{5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1, -1, -1, -1},
		{0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1, -1, -1, -1},
		{10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1},
		{2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1, -1, -1, -1},
		{0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1},
		{0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1},
		{9, 4, 5, 2, 11, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1, -1, -1, -1},
		{5, 10, 2, 5, 2, 4, 4, 2, 0, -1, -1, -1, -1, -1, -1, -1},
		{3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1},
		{5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1, -1, -1, -1},
		{8, 4, 5, 8, 5, 3, 3, 5, 1, -1, -1, -1, -1, -1, -1, -1},
		{0, 4, 5, 1, 0, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1, -1, -1, -1},
		{9, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{4, 11, 7, 4, 9, 11, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1},
		{0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1, -1, -1, -1},
		{1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1, -1, -1, -1},
		{3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1},
		{4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1, -1, -1, -1},
		{9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1},
		{11, 7, 4, 11, 4, 2, 2, 4, 0, -1, -1, -1, -1, -1, -1, -1},
		{11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1, -1, -1, -1},
		{2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1, -1, -1, -1},
		{9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1},
		{3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1},
		{1, 10, 2, 8, 7, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{4, 9, 1, 4, 1, 7, 7, 1, 3, -1, -1, -1, -1, -1, -1, -1},
		{4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1, -1, -1, -1},
		{4, 0, 3, 7, 4, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{4, 8, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{9, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{3, 0, 9, 3, 9, 11, 11, 9, 10, -1, -1, -1, -1, -1, -1, -1},
		{0, 1, 10, 0, 10, 8, 8, 10, 11, -1, -1, -1, -1, -1, -1, -1},
		{3, 1, 10, 11, 3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{1, 2, 11, 1, 11, 9, 9, 11, 8, -1, -1, -1, -1, -1, -1, -1},
		{3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1, -1, -1, -1},
		{0, 2, 11, 8, 0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{3, 2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{2, 3, 8, 2, 8, 10, 10, 8, 9, -1, -1, -1, -1, -1, -1, -1},
		{9, 10, 2, 0, 9, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1, -1, -1, -1},
		{1, 10, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{1, 3, 8, 9, 1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 9, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 3, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}
	};
};
