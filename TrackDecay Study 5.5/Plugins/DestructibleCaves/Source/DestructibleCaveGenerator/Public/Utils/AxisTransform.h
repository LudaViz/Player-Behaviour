// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

namespace AxisTransform
{

enum class EAxisToZ : uint8
{
	XPlus,   // +X 방향을 +Z로
	XMinus,  // -X 방향을 +Z로
	YPlus,   // +Y 방향을 +Z로
	YMinus,  // -Y 방향을 +Z로
	ZPlus,   // +Z(자기 자신)
	ZMinus   // -Z 방향을 +Z로
};


// TODO : 매번 새로 만들지 말고 array를 새로 만들어서 반환하기
FORCEINLINE FIntVector TransformToZ(const FIntVector& In, EAxisToZ Axis, int32 PaddedSize)
{
	switch (Axis)
	{
	case EAxisToZ::XPlus:
		return FIntVector(In.Y, In.Z, In.X);
	case EAxisToZ::XMinus:
		return FIntVector(In.Z, In.Y, PaddedSize - 1 - In.X);
	case EAxisToZ::YPlus:
		return FIntVector(In.Z, In.X, In.Y);
	case EAxisToZ::YMinus:
		return FIntVector(In.X, In.Z, PaddedSize - 1 - In.Y);
	case EAxisToZ::ZMinus:
		return FIntVector(In.Y, In.X, PaddedSize - 1 - In.Z);
	case EAxisToZ::ZPlus:
	default:
		// 변화 없음
		return In;
	}
}

// TransformToZ와 동일한 Axis사용하기
FORCEINLINE FIntVector TransformFromZ(const FIntVector& In, EAxisToZ Axis, int32 PaddedSize)
{
	switch (Axis)
	{
	case EAxisToZ::XPlus:
		return FIntVector(In.Z, In.X, In.Y);
	case EAxisToZ::XMinus:
		return FIntVector(PaddedSize - 1 - In.Z, In.Y, In.X);
	case EAxisToZ::YPlus:
		return FIntVector(In.Y, In.Z, In.X);
	case EAxisToZ::YMinus:
		return FIntVector(In.X, PaddedSize - 1 - In.Z, In.Y);
	case EAxisToZ::ZMinus:
		return FIntVector(In.X, In.Y, PaddedSize - 1 - In.Z);
	case EAxisToZ::ZPlus:
	default:
		return In;
	}
}

FORCEINLINE FVector TransformToZ(const FVector& In, EAxisToZ Axis, float PaddedSize)
{
	switch (Axis)
	{
	case EAxisToZ::XPlus:
		return FVector(In.Y, In.Z, In.X);
	case EAxisToZ::XMinus:
		return FVector(In.Z, In.Y, PaddedSize - 1.f - In.X);
	case EAxisToZ::YPlus:
		return FVector(In.Z, In.X, In.Y);
	case EAxisToZ::YMinus:
		return FVector(In.X, In.Z, PaddedSize - 1.f - In.Y);
	case EAxisToZ::ZMinus:
		return FVector(In.Y, In.X, PaddedSize - 1.f - In.Z);
	case EAxisToZ::ZPlus:
	default:
		return In;
	}
}

FORCEINLINE FVector TransformFromZ(const FVector& In, EAxisToZ Axis, float PaddedSize)
{
	switch (Axis)
	{
	case EAxisToZ::XPlus:
		return FVector(In.Z, In.X, In.Y);
	case EAxisToZ::XMinus:
		return FVector(PaddedSize - 1.f - In.Z, In.Y, In.X);
	case EAxisToZ::YPlus:
		return FVector(In.Y, In.Z, In.X);
	case EAxisToZ::YMinus:
		return FVector(In.X, PaddedSize - 1.f - In.Z, In.Y);
	case EAxisToZ::ZMinus:
		return FVector(In.X, In.Y, PaddedSize - 1.f - In.Z);
	case EAxisToZ::ZPlus:
	default:
		return In;
	}
}

};