// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

template <typename ElementType, int32 Size, int32 Margin, typename Derived>
class TCaveArrayBase
{
public:
	static constexpr int32 PaddedSize = Size + 2 * Margin;
	static constexpr int32 NumElements = PaddedSize * PaddedSize * PaddedSize;

	TCaveArrayBase()
	{
		Data.SetNumZeroed(NumElements);
	}

	// Set 함수: (X, Y, Z)에 Value 저장
	FORCEINLINE void Set(const int16 X, const int16 Y, const int16 Z, ElementType Value)
	{
		ensure(X >= -Margin && X < Size + Margin);
		ensure(Y >= -Margin && Y < Size + Margin);
		ensure(Z >= -Margin && Z < Size + Margin);
		const Derived* Self = static_cast<const Derived*>(this);
		uint32 Index = (Margin == 0)
			? Self->EncodeFunc(static_cast<uint_fast16_t>(X), static_cast<uint_fast16_t>(Y), static_cast<uint_fast16_t>(Z))
			: Self->EncodeFunc(static_cast<uint_fast16_t>(X + Margin), static_cast<uint_fast16_t>(Y + Margin), static_cast<uint_fast16_t>(Z + Margin));
		Data[Index] = Value;
	}

	// Get 함수: (X, Y, Z)의 값을 반환
	FORCEINLINE ElementType Get(const int16 X, const int16 Y, const int16 Z) const
	{
		ensure(X >= -Margin && X < Size + Margin);
		ensure(Y >= -Margin && Y < Size + Margin);
		ensure(Z >= -Margin && Z < Size + Margin);
		const Derived* Self = static_cast<const Derived*>(this);
		uint32 Index = (Margin == 0)
			? Self->EncodeFunc(static_cast<uint_fast16_t>(X), static_cast<uint_fast16_t>(Y), static_cast<uint_fast16_t>(Z))
			: Self->EncodeFunc(static_cast<uint_fast16_t>(X + Margin), static_cast<uint_fast16_t>(Y + Margin), static_cast<uint_fast16_t>(Z + Margin));
		return Data[Index];
	}

	// FIntVector 버전
	FORCEINLINE void Set(const FIntVector& Coord, ElementType Value)
	{
		Set(Coord.X, Coord.Y, Coord.Z, Value);
	}
	FORCEINLINE ElementType Get(const FIntVector& Coord) const
	{
		return Get(Coord.X, Coord.Y, Coord.Z);
	}

	// TODO : 많이 호출되는 함수인데 if문이 너무 많음
	UE::Math::TVector<ElementType> GetGradient(int32 X, int32 Y, int32 Z, const int32 Delta = 1) const
	{
		static_assert(std::is_arithmetic_v<ElementType>, "ElementType must be arithmetic for gradient calculation!");

		// dx 계산
		ElementType dx = 0;
		if (X - Delta >= -Margin && X + Delta < Size + Margin)
		{
			// 중앙 차분
			dx = (Get(X + Delta, Y, Z) - Get(X - Delta, Y, Z)) * 0.5f;
		}
		else if (X + Delta < Size + Margin)
		{
			// forward difference
			dx = Get(X + Delta, Y, Z) - Get(X, Y, Z);
		}
		else if (X - Delta >= -Margin)
		{
			// backward difference
			dx = Get(X, Y, Z) - Get(X - Delta, Y, Z);
		}

		// dy 계산
		ElementType dy = 0;
		if (Y - Delta >= -Margin && Y + Delta < Size + Margin)
		{
			dy = (Get(X, Y + Delta, Z) - Get(X, Y - Delta, Z)) * 0.5f;
		}
		else if (Y + Delta < Size + Margin)
		{
			dy = Get(X, Y + Delta, Z) - Get(X, Y, Z);
		}
		else if (Y - Delta >= -Margin)
		{
			dy = Get(X, Y, Z) - Get(X, Y - Delta, Z);
		}

		// dz 계산
		ElementType dz = 0;
		if (Z - Delta >= -Margin && Z + Delta < Size + Margin)
		{
			dz = (Get(X, Y, Z + Delta) - Get(X, Y, Z - Delta)) * 0.5f;
		}
		else if (Z + Delta < Size + Margin)
		{
			dz = Get(X, Y, Z + Delta) - Get(X, Y, Z);
		}
		else if (Z - Delta >= -Margin)
		{
			dz = Get(X, Y, Z) - Get(X, Y, Z - Delta);
		}

		UE::Math::TVector<ElementType> Gradient(dx, dy, dz);
		if (Gradient.Normalize())
		{
			return Gradient;
		}
		else
		{
			return UE::Math::TVector<ElementType>(0,0,0);
		}
	}

	// FIntVector 버전 오버로드
	UE::Math::TVector<ElementType> GetGradient(const FIntVector& Coord, const int32 Delta = 1) const
	{
		return GetGradient(Coord.X, Coord.Y, Coord.Z, Delta);
	}

