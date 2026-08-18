// Copyright 2025 J2K2. All Rights Reserved.

#include "Chunk/ChunkData.h"
#include "Math/VectorRegister.h" // SIMD 벡터 타입
#include "Destruct/DestructTypes.h"
#include "Async/ParallelFor.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
struct FShapeData
{
	union
	{
		UE::Math::TVector<float> BoxExtent;
		float SphereRadius;
		float CapsuleHeight;
		float CapsuleRadius;
	} Property;

	enum EType
	{
		Box,
		Sphere,
		Capsule,
		None
	} Type;

	FShapeData()
		: Property{UE::Math::TVector<float>(0.0f, 0.0f, 0.0f)}
		  , Type(None)
	{
	}
};

// 도형-AABB 겹침 여부 검사
bool ShapeOverlapsBox(const FBox& Box, const FShapeData& ShapeData, const FTransform& ShapeTransform)
{
	const double RadiusSqr = FMath::Square(ShapeData.Property.SphereRadius);
	switch (ShapeData.Type)
	{
	case FShapeData::EType::Sphere:
		return FMath::SphereAABBIntersection(
			ShapeTransform.GetLocation(),
			RadiusSqr,
			Box);
		break;
	case FShapeData::EType::Box:
		{
			const FBox ShapeBox = FBox::BuildAABB(FVector::ZeroVector, FVector3d(ShapeData.Property.BoxExtent))
				.TransformBy(ShapeTransform);
			return ShapeBox.Intersect(Box);
			break;
		}
	default:
		return false;
	}
}

// 도형이 AABB를 완전히 포함하는지 (full inside)
bool IsBoxCompletelyInsideShape(const FBox& Box, const FShapeData& ShapeData, const FTransform& ShapeTransform)
{
	switch (ShapeData.Type)
	{
	case FShapeData::EType::Sphere:
		{
			const FVector SphereCenter = ShapeTransform.GetLocation();
			const double RadiusSqr = FMath::Square(ShapeData.Property.SphereRadius);
			UE::Math::TVector<double> Corners[8];
			Box.GetVertices(Corners);
			for (int i = 0; i < 8; ++i)
			{
				if ((Corners[i] - SphereCenter).SizeSquared() > RadiusSqr)
					return false;
			}
			return true;
		}
	case FShapeData::EType::Box:
		{
			const FBox ShapeBox = FBox::BuildAABB(FVector::ZeroVector, FVector3d(ShapeData.Property.BoxExtent))
				.TransformBy(ShapeTransform);
			return Box.IsInside(ShapeBox);
		}
	default:
		return false;
	}
}
void FChunkData::ModifyDensityRecursive(
	const FIntVector& Min, const FIntVector& Max,
	int32 Size, float VoxelSize, const FVector& ChunkOrigin,
	const FShapeData& ShapeData, const FTransform& ShapeTransform,
	const ChunkDensityArray& MaxDensityArray,
	float Strength,
	int32& NumModified,
	int32 CurrentDepth,
	int32 MaxDepth)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FChunkData::ModifyDensityRecursive)
	
	const FVector WorldMin = ChunkOrigin + FVector(Min) * VoxelSize;
	const FVector WorldMax = ChunkOrigin + FVector(Max + FIntVector(1)) * VoxelSize;
	const FBox Bounds(WorldMin, WorldMax);

	if (!ShapeOverlapsBox(Bounds, ShapeData, ShapeTransform))return;

	const bool bMaxDepth = CurrentDepth >= MaxDepth;
	const bool bFullyInside = IsBoxCompletelyInsideShape(Bounds, ShapeData, ShapeTransform);

	if (bFullyInside || bMaxDepth)
	{
		const FTransform InverseTransform = ShapeTransform.Inverse();
		float RadiusSquared = FMath::Square(ShapeData.Property.SphereRadius);

		for (auto It = MaxDensityArray.RangeBegin(Min, Max);
			It != MaxDensityArray.RangeEnd(Min, Max);
			++It)
		// for (int32 z = Min.Z; z <= Max.Z; ++z)
		// for (int32 y = Min.Y; y <= Max.Y; ++y)
		// for (int32 x = Min.X; x <= Max.X; ++x)
		{
			const FIntVector LocalCoord = It->Key;
			float MaxDensity = *It->Value;
			// const FIntVector LocalCoord(x,y,z);
			// mat_type MaterialType = MaterialData.Get(LocalCoord);

			const FVector VoxelWorldPos = ChunkOrigin + FVector(LocalCoord) * VoxelSize;

			// Shape 내부에 포함되는지 판정
			const FVector LocalPoint = InverseTransform.TransformPosition(VoxelWorldPos);

			bool bInside = false;

			switch (ShapeData.Type)
			{
			case FShapeData::EType::Sphere:
				//Square가 아닌 제곱으로 비교?
					bInside = LocalPoint.SizeSquared() <= RadiusSquared;
				break;

			case FShapeData::EType::Box:
				bInside =
					FMath::Abs(LocalPoint.X) <= ShapeData.Property.BoxExtent.X &&
					FMath::Abs(LocalPoint.Y) <= ShapeData.Property.BoxExtent.Y &&
					FMath::Abs(LocalPoint.Z) <= ShapeData.Property.BoxExtent.Z;
				break;
			default:
				// TODO : CapsuleComponent
					break;
			}

			if (!bInside) continue;

			const float OldDensity = MaxDensity;
			const float NewDensity = FMath::Clamp(OldDensity + Strength, CHUNK_DENSITY_MIN, CHUNK_DENSITY_MAX);

			if (!FMath::IsNearlyEqual(OldDensity, NewDensity, KINDA_SMALL_NUMBER))
			{
				SetDensityForAllArrays(LocalCoord, NewDensity);
				// Density[Index] = NewDensity;
				++NumModified;
			}
		}
		return;
	}

	// 계속 분할
	// 병렬화의 경우 전부 Density를 공유하고 청크 단위 병렬화로 충분해 보여 일단 보류
	const FIntVector Mid = (Min + Max) / 2;
	for (int dx = 0; dx <= 1; ++dx)
		for (int dy = 0; dy <= 1; ++dy)
			for (int dz = 0; dz <= 1; ++dz)
			{
				FIntVector ChildMin(
					dx == 0 ? Min.X : Mid.X + 1,
					dy == 0 ? Min.Y : Mid.Y + 1,
					dz == 0 ? Min.Z : Mid.Z + 1);
				FIntVector ChildMax(
					dx == 0 ? Mid.X : Max.X,
					dy == 0 ? Mid.Y : Max.Y,
					dz == 0 ? Mid.Z : Max.Z);
				ModifyDensityRecursive(
					ChildMin, ChildMax, Size, VoxelSize, ChunkOrigin,
					ShapeData, ShapeTransform, MaxDensityArray,
 					Strength, NumModified,
					CurrentDepth + 1, MaxDepth);
			}
	return;
}

