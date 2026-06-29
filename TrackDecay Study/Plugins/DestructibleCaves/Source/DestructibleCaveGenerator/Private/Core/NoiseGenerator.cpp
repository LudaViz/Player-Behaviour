// Copyright 2025 J2K2. All Rights Reserved.

#include "Core/NoiseGenerator.h"
#include "Chunk/ChunkData.h"
#include "Container/HeightArray.h"
#include "Container/ThreadSafeMap.h"
#include "Core/Noise/TerrainData.h"
#include "Math/NoiseMathUtility.h"

DCG_DISABLE_OPTIMIZATION

UNoiseGenerator::UNoiseGenerator()
{}

//TSharedPtr<FastNoiseLite> NoiseInstance;
void UNoiseGenerator::Initialize(TArray<FTerrainData*> InTerrainDatas)
{
	for (auto Data : InTerrainDatas)
	{
		if (Data->UseNoise2DUpperBound)
		{
			GetNoiseInstance(Data->Noise2DUpperBound);
		}
		if (Data->UseNoise2DLowerBound)
		{
			GetNoiseInstance(Data->Noise2DLowerBound);
		}
		if (Data->UseNoise3D)
		{
			GetNoiseInstance(Data->Noise3D);
		}
		
		TerrainDatas.Add(Data);
	}

	if (TerrainDatas.IsEmpty())
	{
		UE_LOG(LogDCG, Error, TEXT("Terrain Data is not set."))
	}
}

uint32 UNoiseGenerator::GetSettingsHash(const FNoiseParameter& InNoiseParameter) const
{
    uint32 Hash = FCrc::MemCrc32(&InNoiseParameter, sizeof(FNoiseParameter));
    return Hash;
}

TSharedPtr<FNoiseMathUtility> UNoiseGenerator::GetNoiseInstance(const FNoiseParameter& InNoiseParameter) const
{
    const uint32 Hash = GetSettingsHash(InNoiseParameter);
    if (NoiseInstances.Contains(Hash))
    {
        return NoiseInstances[Hash];
    }
	
	//병렬 처리 시 이 구간에 오면 안됨
    TSharedPtr<FNoiseMathUtility> NewInstance = MakeShared<FNoiseMathUtility>(InNoiseParameter);
    NoiseInstances.Add(Hash, NewInstance);
    return NewInstance;
}

