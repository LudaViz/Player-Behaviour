// Copyright 2025 J2K2. All Rights Reserved.
// 관련 매크로
#pragma once
#include "Logging/LogMacros.h"

// #define DCG_DISABLE_OPTIMIZATION UE_DISABLE_OPTIMIZATION
#define DCG_DISABLE_OPTIMIZATION

#define CHUNK_SIZE 32
// normal 계산을 위해서 필수. gradient 또는 polygon단위 normal 계산에 이용
// gradient계산을 위해서는 1로 충분
#define CHUNK_MARGIN 1 
#define CHUNK_DENSITY_MIN -1.f
#define CHUNK_DENSITY_MAX 1.f

/**
 * 2^MAX_LOD_LEVEL should be less than CHUNK_SIZE / 2
 * therefore, CHUNK_SIZE = 16 -> MAX_LOD_LEVEL should be <= 2
 */
#define MAX_LOD_LEVEL 3

DECLARE_LOG_CATEGORY_EXTERN(LogDCG, Display, All);

// #define DCG_IGNORE_Z_DISTANCE
