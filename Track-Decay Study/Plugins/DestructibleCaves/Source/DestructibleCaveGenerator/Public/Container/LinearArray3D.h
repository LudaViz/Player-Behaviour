// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CaveArrayBase.h"

/**
 * Size가 작을 때 (< 64 정도) 사용합니다.
 */
template <typename ElementType, int32 Size, int32 Margin>
class TLinearArray3D : public TCaveArrayBase<ElementType, Size, Margin, TLinearArray3D<ElementType, Size, Margin>>
{
public:
	using Super = TCaveArrayBase<ElementType, Size, Margin, TLinearArray3D<ElementType, Size, Margin>>;

	static uint32 EncodeFunc(uint_fast16_t x, uint_fast16_t y, uint_fast16_t z)
	{
		constexpr uint32 Dim = Size + 2 * Margin;
		return x + y * Dim + z * Dim * Dim;
	}
	static void DecodeFunc(uint32 index, uint_fast32_t& x, uint_fast32_t& y, uint_fast32_t& z)
	{
		constexpr uint32 Dim = Size + 2 * Margin;
		z = index / (Dim * Dim);
		uint32 yz = index % (Dim * Dim);
		y = yz / Dim;
		x = yz % Dim;
	}
};