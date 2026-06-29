// Copyright 2025 J2K2. All Rights Reserved.

#include "Utils/ChunkUtils.h"
// 네임스페이스를 사용하여 코드 블록을 정의합니다.
namespace FChunkUtils
{
    // static 변수를 사용하여 이 CPP 파일 내에서만 접근 가능한 전역 캐시를 만듭니다.
    // 이 변수는 프로그램 시작 시 초기화되고, 프로그램 종료 시 자동으로 소멸됩니다.
    static TMap<int32, TArray<FIntVector>> G_CubeShellOffsetsCache;
    
    // 함수 구현
    void GenerateCubeShellOffsets(int32 Distance, TArray<FIntVector>& OutOffsets)
    {
        // 1. 캐시에 이미 결과가 있는지 확인합니다.
        if (const TArray<FIntVector>* CachedResult = G_CubeShellOffsetsCache.Find(Distance))
        {
            // 캐시 히트: 계산 없이 바로 복사하고 함수를 종료합니다.
            OutOffsets = *CachedResult;
            return;
        }

        // 캐시 미스: 새로 계산합니다.
        TArray<FIntVector> NewOffsets;
        
        if (Distance < 0)
        {
            OutOffsets.Reset(); // 잘못된 입력 시 빈 배열 반환
            return;
        }
        
        if (Distance == 0)
        {
            NewOffsets.Add(FIntVector::ZeroValue);
        }
        else
        {
            // 예상 크기를 미리 할당하여 성능을 최적화합니다.
            NewOffsets.Reserve(24 * Distance * Distance + 2);

            for (int32 x = -Distance; x <= Distance; ++x)
            {
                for (int32 y = -Distance; y <= Distance; ++y)
                {
                    for (int32 z = -Distance; z <= Distance; ++z)
                    {
                        if (FMath::Abs(x) == Distance || FMath::Abs(y) == Distance || FMath::Abs(z) == Distance)
                        {
                            NewOffsets.Add(FIntVector(x, y, z));
                        }
                    }
                }
            }
        }

        // 3. 계산된 결과를 다음 사용을 위해 캐시에 저장합니다.
        // Emplace는 키와 값을 직접 생성하여 복사를 줄일 수 있습니다.
        const TArray<FIntVector>& CachedEntry = G_CubeShellOffsetsCache.Emplace(Distance, MoveTemp(NewOffsets));

        // 4. 계산된 결과를 출력 배열에 복사합니다.
        // 위에서 Emplace를 사용했으므로, NewOffsets는 이제 비어있습니다. 캐시에서 직접 복사합니다.
        OutOffsets = CachedEntry;
    }

    void ClearCubeShellCache()
    {
        G_CubeShellOffsetsCache.Empty();
        UE_LOG(LogTemp, Log, TEXT("Cube shell offsets cache has been cleared."));
    }
}