	TCaveArrayBase<UE::Math::TVector<ElementType>, Size, Margin, TCaveArrayBase<UE::Math::TVector<ElementType>, Size, Margin, void>>
	GetGradientArray()
	{
		static_assert(std::is_arithmetic_v<ElementType>, "ElementType must be arithmetic for gradient calculation!");

		constexpr int32 PaddedSize = Size + 2 * Margin;
		using GradArrayType = TCaveArrayBase<UE::Math::TVector<ElementType>, Size, Margin, TCaveArrayBase<UE::Math::TVector<ElementType>, Size, Margin, void>>;
		GradArrayType GradArray;

		for (int32 z = 0; z < PaddedSize; ++z)
		{
			for (int32 y = 0; y < PaddedSize; ++y)
			{
				for (int32 x = 0; x < PaddedSize; ++x)
				{
					// 경계(0, PaddedSize-1)에서는 gradient = 0
					if (x == 0 || x == PaddedSize - 1 ||
						y == 0 || y == PaddedSize - 1 ||
						z == 0 || z == PaddedSize - 1)
					{
						GradArray.Set(x, y, z, FVector::ZeroVector);
					}
					else
					{
						// 중앙차분 central difference
						float dx = 0.5f * (Get(x + 1, y, z) - Get(x - 1, y, z));
						float dy = 0.5f * (Get(x, y + 1, z) - Get(x, y - 1, z));
						float dz = 0.5f * (Get(x, y, z + 1) - Get(x, y, z - 1));
						GradArray.Set(x, y, z, FVector(dx, dy, dz));
					}
				}
			}
		}
		return GradArray;
	}
	
	FORCEINLINE const TArray<ElementType>& GetData() const { return Data; }
	FORCEINLINE TArray<ElementType>& GetData() { return Data; }

	static constexpr int32 GetMargin() { return Margin; }
	static constexpr int32 GetPaddedSize() { return PaddedSize; }
	static constexpr int32 GetNumElements() { return NumElements; }

	template <bool bConst>
	class Iterator
	{
		using OwnerType = std::conditional_t<bConst, const TCaveArrayBase, TCaveArrayBase>;
		using ValuePtrType = std::conditional_t<bConst, const ElementType*, ElementType*>;
	public:
		Iterator(OwnerType* InOwner, int32 InIndex)
			: Owner(InOwner), Index(InIndex)
		{
			UpdatePair();
		}
		Iterator& operator++()
		{
			++Index;
			UpdatePair();
			return *this;
		}
		bool operator!=(const Iterator& Other) const { return Index != Other.Index; }
		TPair<FIntVector, ValuePtrType>& operator*() { return Pair; }
		TPair<FIntVector, ValuePtrType>* operator->() { return &Pair; }

	private:
		OwnerType* Owner;
		int32 Index;
		TPair<FIntVector, ValuePtrType> Pair;

		void UpdatePair()
		{
			if (Index < NumElements)
			{
				uint_fast32_t x, y, z;
				Derived::DecodeFunc(Index, x, y, z);
				Pair.Key = FIntVector(x - Margin, y - Margin, z - Margin);
				Pair.Value = &Owner->Data[Index];
			}
		}
	};

	Iterator<false> begin() { return Iterator<false>(this, 0); }
	Iterator<false> end() { return Iterator<false>(this, NumElements); }
	Iterator<true> begin() const { return Iterator<true>(this, 0); }
	Iterator<true> end() const { return Iterator<true>(this, NumElements); }

		// ------- OctantIterator -------
    template <bool bConst>
    class OctantIterator
    {
        using OwnerType = std::conditional_t<bConst, const TCaveArrayBase, TCaveArrayBase>;
        using ValuePtrType = std::conditional_t<bConst, const ElementType*, ElementType*>;

    public:
        OctantIterator(OwnerType* InOwner, int32 OctX, int32 OctY, int32 OctZ, bool bEnd = false)
            : Owner(InOwner), OctantX(OctX), OctantY(OctY), OctantZ(OctZ)
        {
            OctantSize = PaddedSize / 2;
            StartX = OctantX * OctantSize;
            StartY = OctantY * OctantSize;
            StartZ = OctantZ * OctantSize;
            EndX = StartX + OctantSize;
            EndY = StartY + OctantSize;
            EndZ = StartZ + OctantSize;

            if (bEnd)
            {
                X = StartX;
                Y = StartY;
                Z = EndZ;
            }
            else
            {
                X = StartX;
                Y = StartY;
                Z = StartZ;
                UpdatePair();
            }
        }
        OctantIterator& operator++()
        {
            ++X;
            if (X >= EndX)
            {
                X = StartX;
                ++Y;
                if (Y >= EndY)
                {
                    Y = StartY;
                    ++Z;
                }
            }
            if (Z < EndZ) UpdatePair();
            return *this;
        }
        bool operator!=(const OctantIterator& Other) const
        {
            return (X != Other.X) || (Y != Other.Y) || (Z != Other.Z);
        }
        TPair<FIntVector, ValuePtrType>& operator*() { return Pair; }
        TPair<FIntVector, ValuePtrType>* operator->() { return &Pair; }

