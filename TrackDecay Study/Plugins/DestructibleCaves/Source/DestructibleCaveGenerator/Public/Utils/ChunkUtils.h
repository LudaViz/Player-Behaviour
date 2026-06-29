// Copyright 2025 J2K2. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

namespace FChunkUtils
{
	/**
	 * 지정된 거리의 정육면체 껍질에 대한 좌표 오프셋을 생성합니다.
	 * 이 함수는 내부적으로 결과를 캐시하여 동일한 거리에 대한 후속 호출 시 계산을 생략합니다.
	 * 
	 * @param Distance 껍질까지의 거리. 0 이상이어야 합니다.
	 * @param OutOffsets 결과를 저장할 배열. 이전 내용은 지워집니다.
	 */
	void GenerateCubeShellOffsets(int32 Distance, TArray<FIntVector>& OutOffsets);

	/**
	 * GenerateCubeShellOffsets 함수가 사용하는 내부 캐시를 비웁니다.
	 * 레벨 종료 또는 메모리 정리가 필요할 때 호출할 수 있습니다.
	 */
	void ClearCubeShellCache();
}