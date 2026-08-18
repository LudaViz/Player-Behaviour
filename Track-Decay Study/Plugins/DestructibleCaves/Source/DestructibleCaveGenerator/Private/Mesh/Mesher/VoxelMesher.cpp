// Copyright Epic Games, Inc. All Rights Reserved. 
// Modified from UE original source code

#include "Mesh/Mesher/VoxelMesher.h"

#ifdef VOXEL_USE_GRADIENT_AS_NORMAL
int FVoxelMesher::edge_vertex_id(const FVoxelVector3i& Idx1, const FVoxelVector3i& Idx2,
	double F1, double F2,
	FVoxelVector3f N1, FVoxelVector3f N2)
{
	int64 hash = edge_hash(Idx1, Idx2);

	int foundvid = FindVertexID(hash);
	if (foundvid != IndexConstants::InvalidID)
	{
		return foundvid;
	}

	// ok this is a bit messy. We do not want to lock the entire hash table 
	// while we do find_iso. However it is possible that during this time we
	// are unlocked we have re-entered with the same edge. So when we
	// re-acquire the lock we need to check again that we have not already
	// computed this edge, otherwise we will end up with duplicate vertices!

	FVoxelVector3i pa = FVoxelVector3i::Zero(), pb = FVoxelVector3i::Zero();
	corner_pos(Idx1, pa);
	corner_pos(Idx2, pb);
	FVoxelVector Pos = FVoxelVector::Zero();
	find_iso(pa, pb, F1, F2, Pos);

	// === N1, N2 interpolate at Pos ===
	FVoxelVector pa_f(pa.X, pa.Y, pa.Z);
	FVoxelVector pb_f(pb.X, pb.Y, pb.Z);
	
	// Avoid division by zero
	double denom = (pb_f - pa_f).Size();
	double t = denom > 0.0 ? (Pos - pa_f).Size() / denom : 0.0;

	FVoxelVector3f Norm = FMath::Lerp(N1, N2, t).GetSafeNormal();
	
	return AppendOrFindVertexID(hash, Pos, Norm);
}

#else
int FTransvoxelTransition::edge_vertex_id(const FVoxelVector3i& Idx1, const FVoxelVector3i& Idx2, double F1, double F2)
{
	int64 hash = edge_hash(Idx1, Idx2);

	int foundvid = FindVertexID(hash);
	if (foundvid != IndexConstants::InvalidID)
	{
		return foundvid;
	}

	// ok this is a bit messy. We do not want to lock the entire hash table 
	// while we do find_iso. However it is possible that during this time we
	// are unlocked we have re-entered with the same edge. So when we
	// re-acquire the lock we need to check again that we have not already
	// computed this edge, otherwise we will end up with duplicate vertices!

	FVoxelVector3i pa = FVoxelVector3i::Zero(), pb = FVoxelVector3i::Zero();
	corner_pos(Idx1, pa);
	corner_pos(Idx2, pb);
	FVoxelVector Pos = FVoxelVector::Zero();
	find_iso(pa, pb, F1, F2, Pos);

	return AppendOrFindVertexID(hash, Pos);
}
#endif