mat_type FChunkData::GetMaterialType(FIntVector Coord)
{
	mat_type DominantMat = 0;
	float MaxDensity = -FLT_MAX;
	for (const auto& Pair : DensityMap)
	{
		const float D = Pair.Value.Get(Coord);
		if (D > MaxDensity)
		{
			MaxDensity = D;
			DominantMat = Pair.Key;
		}
	}
	return DominantMat;
}

int32 FChunkData::ModifyDensityRecursive_Parallel(
	const FIntVector& Min, const FIntVector& Max,
	int32 Size, float VoxelSize, const FVector& ChunkOrigin,
	const FShapeData& ShapeData, const FTransform& ShapeTransform,
	const FTransform& InverseTransform, const float& RadiusSquared,
	const ChunkDensityArray& MaxDensityArray,
	float Strength,
	TQueue<FDestructionEvent,EQueueMode::Mpsc>* OutDestructionEvents=nullptr,
	int32 CurrentDepth,
	int32 MaxDepth)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FChunkData::ModifyDensityRecursive_Parallel)
	int32 LocalModified = 0;

	const FVector WorldMin = ChunkOrigin + FVector(Min) * VoxelSize;
	const FVector WorldMax = ChunkOrigin + FVector(Max + FIntVector(1)) * VoxelSize;
	const FBox Bounds(WorldMin, WorldMax);

	if (!ShapeOverlapsBox(Bounds, ShapeData, ShapeTransform)) return 0;

	const bool bMaxDepth = CurrentDepth >= MaxDepth;
	const bool bFullyInside = IsBoxCompletelyInsideShape(Bounds, ShapeData, ShapeTransform);

	//TODO
	//Full inside 일 때는 검사 생략
	const float FadeThickness = VoxelSize * 1; // 부드러운 경계 두께 (원하는 값으로 조절)

	//전부 포함이면 8분할 할 필요가 없이 바로 처리
	//일정 깊이 이상이면 분할하는게 더 오래 걸리기에 bMaxDepth부터는 8분할로 나누지 않음
	if (bFullyInside || bMaxDepth)
	{
	    for (auto It = MaxDensityArray.RangeBegin(Min, Max);
	        It != MaxDensityArray.RangeEnd(Min, Max);
	        ++It)
	    {
	        const FIntVector LocalCoord = It->Key;
	        float MaxDensity = *It->Value;

	        float t = 1.f; // 기본은 완전 적용
			//전부 포함되지 않은 경우 데이터의 포함 여부 확인
	        if (!bFullyInside)
	        {
	            const FVector VoxelWorldPos = ChunkOrigin + FVector(LocalCoord) * VoxelSize;
	            const FVector LocalPoint = InverseTransform.TransformPosition(VoxelWorldPos);

	            bool bInside = false;
	            switch (ShapeData.Type)
	            {
	            case FShapeData::EType::Sphere:
	            {
	                float Dist = LocalPoint.Size();
	            	float Radius = FMath::Sqrt(RadiusSquared);
	                float FadeStart = Radius - FadeThickness;
	                float FadeEnd = Radius;
	                t = FMath::Clamp((FadeEnd - Dist) / FadeThickness, 0.f, 1.f);
	                bInside = Dist <= Radius;
	                // 부드러운 경계: t=1은 중심부, t=0은 외곽
	                break;
	            }
	            case FShapeData::EType::Box:
	            {
	                FVector AbsLocal = LocalPoint.GetAbs();
	                FVector BoxExtent = FVector(ShapeData.Property.BoxExtent);
	                FVector FadeStart = BoxExtent - FVector(FadeThickness);
	                FVector tVec = (FadeStart - AbsLocal) / FadeThickness;
	                tVec.X = FMath::Clamp(tVec.X, 0.f, 1.f);
	                tVec.Y = FMath::Clamp(tVec.Y, 0.f, 1.f);
	                tVec.Z = FMath::Clamp(tVec.Z, 0.f, 1.f);
	                t = FMath::Min3(tVec.X, tVec.Y, tVec.Z); // 세 방향 중 가장 바깥쪽 기준으로
	                bInside = 
	                    AbsLocal.X <= BoxExtent.X &&
	                    AbsLocal.Y <= BoxExtent.Y &&
	                    AbsLocal.Z <= BoxExtent.Z;
	                break;
	            }
	            default:
	                break;
	            }
				//만약 포함되지 않으면 스킵
	            if (!bInside)
	                continue;
	        }

	        // Soft transition
	        float BlendedStrength = Strength * t;
	        const float OldDensity = MaxDensity;
	        const float NewDensity = FMath::Clamp(OldDensity + BlendedStrength, CHUNK_DENSITY_MIN, CHUNK_DENSITY_MAX);

	        if (!FMath::IsNearlyEqual(OldDensity, NewDensity, KINDA_SMALL_NUMBER))
	        {
	        	float Delta = NewDensity - OldDensity;
	        	TFunction<float(mat_type, float)> Function = [=](mat_type Type, float f) -> float
	        	{
	        		float Result = f + BlendedStrength * ( 1 - Type / 256.f);
	        		return FMath::Clamp(Result, -1.f, 1.f);	
	        		// return f + Delta * FMath::Pow((float)(Type + 1), 0.7);
	        	};
	        	if (OutDestructionEvents)
	        	{
	        		FDestructionEvent Event;
	        		Event.VoxelWorldPosition = ChunkOrigin + FVector(LocalCoord) * VoxelSize;
	        		Event.VoxelCoord=LocalCoord;
	        		Event.OldDensity = OldDensity;
	        		Event.NewDensity = NewDensity;
	        		Event.MaterialType = GetMaterialType(LocalCoord);

	        		OutDestructionEvents->Enqueue(Event);
	        		
	        	}
	        	ApplyForAllArrays(LocalCoord, Function);

	            // SetDensityForAllArrays(LocalCoord, NewDensity);
	        	// if (!FMath::IsNearlyEqual(OldDensity, NewDensity, 0.5f))
	        	{
					++LocalModified;
	        	}
	        }
	    }
	    return LocalModified;
	}
	else
	{
		// 맨 처음 8분할로 공간을 잘라서 병렬로 실행
		const FIntVector Mid = (Min + Max) / 2;
		//8분할만
		if (CurrentDepth==0)
		{
			TArray<int32> LocalCounts;
			//LocalCounts.Init(0, 8);
			LocalCounts.SetNumUninitialized(8);
			ParallelFor(8, [&](int32 i)
			{
				const int dx = (i & 1);
				const int dy = (i >> 1) & 1;
				const int dz = (i >> 2) & 1;

				const FIntVector ChildMin(
					dx == 0 ? Min.X : Mid.X + 1,
					dy == 0 ? Min.Y : Mid.Y + 1,
					dz == 0 ? Min.Z : Mid.Z + 1);

				const FIntVector ChildMax(
					dx == 0 ? Mid.X : Max.X,
					dy == 0 ? Mid.Y : Max.Y,
					dz == 0 ? Mid.Z : Max.Z);

				// 쓰레드마다 독립적인 영역만 수정함 (index 충돌 없음 보장)

				LocalCounts[i] = ModifyDensityRecursive_Parallel(
					ChildMin, ChildMax, Size, VoxelSize, ChunkOrigin,
					ShapeData, ShapeTransform, InverseTransform, RadiusSquared, MaxDensityArray,
					Strength, OutDestructionEvents,CurrentDepth + 1, MaxDepth);
			});

			for (int32 Count : LocalCounts)
				LocalModified += Count;
		}
		else
		{
			//그 외 경우에는 해당 쓰레드에서 8분할로 나눠서 실행
			for (int dx = 0; dx <= 1; ++dx)
				for (int dy = 0; dy <= 1; ++dy)
					for (int dz = 0; dz <= 1; ++dz)
					{
				const FIntVector ChildMin(
					dx == 0 ? Min.X : Mid.X + 1,
					dy == 0 ? Min.Y : Mid.Y + 1,
					dz == 0 ? Min.Z : Mid.Z + 1);

				const FIntVector ChildMax(
					dx == 0 ? Mid.X : Max.X,
					dy == 0 ? Mid.Y : Max.Y,
					dz == 0 ? Mid.Z : Max.Z);
				LocalModified += ModifyDensityRecursive_Parallel(
					ChildMin, ChildMax, Size, VoxelSize, ChunkOrigin,
					ShapeData, ShapeTransform, InverseTransform, RadiusSquared, MaxDensityArray,
					Strength, OutDestructionEvents,CurrentDepth + 1, MaxDepth);
			}
		}
		return LocalModified;
	}
}