void UNoiseGenerator::FillChunkData(FChunkData* ChunkData, const FIntVector& ChunkCoord) const
{
	if (!ChunkData) return;
	
	TRACE_CPUPROFILER_EVENT_SCOPE(UNoiseGenerator::FillChunkData)

	if (TerrainDatas.IsEmpty())
	{
		UE_LOG(LogDCG, Error, TEXT("TerrainDataInfo.DataTable is null! Please assign a valid DataTable asset to TerrainManager."))
		return;
	}
	
	FIntVector2 ChunkCoord2D = {ChunkCoord.X, ChunkCoord.Y};
	using HeightMapType = TLinearArray2D<CHUNK_SIZE + 1, CHUNK_MARGIN, float>;
	static TThreadSafeMap<FHeightMapKey, HeightMapType> HeightMapMap;
	
	TArray<HeightMapType> UpperHeightMaps;
	TArray<HeightMapType> LowerHeightMaps;
	UpperHeightMaps.SetNum(TerrainDatas.Num());
	LowerHeightMaps.SetNum(TerrainDatas.Num());

	for (int32 i = 0; i < TerrainDatas.Num(); ++i)
	{
		auto& Data = TerrainDatas[i];
		if (Data->UseNoise2DUpperBound)
		{
			const TSharedPtr<FNoiseMathUtility>* FoundInstance = NoiseInstances.Find(GetSettingsHash(Data->Noise2DUpperBound));
			if (FoundInstance)
			{
				uint32 ParamHash = GetSettingsHash(Data->Noise2DUpperBound);
				FHeightMapKey MapKey(ChunkCoord2D, ParamHash);
				
				FScopeLock Lock(&HeightMapMap.GetCriticalSection());
				if (HeightMapType* FoundMap = HeightMapMap.Find(MapKey))
				{
					UpperHeightMaps[i] = *FoundMap;
				}
				else
				{
					HeightMapType NewHeightMap;
					for (int y = -CHUNK_MARGIN; y <= CHUNK_SIZE + CHUNK_MARGIN; ++y)
					{
						for (int x = -CHUNK_MARGIN; x <= CHUNK_SIZE + CHUNK_MARGIN; ++x)
						{
							FIntVector ChunkOrigin = ChunkCoord * CHUNK_SIZE;
							FIntVector LocalPosition = FIntVector(x,y,0);
							FIntVector GridPos = ChunkOrigin + LocalPosition;
							float SurfaceHeight = (*FoundInstance)->ModifiedPerlinNoise2D(FVector2D(GridPos.X, GridPos.Y));
							NewHeightMap.Set(LocalPosition.X, LocalPosition.Y, SurfaceHeight);
						}
					}
					UpperHeightMaps[i] = NewHeightMap;
					HeightMapMap.Add(MapKey, NewHeightMap);
				}
			}
		}
		if (Data->UseNoise2DLowerBound)
		{
			const TSharedPtr<FNoiseMathUtility>* FoundInstance = NoiseInstances.Find(GetSettingsHash(Data->Noise2DLowerBound));
			if (FoundInstance)
			{
				uint32 ParamHash = GetSettingsHash(Data->Noise2DLowerBound);
				FHeightMapKey MapKey(ChunkCoord2D, ParamHash);

				FScopeLock Lock(&HeightMapMap.GetCriticalSection());
				if (HeightMapType* FoundMap = HeightMapMap.Find(MapKey))
				{
					LowerHeightMaps[i] = *FoundMap;
				}
				else
				{
					HeightMapType NewHeightMap;
					for (int y = -CHUNK_MARGIN; y <= CHUNK_SIZE + CHUNK_MARGIN; ++y)
					{
						for (int x = -CHUNK_MARGIN; x <= CHUNK_SIZE + CHUNK_MARGIN; ++x)
						{
							FIntVector ChunkOrigin = ChunkCoord * CHUNK_SIZE;
							FIntVector LocalPosition = FIntVector(x,y,0);
							FIntVector GridPos = ChunkOrigin + LocalPosition;
							float SurfaceHeight = (*FoundInstance)->ModifiedPerlinNoise2D(FVector2D(GridPos.X, GridPos.Y));
							NewHeightMap.Set(LocalPosition.X, LocalPosition.Y, SurfaceHeight);
						}
					}
					LowerHeightMaps[i] = NewHeightMap;
					HeightMapMap.Add(MapKey, NewHeightMap);
				}
			}
		}
	}

	// 0. 데이터 분류 및 인덱스 매핑
	TMap<FName, int32> RowNameToIndexMap;
	TArray<int32> BaseTerrainIndices;
	TArray<int32> OreIndices;

	for (int32 i = 0; i < TerrainDatas.Num(); ++i)
	{
		RowNameToIndexMap.Add(TerrainDatas[i]->RowName, i);
		
		if (TerrainDatas[i]->bIsOre)
		{
			OreIndices.Add(i);
		}
		else
		{
			BaseTerrainIndices.Add(i);
		}
	}

	// 1. 모든 재질에 대한 DensityMap을 미리 생성하여 ChunkData에 추가합니다.
	ChunkData->DensityMap.Reserve(TerrainDatas.Num());
	for (int32 i = 0; i < TerrainDatas.Num(); ++i)
	{
		ChunkData->DensityMap.Add(i, ChunkDensityArray());
	}

	const ChunkDensityArray TempVoxelIterator;
	const FIntVector ChunkOrigin = ChunkCoord * CHUNK_SIZE;

	// --- Pass 1: Generate Base Terrains ---
	for (const auto& Pair : TempVoxelIterator)
	{
		const FIntVector LocalPosition = Pair.Key;
		const FIntVector GridPos = ChunkOrigin + LocalPosition;

		for (const int32 Index : BaseTerrainIndices)
		{
			const auto& Data = TerrainDatas[Index];
			float Density = CHUNK_DENSITY_MIN;

			const float UpperHeight = Data->UseNoise2DUpperBound ? UpperHeightMaps[Index].Get(LocalPosition.X, LocalPosition.Y) * 16.f + Data->UpperBound : Data->UpperBound;
			const float LowerHeight = Data->UseNoise2DLowerBound ? LowerHeightMaps[Index].Get(LocalPosition.X, LocalPosition.Y) * 16.f + Data->LowerBound : Data->LowerBound;
			constexpr float Sharpness = 2.0f;

			if (Data->UseNoise3D)
			{
				const TSharedPtr<FNoiseMathUtility>* FoundInstance = NoiseInstances.Find(GetSettingsHash(Data->Noise3D));
				if (FoundInstance)
				{
					const float Area = (*FoundInstance)->ModifiedPerlinNoise3D(FVector(GridPos));
					auto Func = [](const float A, const float X) { return A * X * X + X - A; };
					
					const float NoiseDensity = Func(-0.4, Area);
					
					const float sdfUpper = (UpperHeight - GridPos.Z) / Sharpness;
					const float sdfLower = (GridPos.Z - LowerHeight) / Sharpness;
					const float sdf = FMath::Min(sdfUpper, sdfLower);
					
					if (sdf < -1.0f) Density = CHUNK_DENSITY_MIN;
					else if (sdf > 1.0f) Density = NoiseDensity;
					else Density = FMath::Lerp(CHUNK_DENSITY_MIN, NoiseDensity, (sdf + 1.0f) * 0.5f);
				}
			}
			else
			{
				const float sdfUpper = (UpperHeight - GridPos.Z) / Sharpness;
				const float sdfLower = (GridPos.Z - LowerHeight) / Sharpness;
				const float sdf = FMath::Min(sdfUpper, sdfLower);
				
				Density = FMath::Clamp(sdf, CHUNK_DENSITY_MIN, CHUNK_DENSITY_MAX);
			}
			ChunkData->DensityMap[Index].Set(LocalPosition, Density);
		}
	}

	// --- Pass 2: Generate Ores ---
	for (const auto& Pair : TempVoxelIterator)
	{
		const FIntVector LocalPosition = Pair.Key;
		const FIntVector GridPos = ChunkOrigin + LocalPosition;

		for (const int32 Index : OreIndices)
		{
			const auto& Data = TerrainDatas[Index];
			float Density = CHUNK_DENSITY_MIN;

			// 1. 이 광물이 속할 기반 지형의 인덱스를 찾습니다.
			const int32* BaseTerrainIndexPtr = RowNameToIndexMap.Find(Data->BaseTerrainRowName);

			// 2. 기반 지형이 유효하고, 해당 위치가 '내부'일 때만 광물 생성을 시도합니다.
			if (Data->IsInsideTerrain)
			{
				if (BaseTerrainIndexPtr && ChunkData->DensityMap[*BaseTerrainIndexPtr].Get(LocalPosition) + Data->OffSet > 0.0f)
				{
					const float UpperHeight = Data->UseNoise2DUpperBound ? UpperHeightMaps[Index].Get(LocalPosition.X, LocalPosition.Y) * 16.f + Data->UpperBound : Data->UpperBound;
					const float LowerHeight = Data->UseNoise2DLowerBound ? LowerHeightMaps[Index].Get(LocalPosition.X, LocalPosition.Y) * 16.f + Data->LowerBound : Data->LowerBound;
					constexpr float Sharpness = 2.0f;
					
					if (Data->UseNoise3D)
					{
						const TSharedPtr<FNoiseMathUtility>* FoundInstance = NoiseInstances.Find(GetSettingsHash(Data->Noise3D));
						if (FoundInstance)
						{
							const float Area = (*FoundInstance)->ModifiedPerlinNoise3D(FVector(GridPos));
							auto Func = [](const float A, const float X) { return A * X * X + X - A; };
							
							const float NoiseDensity = Func(-0.4, Area);
							
							const float sdfUpper = (UpperHeight - GridPos.Z) / Sharpness;
							const float sdfLower = (GridPos.Z - LowerHeight) / Sharpness;
							const float sdf = FMath::Min(sdfUpper, sdfLower);
							
							if (sdf < -1.0f) Density = CHUNK_DENSITY_MIN;
							else if (sdf > 1.0f) Density = NoiseDensity;
							else Density = FMath::Lerp(CHUNK_DENSITY_MIN, NoiseDensity, (sdf + 1.0f) * 0.5f);
						}
					}
					else
					{
						const float sdfUpper = (UpperHeight - GridPos.Z) / Sharpness;
						const float sdfLower = (GridPos.Z - LowerHeight) / Sharpness;
						const float sdf = FMath::Min(sdfUpper, sdfLower);
						
						Density = FMath::Clamp(sdf, CHUNK_DENSITY_MIN, CHUNK_DENSITY_MAX);
					}
				}
			}
			else
			{
				const float UpperHeight = Data->UseNoise2DUpperBound ? UpperHeightMaps[Index].Get(LocalPosition.X, LocalPosition.Y) * 16.f + Data->UpperBound : Data->UpperBound;
				const float LowerHeight = Data->UseNoise2DLowerBound ? LowerHeightMaps[Index].Get(LocalPosition.X, LocalPosition.Y) * 16.f + Data->LowerBound : Data->LowerBound;
				constexpr float Sharpness = 2.0f;
					
				if (Data->UseNoise3D)
				{
					const TSharedPtr<FNoiseMathUtility>* FoundInstance = NoiseInstances.Find(GetSettingsHash(Data->Noise3D));
					if (FoundInstance)
					{
						const float Area = (*FoundInstance)->ModifiedPerlinNoise3D(FVector(GridPos));
						auto Func = [](const float A, const float X) { return A * X * X + X - A; };
							
						const float NoiseDensity = Func(-0.4, Area);
							
						const float sdfUpper = (UpperHeight - GridPos.Z) / Sharpness;
						const float sdfLower = (GridPos.Z - LowerHeight) / Sharpness;
						const float sdf = FMath::Min(sdfUpper, sdfLower);
							
						if (sdf < -1.0f) Density = CHUNK_DENSITY_MIN;
						else if (sdf > 1.0f) Density = NoiseDensity;
						else Density = FMath::Lerp(CHUNK_DENSITY_MIN, NoiseDensity, (sdf + 1.0f) * 0.5f);
					}
				}
				else
				{
					const float sdfUpper = (UpperHeight - GridPos.Z) / Sharpness;
					const float sdfLower = (GridPos.Z - LowerHeight) / Sharpness;
					const float sdf = FMath::Min(sdfUpper, sdfLower);
						
					Density = FMath::Clamp(sdf, CHUNK_DENSITY_MIN, CHUNK_DENSITY_MAX);
				}
			}
			
			// 기반 지형이 '내부'가 아니면 Density는 기본값인 CHUNK_DENSITY_MIN으로 유지됩니다.
			ChunkData->DensityMap[Index].Set(LocalPosition, Density);
		}
	}
}
