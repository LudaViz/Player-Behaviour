// Copyright 2025 J2K2. All Rights Reserved.

// UE_DISABLE_OPTIMIZATION

#include "Mesh/MarchingCubes.h"

#include "Generators/MarchingCubes.h"

#include "Chunk/ChunkData.h"
#include "Mesh/MeshUtils.h"
// #include "Mesh/Mesher/TransvoxelTransition.h"
// #include "Mesh/Mesher/TransvoxelRegular.h"
#include "Mesh/Mesher/VoxelMesher.h"


void FCaveMarchingCubes::GenerateMesh(const FChunkData* ChunkData, int32 LODLevel,
                                      FChunkLODLevel& OutMultiMesh)
{
	switch (Type)
	{
	case EMarchingCubesType::Legacy:
		GenerateMeshLegacy(ChunkData, LODLevel, OutMultiMesh);
		break;
	case EMarchingCubesType::VoxelMesher:
		GenerateMeshMesher(ChunkData, LODLevel, OutMultiMesh);
		break;
	// case EMarchingCubesType::TransVoxel:
	// 	GenerateMeshTransVoxel(ChunkData, LODLevel, OutMultiMesh);
	// 	break;
	// case EMarchingCubesType::Regular:
	// 	GenerateMeshRegular(ChunkData, LODLevel, OutMultiMesh);
	// 	break;
	// case EMarchingCubesType::Transition:
	// 	GenerateMeshTransition(ChunkData, LODLevel, OutMultiMesh);
	// 	break;
	// case EMarchingCubesType::GPU:
	// 	break;
	// default: ;
	}
}

void FCaveMarchingCubes::GenerateMeshLegacy(
	const FChunkData* ChunkData,
	int32 LODLevel,
	FChunkLODLevel& OutMultiMesh)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GenerateMeshLegacy)

	TArray<TPair<mat_type, ChunkDensityArray>> DensityArray = ChunkData->DensityMap.Array();
	FCriticalSection MeshMapSection;

	ParallelFor(DensityArray.Num(), [&](int32 Index)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GenerateMeshLegacy-Parellel)
		UE::Geometry::FMarchingCubes* Marcher = new UE::Geometry::FMarchingCubes();
		Marcher->Reset();
	
		// 0~ChunkSize 범위로 정규화된 공간
		FVector WorldMin = FVector::ZeroVector;
		FVector WorldMax = FVector(CHUNK_SIZE-1);

		Marcher->Bounds = UE::Geometry::TAxisAlignedBox3<double>(WorldMin, WorldMax);
		Marcher->CubeSize = 1.0f;
		Marcher->bParallelCompute = true;

		Marcher->IsoValue += 0.3;
		mat_type MatType = DensityArray[Index].Key;
		const ChunkDensityArray& Array = DensityArray[Index].Value;

		Marcher->Implicit = [&Array](UE::Math::TVector<double> Pos) -> double
		{
			FIntVector Coord = FIntVector(Pos);
			return Array.Get(Coord);
		};

		Marcher->Generate();

		FMeshData TempMeshData;
		DCGMeshUtils::ConvertMeshShapeGeneratorToMeshData(Marcher, TempMeshData);
		{
			FScopeLock Lock(&MeshMapSection);
			FChunkMaterialSection& OutMesh = OutMultiMesh.MaterialSections.FindOrAdd(MatType);
			OutMesh.Interior = TempMeshData;
		}
		delete Marcher;
	});
}