int32 FChunkData::ModifyVoxelsBySphere(const FSphere& Sphere, float VoxelSize, const FIntVector& ChunkCoord,
	float Strength,TQueue<FDestructionEvent,EQueueMode::Mpsc>* OutDestructionEvents)
{/*
	if (!ShapeComponent)
		return false;*/

	//PROFILE_BEGIN("FChunkData::ModifyVoxelsByShape")
	
	const FVector ChunkOrigin = FVector(ChunkCoord) * (CHUNK_SIZE * VoxelSize);
	int32 NumModified = 0;
	FShapeData ShapeData;
	ShapeData.Type = FShapeData::EType::Sphere;
	ShapeData.Property.SphereRadius = Sphere.W;

	/*if (const USphereComponent* Sphere = Cast<USphereComponent>(ShapeComponent))
	{
		ShapeData.Type = FShapeData::EType::Sphere;
		ShapeData.Property.SphereRadius = Sphere->GetUnscaledSphereRadius();
	}
	else if (const UBoxComponent* Box = Cast<UBoxComponent>(ShapeComponent))
	{
		ShapeData.Type = FShapeData::EType::Box;
		ShapeData.Property.BoxExtent = UE::Math::TVector<float>(Box->GetUnscaledBoxExtent());
	}
	else
	{
		ShapeData.Type = FShapeData::EType::None;
	}

	const FTransform ShapeTransform = ShapeComponent->GetComponentTransform();*/

	/**
	 * 아직 테스트중이기 때문에 매크로로 해놓았습니다.
	 * 속도는 Parallel_SIMD > Parallel > SIMD > Serial 순입니다.
	 */
	// #define FChunkData_Modify_Serial
	#define FChunkData_Modify_Parallel
	//#define FChunkData_Modify_SIMD
	//#define FChunkData_Modify_Parallel_SIMD

	#define Recursive_Octree
#ifdef FChunkData_Modify_Serial
	/*
	float RadiusSquared = FMath::Square(ShapeData.Property.SphereRadius);
	const FTransform InverseTransform = ShapeTransform.Inverse();*/
#ifdef Recursive_Octree
	ModifyDensityRecursive(
		FIntVector(0, 0, 0),                 // Min
		FIntVector(Size, Size, Size),        // Max
		Size,                                // ChunkSize
		VoxelSize,                           // Voxel size (world unit per voxel)
		ChunkOrigin,                         // Origin of this chunk in world space
		ShapeData,                           // Shape information (Sphere/Box)
		ShapeTransform,                      // World transform of the shape
		Density,                             // Reference to voxel density array
		Strength,                            // Density modification amount
		[this](int32 Index, float NewDensity) {
			this->SetDensityByIndex(Index, NewDensity);
		},                                   // SetDensityByIndex lambda
		[this](const FIntVector& Coord) {
			return this->CoordToIndex(Coord);
		},                                   // CoordToIndex lambda
		NumModified                          // OUT: Modified voxel count
	);
#else
	for (int32 z = 0; z <= Size; ++z)
		for (int32 y = 0; y <= Size; ++y)
			for (int32 x = 0; x <= Size; ++x)
			{
				const FIntVector LocalCoord(x, y, z);
				const int32 Index = CoordToIndex(LocalCoord);
				if (!Density.IsValidIndex(Index))
					continue;
	
				const FVector VoxelWorldPos = ChunkOrigin + FVector(x, y, z) * VoxelSize;
	
				// Shape 내부에 포함되는지 판정
				const FVector LocalPoint = InverseTransform.TransformPosition(VoxelWorldPos);
	
				bool bInside = false;
	
				switch (ShapeData.Type)
				{
				case FShapeData::EType::Sphere:
					bInside = LocalPoint.SizeSquared() <= RadiusSquared;
					break;

				case FShapeData::EType::Box:
					bInside =
						FMath::Abs(LocalPoint.X) <= ShapeData.Property.BoxExtent.X &&
						FMath::Abs(LocalPoint.Y) <= ShapeData.Property.BoxExtent.Y &&
						FMath::Abs(LocalPoint.Z) <= ShapeData.Property.BoxExtent.Z;
					break;
				default:
					// TODO : CapsuleComponent
					return false;
				}
	
				if (!bInside) continue;
	
				const float OldDensity = Density[Index];
				const float NewDensity = FMath::Clamp(OldDensity + Strength, -1.0f, 1.0f);
	
				if (!FMath::IsNearlyEqual(OldDensity, NewDensity, KINDA_SMALL_NUMBER))
				{
					SetDensityByIndex(Index,NewDensity);
					// Density[Index] = NewDensity;
					++NumModified;
				}
			}
#endif
#endif

#ifdef FChunkData_Modify_Parallel
#ifdef Recursive_Octree
	NumModified += ModifyDensityRecursive_Parallel(
		FIntVector(-CHUNK_MARGIN, -CHUNK_MARGIN, -CHUNK_MARGIN), // Min
		FIntVector(CHUNK_SIZE + CHUNK_MARGIN, CHUNK_SIZE + CHUNK_MARGIN, CHUNK_SIZE + CHUNK_MARGIN), // Max
		CHUNK_SIZE, // ChunkSize
		VoxelSize, // Voxel size (world unit per voxel)
		ChunkOrigin, // Origin of this chunk in world space
		ShapeData, // Shape information (Sphere/Box)
		FTransform(Sphere.Center), // World transform of the shape
		FTransform(Sphere.Center).Inverse(),
		FMath::Square(ShapeData.Property.SphereRadius),
		GetMaxDensity(),
		Strength,
		OutDestructionEvents
		
	);
#else
	float RadiusSquared = FMath::Square(ShapeData.Property.SphereRadius);

	TArray<int32> NumModifiedThd;
	NumModifiedThd.SetNum(Size + 1);
	// z축을 병렬처리
	// TODO : 이미 이 함수도 ParallelFor로 실행되는데, 이거까지 그래야하는가?
	ParallelFor(Size + 1, [&](int32 z) -> void
	{
		int32 LocalNumModified = 0;
		for (int32 y = 0; y <= Size; ++y)
		{
			for (int32 x = 0; x <= Size; ++x)
			{
				const FIntVector LocalCoord(x, y, z);
				const int32 Index = CoordToIndex(LocalCoord);
				if (!Density.IsValidIndex(Index))
					continue;
	
				const FVector VoxelWorldPos = ChunkOrigin + FVector(x, y, z) * VoxelSize;
	
				// Shape 내부에 포함되는지 판정
				const FVector LocalPoint = ShapeTransform.InverseTransformPosition(VoxelWorldPos);
	
				bool bInside = false;
	
				switch (ShapeData.Type)
				{
				case FShapeData::EType::Sphere:
					{
						float DistSq = LocalPoint.X * LocalPoint.X + LocalPoint.Y * LocalPoint.Y + LocalPoint.Z * LocalPoint.Z;
						bInside = DistSq <= RadiusSquared;
						break;
					}

				case FShapeData::EType::Box:
					bInside =
						FMath::Abs(LocalPoint.X) <= ShapeData.Property.BoxExtent.X &&
						FMath::Abs(LocalPoint.Y) <= ShapeData.Property.BoxExtent.Y &&
						FMath::Abs(LocalPoint.Z) <= ShapeData.Property.BoxExtent.Z;
					break;
				default:
					return;
				}
	
				if (!bInside) continue;
	
				const float OldDensity = Density[Index];
				const float NewDensity = FMath::Clamp(OldDensity + Strength, -1.0f, 1.0f);
	
				if (!FMath::IsNearlyEqual(OldDensity, NewDensity, KINDA_SMALL_NUMBER))
				{
					SetDensityByIndex(Index, NewDensity);
					++LocalNumModified;
				}
			}
		}
		NumModifiedThd[z] = LocalNumModified;
	});
	
	for (int32 Num : NumModifiedThd)
	{
		NumModified += Num;
	}
#endif
#endif

#ifdef FChunkData_Modify_SIMD
	float RadiusSquared = FMath::Square(ShapeData.Property.SphereRadius);
	VectorRegister4Float RadiusSq = VectorLoadFloat1(&RadiusSquared);

    // SIMD로 한 번에 4개씩 처리
    const int32 NumVoxels = Density.Num();
	constexpr int32 Step = 4;
	// 포인터로 넣어야 할때 사용
	// 어차피 최적화 안될듯...
	constexpr float ThresholdFloat = UE_KINDA_SMALL_NUMBER;
	constexpr float PlusOne = 1.f;
	constexpr float MinusOne = -1.f;

	int32 i = 0;
	for (; i <= NumVoxels - Step; i += Step)
	{
	    FVector VoxelWorldPos[Step];
	    UE::Math::TVector<float> LocalPoints[Step];

	    for (int32 j = 0; j < Step; ++j)
	    {
	        FIntVector coord = IndexToCoord(i + j);
	        VoxelWorldPos[j] = ChunkOrigin + FVector(coord) * VoxelSize;
	        LocalPoints[j] = UE::Math::TVector<float>(ShapeTransform.InverseTransformPosition(VoxelWorldPos[j]));
	    }

		VectorRegister4Float Mask;
		
		if (ShapeData.Type == FShapeData::EType::Sphere)
		{
			VectorRegister4Float Px = MakeVectorRegister(LocalPoints[0].X, LocalPoints[1].X, LocalPoints[2].X, LocalPoints[3].X);
			VectorRegister4Float Py = MakeVectorRegister(LocalPoints[0].Y, LocalPoints[1].Y, LocalPoints[2].Y, LocalPoints[3].Y);
			VectorRegister4Float Pz = MakeVectorRegister(LocalPoints[0].Z, LocalPoints[1].Z, LocalPoints[2].Z, LocalPoints[3].Z);

			VectorRegister4Float Sq = VectorAdd(
				VectorAdd(
					VectorMultiply(Px, Px),
					VectorMultiply(Py, Py)
				),
				VectorMultiply(Pz, Pz)
			);
			Mask = VectorCompareLE(Sq, RadiusSq);
		}
		else if (ShapeData.Type == FShapeData::EType::Box)
		{
			UE::Math::TVector<float> Ext = ShapeData.Property.BoxExtent;

			VectorRegister4Float AbsX = VectorAbs(MakeVectorRegister(LocalPoints[0].X, LocalPoints[1].X, LocalPoints[2].X, LocalPoints[3].X));
			VectorRegister4Float AbsY = VectorAbs(MakeVectorRegister(LocalPoints[0].Y, LocalPoints[1].Y, LocalPoints[2].Y, LocalPoints[3].Y));
			VectorRegister4Float AbsZ = VectorAbs(MakeVectorRegister(LocalPoints[0].Z, LocalPoints[1].Z, LocalPoints[2].Z, LocalPoints[3].Z));

			VectorRegister4Float ExtX = VectorLoadFloat1(&Ext.X);
			VectorRegister4Float ExtY = VectorLoadFloat1(&Ext.Y);
			VectorRegister4Float ExtZ = VectorLoadFloat1(&Ext.Z);

			VectorRegister4Float MaskX = VectorCompareLE(AbsX, ExtX);
			VectorRegister4Float MaskY = VectorCompareLE(AbsY, ExtY);
			VectorRegister4Float MaskZ = VectorCompareLE(AbsZ, ExtZ);

			Mask = VectorBitwiseAnd(VectorBitwiseAnd(MaskX, MaskY), MaskZ);
		}
		
		VectorRegister4Float OldDensity = MakeVectorRegister(
			Density[i + 0], Density[i + 1], Density[i + 2], Density[i + 3]
		);

		VectorRegister4Float StrengthV = VectorLoadFloat1(&Strength);

		VectorRegister4Float Added = VectorAdd(OldDensity, StrengthV);
		VectorRegister4Float ClampedLow = VectorMax(Added, VectorLoadFloat1(&MinusOne));
		VectorRegister4Float NewDensity = VectorMin(ClampedLow, VectorLoadFloat1(&PlusOne));

		VectorRegister4Float Diff = VectorAbs(VectorSubtract(OldDensity, NewDensity));
		VectorRegister4Float Threshold = VectorLoadFloat1(&ThresholdFloat);
		VectorRegister4Float NearlyEqualMask = VectorCompareGE(Diff, Threshold);

		VectorRegister4Float WriteMask = VectorBitwiseAnd(Mask, NearlyEqualMask);

		alignas(16) float WriteMaskArray[4];
		VectorStore(WriteMask, WriteMaskArray);
	    				
		alignas(16) float NewDensityArray[4];
		VectorStore(NewDensity, NewDensityArray);
	}

	for (int32 j = i; j < NumVoxels; ++j)
	{
		FIntVector coord = IndexToCoord(j);
		FVector VoxelWorldPos = ChunkOrigin + FVector(coord) * VoxelSize;
		FVector LocalPoint = ShapeTransform.InverseTransformPosition(VoxelWorldPos);

		bool bInside = false;
		if (ShapeData.Type == FShapeData::EType::Sphere)
		{
			float DistSq = LocalPoint.X * LocalPoint.X + LocalPoint.Y * LocalPoint.Y + LocalPoint.Z * LocalPoint.Z;
			bInside = DistSq <= RadiusSquared;
		}
		else if (ShapeData.Type == FShapeData::EType::Box)
		{
			bInside =
				FMath::Abs(LocalPoint.X) <= ShapeData.Property.BoxExtent.X &&
				FMath::Abs(LocalPoint.Y) <= ShapeData.Property.BoxExtent.Y &&
				FMath::Abs(LocalPoint.Z) <= ShapeData.Property.BoxExtent.Z;
		}

		if (!bInside)
			continue;

		const float OldDensity = Density[j];
		const float NewDensity = FMath::Clamp(OldDensity + Strength, -1.f, 1.f);
		if (!FMath::IsNearlyEqual(OldDensity, NewDensity, KINDA_SMALL_NUMBER))
		{
			SetDensityByIndex(j, NewDensity);
			NumModified++;
		}
	}
#endif

#ifdef FChunkData_Modify_Parallel_SIMD
	const int32 NumVoxels = Density.Num();
	
	float RadiusSquared = FMath::Square(ShapeData.Property.SphereRadius);
	VectorRegister4Float RadiusSq = VectorLoadFloat1(&RadiusSquared);

	constexpr float ThresholdFloat = UE_KINDA_SMALL_NUMBER;
	constexpr float PlusOne = 1.f;
	constexpr float MinusOne = -1.f;

	// 병렬 처리할 batch 수 계산
	constexpr int32 NumThreads = 32;
	const int32 NumVoxelsPerThread = (NumVoxels / (NumThreads - 1) / 4) * 4; // 4개 단위로

	TArray<int32> NumModifiedThd;
	NumModifiedThd.SetNum(NumThreads);
	
	ParallelFor(NumThreads, [&](int32 ThreadIdx)
	{
		int32 LocalNumThread = 0;
		const int32 StartVoxelIdx = ThreadIdx * NumVoxelsPerThread;
		const int32 EndVoxelIdx = FMath::Min(StartVoxelIdx + NumVoxelsPerThread, NumVoxels - 4);
		
		for (int32 VoxelIdx = StartVoxelIdx; VoxelIdx < EndVoxelIdx; VoxelIdx += 4)
		{
			FVector VoxelWorldPos[4];
			UE::Math::TVector<float> LocalPoints[4];

			for (int i = 0; i < 4; i++)
			{
				FIntVector coord = IndexToCoord(VoxelIdx + i);
				VoxelWorldPos[i] = ChunkOrigin + FVector(coord) * VoxelSize;
				LocalPoints[i] = UE::Math::TVector<float>(ShapeTransform.InverseTransformPosition(VoxelWorldPos[i]));
			}

			VectorRegister4Float Mask;
			
			switch (ShapeData.Type)
			{
			case FShapeData::EType::Sphere:
				VectorRegister4Float Px = MakeVectorRegister(LocalPoints[0].X, LocalPoints[1].X, LocalPoints[2].X, LocalPoints[3].X);
				VectorRegister4Float Py = MakeVectorRegister(LocalPoints[0].Y, LocalPoints[1].Y, LocalPoints[2].Y, LocalPoints[3].Y);
				VectorRegister4Float Pz = MakeVectorRegister(LocalPoints[0].Z, LocalPoints[1].Z, LocalPoints[2].Z, LocalPoints[3].Z);

				VectorRegister4Float Sq = VectorAdd(
					VectorAdd(
						VectorMultiply(Px, Px),
						VectorMultiply(Py, Py)
					),
					VectorMultiply(Pz, Pz)
				);
				Mask = VectorCompareLE(Sq, RadiusSq);
				break;
			
			case FShapeData::EType::Box:
				UE::Math::TVector<float> Ext = ShapeData.Property.BoxExtent;

				VectorRegister4Float AbsX = VectorAbs(MakeVectorRegister(LocalPoints[0].X, LocalPoints[1].X, LocalPoints[2].X, LocalPoints[3].X));
				VectorRegister4Float AbsY = VectorAbs(MakeVectorRegister(LocalPoints[0].Y, LocalPoints[1].Y, LocalPoints[2].Y, LocalPoints[3].Y));
				VectorRegister4Float AbsZ = VectorAbs(MakeVectorRegister(LocalPoints[0].Z, LocalPoints[1].Z, LocalPoints[2].Z, LocalPoints[3].Z));

				VectorRegister4Float ExtX = VectorLoadFloat1(&Ext.X);
				VectorRegister4Float ExtY = VectorLoadFloat1(&Ext.Y);
				VectorRegister4Float ExtZ = VectorLoadFloat1(&Ext.Z);

				VectorRegister4Float MaskX = VectorCompareLE(AbsX, ExtX);
				VectorRegister4Float MaskY = VectorCompareLE(AbsY, ExtY);
				VectorRegister4Float MaskZ = VectorCompareLE(AbsZ, ExtZ);

				Mask = VectorBitwiseAnd(VectorBitwiseAnd(MaskX, MaskY), MaskZ);
				break;
				
			default:
				return;
			}
			
			VectorRegister4Float OldDensity = MakeVectorRegister(
				Density[VoxelIdx + 0], Density[VoxelIdx + 1], Density[VoxelIdx + 2], Density[VoxelIdx + 3]
			);

			VectorRegister4Float StrengthV = VectorLoadFloat1(&Strength);

			VectorRegister4Float Added = VectorAdd(OldDensity, StrengthV);
			VectorRegister4Float ClampedLow = VectorMax(Added, VectorLoadFloat1(&MinusOne));
			VectorRegister4Float NewDensity = VectorMin(ClampedLow, VectorLoadFloat1(&PlusOne));

			VectorRegister4Float Diff = VectorAbs(VectorSubtract(OldDensity, NewDensity));
			VectorRegister4Float Threshold = VectorLoadFloat1(&ThresholdFloat);
			VectorRegister4Float NearlyEqualMask = VectorCompareGE(Diff, Threshold);

			VectorRegister4Float WriteMask = VectorBitwiseAnd(Mask, NearlyEqualMask);

			alignas(16) float WriteMaskArray[4];
			VectorStore(WriteMask, WriteMaskArray);
	    					
			alignas(16) float NewDensityArray[4];
			VectorStore(NewDensity, NewDensityArray);

			for (int i = 0; i < 4; i++)
			{
				if (std::bit_cast<uint32>(WriteMaskArray[i]) != 0)
				{
					SetDensityByIndex(VoxelIdx + i, NewDensityArray[i]);
					LocalNumThread++;
				}
			}
		} // for (int32 VoxelIdx = StartVoxelIdx; VoxelIdx < EndVoxelIdx; VoxelIdx += 4)

	});

	// 남은 나머지 스칼라 처리
	const int32 StartIdx = FMath::Min((NumThreads - 1) + NumVoxelsPerThread, NumVoxels - 4);
	
	for (int32 j = StartIdx; j < NumVoxels; ++j)
	{
		FIntVector coord = IndexToCoord(j);
		FVector VoxelWorldPos = ChunkOrigin + FVector(coord) * VoxelSize;
		FVector LocalPoint = ShapeTransform.InverseTransformPosition(VoxelWorldPos);

		bool bInside = false;
		switch (ShapeData.Type)
		{
		case FShapeData::EType::Sphere:
			{
				float DistSq = LocalPoint.X * LocalPoint.X + LocalPoint.Y * LocalPoint.Y + LocalPoint.Z * LocalPoint.Z;
				bInside = DistSq <= RadiusSquared;
				break;
			}

		case FShapeData::EType::Box:
			bInside =
				FMath::Abs(LocalPoint.X) <= ShapeData.Property.BoxExtent.X &&
				FMath::Abs(LocalPoint.Y) <= ShapeData.Property.BoxExtent.Y &&
				FMath::Abs(LocalPoint.Z) <= ShapeData.Property.BoxExtent.Z;
			break;
		default:
			return false;
		}

		if (!bInside)
			continue;

		const float OldDensity = Density[j];
		const float NewDensity = FMath::Clamp(OldDensity + Strength, -1.f, 1.f);
		if (!FMath::IsNearlyEqual(OldDensity, NewDensity, KINDA_SMALL_NUMBER))
		{
			SetDensityByIndex(j, NewDensity);
			LocalNumThread++;
		}
		NumModifiedThd[ThreadIdx] = LocalNumThread;
	}

	for (int32 Num : NumModifiedThd)
	{
		NumModified += Num;
	}
#endif

	//PROFILE_END("FChunkData::ModifyVoxelsByShape")

	return NumModified;
}