    private:
        OwnerType* Owner;
        int32 OctantX, OctantY, OctantZ;
        int32 OctantSize;
        int32 StartX, StartY, StartZ, EndX, EndY, EndZ;
        int32 X, Y, Z;
        TPair<FIntVector, ValuePtrType> Pair;

        void UpdatePair()
        {
            if (Z < EndZ)
            {
                uint32 idx = Derived::EncodeFunc(
                    static_cast<uint_fast16_t>(X),
                    static_cast<uint_fast16_t>(Y),
                    static_cast<uint_fast16_t>(Z)
                );
                Pair.Key = FIntVector(X - Margin, Y - Margin, Z - Margin);
                Pair.Value = &Owner->Data[idx];
            }
        }
    };

    // 비-const 버전
    OctantIterator<false> OctantBegin(int32 OctX, int32 OctY, int32 OctZ)
    {
        return OctantIterator<false>(this, OctX, OctY, OctZ, false);
    }
    OctantIterator<false> OctantEnd(int32 OctX, int32 OctY, int32 OctZ)
    {
        return OctantIterator<false>(this, OctX, OctY, OctZ, true);
    }
    // const 버전
    OctantIterator<true> OctantBegin(int32 OctX, int32 OctY, int32 OctZ) const
    {
        return OctantIterator<true>(this, OctX, OctY, OctZ, false);
    }
    OctantIterator<true> OctantEnd(int32 OctX, int32 OctY, int32 OctZ) const
    {
        return OctantIterator<true>(this, OctX, OctY, OctZ, true);
    }
	// ------- RangeIterator -------
    template <bool bConst>
    class RangeIterator
    {
        // OwnerType: const/비-const 자동 분기
        using OwnerType = std::conditional_t<bConst, const TCaveArrayBase, TCaveArrayBase>;
        using ValuePtrType = std::conditional_t<bConst, const ElementType*, ElementType*>;

    public:
        RangeIterator(OwnerType* InOwner, const FIntVector& InMin, const FIntVector& InMax, const FIntVector& InPos, bool bEnd = false)
            : Owner(InOwner), Min(InMin), Max(InMax), Pos(InPos), bEnd(bEnd)
        {
            if (!bEnd)
                UpdatePair();
        }
        RangeIterator& operator++()
        {
            if (bEnd) return *this;
            ++Pos.X;
            if (Pos.X > Max.X)
            {
                Pos.X = Min.X;
                ++Pos.Y;
                if (Pos.Y > Max.Y)
                {
                    Pos.Y = Min.Y;
                    ++Pos.Z;
                    if (Pos.Z > Max.Z)
                    {
                        bEnd = true;
                        return *this;
                    }
                }
            }
            if (!bEnd)
                UpdatePair();
            return *this;
        }
        bool operator!=(const RangeIterator& Other) const
        {
            if (bEnd || Other.bEnd)
                return bEnd != Other.bEnd;
            return Pos != Other.Pos;
        }
        TPair<FIntVector, ValuePtrType>& operator*() { return Pair; }
        TPair<FIntVector, ValuePtrType>* operator->() { return &Pair; }

    private:
        OwnerType* Owner;
        FIntVector Min, Max, Pos;
        bool bEnd;
        TPair<FIntVector, ValuePtrType> Pair;

        void UpdatePair()
        {
            uint32 idx = Derived::EncodeFunc(
                static_cast<uint_fast16_t>(Pos.X + Margin),
                static_cast<uint_fast16_t>(Pos.Y + Margin),
                static_cast<uint_fast16_t>(Pos.Z + Margin)
            );
            Pair.Key = Pos;
            Pair.Value = &Owner->Data[idx];
        }
    };

    // 비-const 버전 begin/end
    RangeIterator<false> RangeBegin(const FIntVector& Min, const FIntVector& Max)
    {
        return RangeIterator<false>(this, Min, Max, Min, false);
    }
    RangeIterator<false> RangeEnd(const FIntVector& Min, const FIntVector& Max)
    {
        return RangeIterator<false>(this, Min, Max, FIntVector(), true);
    }
    // const 버전 begin/end
    RangeIterator<true> RangeBegin(const FIntVector& Min, const FIntVector& Max) const
    {
        return RangeIterator<true>(this, Min, Max, Min, false);
    }
    RangeIterator<true> RangeEnd(const FIntVector& Min, const FIntVector& Max) const
    {
        return RangeIterator<true>(this, Min, Max, FIntVector(), true);
    }
	
protected:
	TArray<ElementType> Data;
};