void FCaveMarchingCubes::GenerateMeshMesher(const FChunkData* ChunkData, int32 LODLevel, FChunkLODLevel& OutMultiMesh)
{
	TArray<TPair<mat_type, ChunkDensityArray>> DensityArray = ChunkData->DensityMap.Array();
	FCriticalSection MeshMapSection;

	ParallelFor(DensityArray.Num(), [&](int32 Index)
	{
		FVoxelMesher* Marcher = new FVoxelMesher();
		Marcher->Reset();
	
	// 0~ChunkSize 범위로 정규화된 공간
		// const UE::Geometry::FVector3i WorldMin = UE::Geometry::FVector3i(2, 2, 2);
		// const UE::Geometry::FVector3i WorldMax =
		// 	UE::Geometry::FVector3i(CHUNK_SIZE - 2,
		// 							CHUNK_SIZE - 2,
		// 							CHUNK_SIZE - 2);
		const UE::Geometry::FVector3i WorldMin = UE::Geometry::FVector3i(0, 0, 0);
		const UE::Geometry::FVector3i WorldMax =
			UE::Geometry::FVector3i(CHUNK_SIZE,
									CHUNK_SIZE,
									CHUNK_SIZE);

		Marcher->Bounds = FVoxelMesher::FVoxelAxisAlignedBox3i(WorldMin, WorldMax);
		Marcher->Step = 2;
		Marcher->bParallelCompute =	false;

		Marcher->IsoValue = 0.0f;
		mat_type MatType = DensityArray[Index].Key;
		const ChunkDensityArray& Array = DensityArray[Index].Value;

		Marcher->Implicit = [&Array](UE::Geometry::FVector3i Pos) -> double
		{
			FIntVector Coord(Pos.X, Pos.Y, Pos.Z);
			return Array.Get(Coord);
		};

#ifdef VOXEL_USE_GRADIENT_AS_NORMAL
		Marcher->ImplicitGradient = [&Array](UE::Geometry::FVector3i Pos) -> FVector3f
		{
			FIntVector Coord(Pos.X, Pos.Y, Pos.Z);
			return -Array.GetGradient(Coord);
		};
#endif

		Marcher->Generate();

		FMeshData TempMeshData;
		DCGMeshUtils::ConvertMeshShapeGeneratorToMeshData(Marcher, TempMeshData);
		{
			FScopeLock Lock(&MeshMapSection);
			FChunkMaterialSection& OutMesh = OutMultiMesh.MaterialSections.FindOrAdd(MatType);
			OutMesh.Interior = TempMeshData;
		}
		delete Marcher;
	});
}