void FChunkData::SerializeToBytes(TArray<uint8>& OutBytes) const
{
	OutBytes.Reset();
	FMemoryWriter Writer(OutBytes, /*bIsPersistent=*/ true);

	const uint32 Version = 1;
	Writer << const_cast<uint32&>(Version);

	// DensityMap 저장
	int32 DensityCount = DensityMap.Num();
	Writer << DensityCount;

	for (const auto& Pair : DensityMap)
	{
		mat_type MaterialType = Pair.Key;
		Writer << MaterialType;

		const ChunkDensityArray& Array = Pair.Value;
		const int32 NumElements = Array.GetNumElements();
		const float* RawPtr = Array.GetData().GetData();

		Writer.Serialize((void*)RawPtr, sizeof(float) * NumElements);
	}

	// DataMap 저장
	int32 DataCount = DataMap.Num();
	Writer << DataCount;

	for (const auto& Pair : DataMap)
	{
		mat_type MaterialType = Pair.Key;
		Writer << MaterialType;

		const ChunkVoxelDataArray& Array = Pair.Value;
		const int32 NumElements = Array.GetNumElements();
		const FVoxelData* RawPtr = Array.GetData().GetData();

		Writer.Serialize((void*)RawPtr, sizeof(FVoxelData) * NumElements);
	}
}
void FChunkData::DeserializeFromBytes(const TArray<uint8>& InBytes)
{
	FMemoryReader Reader(InBytes, /*bIsPersistent=*/ true);

	uint32 Version;
	Reader << Version;

	// DensityMap 읽기
	int32 DensityCount = 0;
	Reader << DensityCount;
	DensityMap.Empty(DensityCount);

	for (int32 i = 0; i < DensityCount; ++i)
	{
		mat_type MaterialType;
		Reader << MaterialType;

		ChunkDensityArray Array;
		float* RawPtr = Array.GetData().GetData();
		const int32 NumElements = Array.GetNumElements();

		Reader.Serialize(RawPtr, sizeof(float) * NumElements);
		DensityMap.Add(MaterialType, Array);
	}

	// DataMap 읽기
	int32 DataCount = 0;
	Reader << DataCount;
	DataMap.Empty(DataCount);

	for (int32 i = 0; i < DataCount; ++i)
	{
		mat_type MaterialType;
		Reader << MaterialType;

		ChunkVoxelDataArray Array;
		FVoxelData* RawPtr = Array.GetData().GetData();
		const int32 NumElements = Array.GetNumElements();

		Reader.Serialize(RawPtr, sizeof(FVoxelData) * NumElements);
		DataMap.Add(MaterialType, Array);
	}
}



TSet<uint8> FChunkData::CollectUsedMaterialTypes() const
{
	TArray<mat_type> KeyArray;
	DensityMap.GenerateKeyArray(KeyArray);

	return TSet<mat_type>(KeyArray);
}

ChunkDensityArray FChunkData::GetMaxDensity() const
{
	int32 NumElements = ChunkDensityArray::NumElements;
	ChunkDensityArray MaxDensity;
	MaxDensity.GetData().Init(CHUNK_DENSITY_MIN, NumElements);
	
	for (const auto& Pair : DensityMap)
	{
		for (int32 i = 0; i < NumElements; ++i)
		{
			MaxDensity.GetData()[i] = FMath::Max(Pair.Value.GetData()[i], MaxDensity.GetData()[i]);
		}
	}

	return MoveTemp(MaxDensity);
}
