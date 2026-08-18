// Copyright 2025 J2K2. All Rights Reserved.

#include "Stats/GameplayStatsManager.h"

void FDestructionStats::RegisterDig(int32 NumModifiedVoxels,float VoxelSize)
{
	NumDigOperations++;

	NumVoxelsModified += NumModifiedVoxels;

	double VoxelVolumeCM = FMath::Pow(VoxelSize, 3.0);
	EstimatedDestroyedVolumeM += (NumModifiedVoxels * VoxelVolumeCM/1000000.0);
	//UE_LOG(LogChunk,Warning,TEXT("%lfM3"), EstimatedDestroyedVolumeM)
}