//
// void FCaveMarchingCubes::GenerateMeshTransVoxel(
// 	const FChunkData* ChunkData,
// 	int32 LODLevel,
// 	FChunkLODLevel& OutMultiMesh)
// {
// 	TArray<TPair<mat_type, ChunkDensityArray>> DensityArray = ChunkData->DensityMap.Array();
// 	FCriticalSection MeshMapSection;
//
// 	float IsoValue = 0.0f;
// 	ParallelFor(DensityArray.Num(), [&](int32 Index)
// 	{
// 		FTransvoxelRegular* Marcher = new FTransvoxelRegular();
// 		Marcher->Reset();
// 	
// 		// 0~ChunkSize 범위로 정규화된 공간
// 		// const UE::Geometry::FVector3i WorldMin = UE::Geometry::FVector3i(2, 2, 2);
// 		// const UE::Geometry::FVector3i WorldMax =
// 		// 	UE::Geometry::FVector3i(CHUNK_SIZE - 2,
// 		// 							CHUNK_SIZE - 2,
// 		// 							CHUNK_SIZE - 2);
// 		const UE::Geometry::FVector3i WorldMin = UE::Geometry::FVector3i(0,0,0);
// 		const UE::Geometry::FVector3i WorldMax =
// 			UE::Geometry::FVector3i(CHUNK_SIZE,
// 									CHUNK_SIZE,
// 									CHUNK_SIZE);
//
// 		Marcher->Bounds = FVoxelMesher::FVoxelAxisAlignedBox3i(WorldMin, WorldMax);
// 		Marcher->Step = 2;
// 		Marcher->bParallelCompute = false;
//
// 		Marcher->IsoValue = IsoValue;
// 		mat_type MatType = DensityArray[Index].Key;
// 		const ChunkDensityArray& Array = DensityArray[Index].Value;
//
// 		Marcher->Implicit = [&Array](UE::Geometry::FVector3i Pos) -> double
// 		{
// 			FIntVector Coord(Pos.X, Pos.Y, Pos.Z);
// 			return Array.Get(Coord);
// 		};
//
// #ifdef VOXEL_USE_GRADIENT_AS_NORMAL
// 		Marcher->ImplicitGradient = [&Array](UE::Geometry::FVector3i Pos) -> FVector3f
// 		{
// 			FIntVector Coord(Pos.X, Pos.Y, Pos.Z);
// 			return -Array.GetGradient(Coord);
// 		};
// #endif
// 		
// 		Marcher->Generate();
//
// 		FMeshData TempMeshData;
// 		DCGMeshUtils::ConvertMeshShapeGeneratorToMeshData(Marcher, TempMeshData);
// 		{
// 			FScopeLock Lock(&MeshMapSection);
// 			FChunkMaterialSection& OutMesh = OutMultiMesh.MaterialSections.FindOrAdd(MatType);
// 			OutMesh.Interior = MoveTemp(TempMeshData);
// 		}
// 		delete Marcher;
// 	});
// }
//
// void FCaveMarchingCubes::GenerateMeshRegular(const FChunkData* ChunkData, int32 LODLevel,
// 	FChunkLODLevel& OutMultiMesh)
// {
// 	TArray<TPair<mat_type, ChunkDensityArray>> DensityArray = ChunkData->DensityMap.Array();
// 	FCriticalSection MeshMapSection;
//
// 	float IsoValue = 0;
// 	ParallelFor(DensityArray.Num(), [&](int32 Index)
// 	{
// 		FTransvoxelRegular* RegularMarcher = new FTransvoxelRegular();
// 		RegularMarcher->Reset();
// 		const int32 Step = 2;
// 	
// 		// 0~ChunkSize 범위로 정규화된 공간
// 		const UE::Geometry::FVector3i WorldMin = UE::Geometry::FVector3i(Step, Step, Step);
// 		const UE::Geometry::FVector3i WorldMax =
// 			UE::Geometry::FVector3i(CHUNK_SIZE - Step,
// 									CHUNK_SIZE - Step,
// 									CHUNK_SIZE - Step);
//
// 		RegularMarcher->Bounds = FVoxelMesher::FVoxelAxisAlignedBox3i(WorldMin, WorldMax);
// 		RegularMarcher->Step = Step;
// 		RegularMarcher->bParallelCompute = false;
//
// 		RegularMarcher->IsoValue = IsoValue;
// 		mat_type MatType = DensityArray[Index].Key;
// 		const ChunkDensityArray& Array = DensityArray[Index].Value;
//
// 		RegularMarcher->Implicit = [&Array](UE::Geometry::FVector3i Pos) -> double
// 		{
// 			FIntVector Coord(Pos.X, Pos.Y, Pos.Z);
// 			return Array.Get(Coord);
// 		};
//
// #ifdef VOXEL_USE_GRADIENT_AS_NORMAL
// 		RegularMarcher->ImplicitGradient = [&Array](UE::Geometry::FVector3i Pos) -> FVector3f
// 		{
// 			FIntVector Coord(Pos.X, Pos.Y, Pos.Z);
// 			return -Array.GetGradient(Coord);
// 		};
// #endif
// 		
// 		RegularMarcher->Generate();
//
// 		FMeshData InteriorMeshData;
// 		DCGMeshUtils::ConvertMeshShapeGeneratorToMeshData(RegularMarcher, InteriorMeshData);
// 				
// 		FMeshData MeshDataTransition[6];
// 		for (int i = 0; i < 6; ++i)
// 		{
// 			RegularMarcher->Reset();
//
// 			UE::Geometry::FVector3i TransitionMin, TransitionMax;
//
// 			switch (i)
// 			{
// 				case 0: // XPlus (x == CHUNK_SIZE - Step ~ CHUNK_SIZE)
// 					TransitionMin = UE::Geometry::FVector3i(CHUNK_SIZE - Step, 0, 0);
// 					TransitionMax = UE::Geometry::FVector3i(CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE);
// 					break;
// 				case 1: // XMinus (x == 0 ~ Step)
// 					TransitionMin = UE::Geometry::FVector3i(0, 0, 0);
// 					TransitionMax = UE::Geometry::FVector3i(Step, CHUNK_SIZE, CHUNK_SIZE);
// 					break;
// 				case 2: // YPlus (y == CHUNK_SIZE - Step ~ CHUNK_SIZE)
// 					TransitionMin = UE::Geometry::FVector3i(0, CHUNK_SIZE - Step, 0);
// 					TransitionMax = UE::Geometry::FVector3i(CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE);
// 					break;
// 				case 3: // YMinus (y == 0 ~ Step)
// 					TransitionMin = UE::Geometry::FVector3i(0, 0, 0);
// 					TransitionMax = UE::Geometry::FVector3i(CHUNK_SIZE, Step, CHUNK_SIZE);
// 					break;
// 				case 4: // ZPlus (z == CHUNK_SIZE - Step ~ CHUNK_SIZE)
// 					TransitionMin = UE::Geometry::FVector3i(0, 0, CHUNK_SIZE - Step);
// 					TransitionMax = UE::Geometry::FVector3i(CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE);
// 					break;
// 				case 5: // ZMinus (z == 0 ~ Step)
// 					TransitionMin = UE::Geometry::FVector3i(0, 0, 0);
// 					TransitionMax = UE::Geometry::FVector3i(CHUNK_SIZE, CHUNK_SIZE, Step);
// 					break;
// 			}
//
// 			RegularMarcher->Bounds = FTransvoxelRegular::FVoxelAxisAlignedBox3i(TransitionMin, TransitionMax);
// 			RegularMarcher->Generate();
//
// 			DCGMeshUtils::ConvertMeshShapeGeneratorToMeshData(RegularMarcher, MeshDataTransition[i]);
// 		}
//
// 		{
// 			FScopeLock Lock(&MeshMapSection);
// 			FChunkMaterialSection& OutMesh = OutMultiMesh.MaterialSections.FindOrAdd(MatType);
// 			OutMesh.Interior = MoveTemp(InteriorMeshData);
// 			for (int i = 0; i < 6; ++i)
// 			{
// 				OutMesh.BorderTransitions.Borders[i] = MoveTemp(MeshDataTransition[i]);
// 			}
// 		}
// 		// IsoValue += 0.3;
// 		delete RegularMarcher;
// 	});
// }
//
// void FCaveMarchingCubes::GenerateMeshTransition(const FChunkData* ChunkData,
// 	int32 LODLevel,
// 	FChunkLODLevel& OutMultiMesh)
// {
// 	TArray<TPair<mat_type, ChunkDensityArray>> DensityArray = ChunkData->DensityMap.Array();
// 	FCriticalSection MeshMapSection;
//
// 	const int32 Step = 1u << LODLevel;
// 	constexpr int32 NumFaces = 6;
// 	float IsoValue = 0;
// 	ParallelFor(DensityArray.Num(), [&](int32 Index)
// 	{
// 		TRACE_CPUPROFILER_EVENT_SCOPE(FCaveMarchingCubes::GenerateMeshTransition::ParallelFor)
// 		
// 		FTransvoxelRegular* RegularMarcher = new FTransvoxelRegular();
// 		RegularMarcher->Reset();
//
// 		constexpr UE::Geometry::FVector3i WorldMin = UE::Geometry::FVector3i(
// 			0, 0, 0);
// 		constexpr UE::Geometry::FVector3i WorldMax = UE::Geometry::FVector3i(
// 			CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE);
//
// 		RegularMarcher->Bounds = FVoxelMesher::FVoxelAxisAlignedBox3i(WorldMin, WorldMax);
// 		RegularMarcher->Step = Step;
// 		RegularMarcher->bParallelCompute = false;
//
// 		RegularMarcher->IsoValue = IsoValue;
// 		mat_type MatType = DensityArray[Index].Key;
// 		const ChunkDensityArray& Array = DensityArray[Index].Value;
//
// 		RegularMarcher->Implicit = [&Array](UE::Geometry::FVector3i Pos) -> double
// 		{
// 			FIntVector Coord(Pos.X, Pos.Y, Pos.Z);
// 			return Array.Get(Coord);
// 		};
//
// #ifdef VOXEL_USE_GRADIENT_AS_NORMAL
// 		RegularMarcher->ImplicitGradient = [&Array](UE::Geometry::FVector3i Pos) -> FVector3f
// 		{
// 			FIntVector Coord(Pos.X, Pos.Y, Pos.Z);
// 			return -Array.GetGradient(Coord);
// 		};
// #endif
//
// 		RegularMarcher->Generate();
//
// 		if (LODLevel == 0)
// 		{
// 			FMeshData FullMeshData;
// 			DCGMeshUtils::ConvertMeshShapeGeneratorToMeshData(RegularMarcher, FullMeshData);
// 			FScopeLock Lock(&MeshMapSection);
// 			FChunkMaterialSection& OutMesh = OutMultiMesh.MaterialSections.FindOrAdd(MatType);
// 			OutMesh.Interior = MoveTemp(FullMeshData);
// 		}
// 		else
// 		{
// 			FMeshData InteriorMeshData;
// 			DCGMeshUtils::SeparateInterior(RegularMarcher, InteriorMeshData);
//
// 			FChunkMeshSectionBorders BorderRegularMeshData;
// 			DCGMeshUtils::SeparateBorders(RegularMarcher, BorderRegularMeshData);
//
// 			FMeshData BorderFullMeshData;
// 			DCGMeshUtils::SeparateBordersFull(RegularMarcher, BorderFullMeshData);
// 			
// 			FTransvoxelTransition* TransitionMarcher = new FTransvoxelTransition();
// 			TransitionMarcher->Step = Step / 2;
// 			TransitionMarcher->bParallelCompute = false;
// 			TransitionMarcher->IsoValue = IsoValue;
//
// 			TransitionMarcher->Implicit = [&Array](UE::Geometry::FVector3i Pos) -> double
// 			{
// 			FIntVector Coord(Pos.X, Pos.Y, Pos.Z);
// 			return Array.Get(Coord);
// 			};
//
// #ifdef VOXEL_USE_GRADIENT_AS_NORMAL
// 			TransitionMarcher->ImplicitGradient = [&Array](UE::Geometry::FVector3i Pos) -> FVector3f
// 				{
// 					FIntVector Coord(Pos.X, Pos.Y, Pos.Z);
// 					return -Array.GetGradient(Coord);
// 				};
// #endif
//
// 			FChunkMeshSectionBorders BorderTransitionMeshData;
// 			for (int i = 0; i < NumFaces; ++i)
// 			{
// 				TransitionMarcher->Reset();
//
// 				TransitionMarcher->Direction = static_cast<FTransvoxelTransition::EWorldZMapping>(i);
// 				UE::Geometry::FVector3i TransitionMin(0,0,0);
// 				UE::Geometry::FVector3i TransitionMax(CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE);
//
// 				TransitionMarcher->Bounds = FTransvoxelRegular::FVoxelAxisAlignedBox3i(TransitionMin, TransitionMax);
// 				TransitionMarcher->Generate();
//
// 				DCGMeshUtils::ConvertMeshShapeGeneratorToMeshData(TransitionMarcher, BorderTransitionMeshData.Borders[i]);
// 			}
// 			delete TransitionMarcher;
//
// 			FScopeLock Lock(&MeshMapSection);
// 			FChunkMaterialSection& OutMesh = OutMultiMesh.MaterialSections.FindOrAdd(MatType);
// 			OutMesh.Interior = MoveTemp(InteriorMeshData);
// 			OutMesh.BorderRegular = MoveTemp(BorderFullMeshData);
// 			for (int i = 0; i < NumFaces; ++i)
// 			{
// 				DCGMeshUtils::ConcatenateMeshes(BorderRegularMeshData.Borders[i],
// 					BorderTransitionMeshData.Borders[i],OutMesh.BorderTransitions.Borders[i]);
// 			}
// 		}
// 		delete RegularMarcher;
// 	});
// }
//
