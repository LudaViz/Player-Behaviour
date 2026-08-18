// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Materials/MaterialInstance.h"
#include "NoiseGenerator.generated.h"

struct FTerrainData;
struct FNoiseParameter;
class FNoiseMathUtility;
struct FChunkData;

struct FHeightMapKey
{
    FIntVector2 ChunkCoord2D;
    uint32 NoiseParamHash;

    FHeightMapKey(const FIntVector2& InCoord, uint32 InHash)
        : ChunkCoord2D(InCoord), NoiseParamHash(InHash) {}

    bool operator==(const FHeightMapKey& Other) const
    {
        return ChunkCoord2D == Other.ChunkCoord2D && NoiseParamHash == Other.NoiseParamHash;
    }
};

FORCEINLINE uint32 GetTypeHash(const FHeightMapKey& Key)
{
    return HashCombine(GetTypeHash(Key.ChunkCoord2D), Key.NoiseParamHash);
}

UCLASS(BlueprintType)
class UNoiseGenerator : public UObject
{
    GENERATED_BODY()

public:
    /** Create default noise settings */
    UNoiseGenerator();
    
    //쓰레드 세이프할 수 있도록 각 Setting별 인스턴스 미리 생성
    /** Initialize internal noise instances and presets */
    void Initialize(TArray<FTerrainData*> InTerrainData);

    TArray<FTerrainData*> TerrainDatas;

    /** Fill given chunk data with noise values */
    void FillChunkData(FChunkData* ChunkData, const FIntVector& ChunkCoord) const;
    
private:

    // === FastNoiseLite 인스턴스 캐시 ===
    // --- FastNoiseLite Caching ---

    /** 메인 노이즈 인스턴스를 위한 TMap 캐시 */
    mutable TMap<uint32, TSharedPtr<FNoiseMathUtility>> NoiseInstances;
    
    /** FNoiseSettings로부터 해시를 생성 */
    uint32 GetSettingsHash(const FNoiseParameter& InSettings) const;

    /** 캐시로부터 Noise 인스턴스를 가져오거나 생성 */
    TSharedPtr<FNoiseMathUtility> GetNoiseInstance(const FNoiseParameter& InData) const;
};
