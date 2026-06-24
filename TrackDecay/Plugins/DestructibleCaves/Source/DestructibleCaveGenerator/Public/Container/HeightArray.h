// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 2D 전용 베이스 템플릿
 *
 * Size  : 유효 영역 한 변의 길이 (패딩 제외)
 * Margin: 외곽 패딩 폭
 * ElementType: 저장 자료형
 * Derived     : CRTP(자기 자신) – 인코딩 함수를 파생형이 구현
 */
template<
    int32 Size,
    int32 Margin,
    typename ElementType,
    typename Derived>
class TCaveArrayBase2D
{
public:
    static constexpr int32 PaddedSize  = Size + 2 * Margin;         // 패딩 포함 총 길이
    static constexpr int32 NumElements = PaddedSize * PaddedSize;   // 총 원소 수

    TCaveArrayBase2D()       { Data.SetNumZeroed(NumElements); }

    /* ------- 값 쓰기 / 읽기 ------- */
    FORCEINLINE void Set(int16 X, int16 Y, ElementType Value)
    {
        ensure(X >= -Margin && X < Size + Margin);
        ensure(Y >= -Margin && Y < Size + Margin);

        const Derived* Self = static_cast<const Derived*>(this);
        const uint32 Index  = (Margin == 0)
            ? Self->EncodeFunc(static_cast<uint_fast16_t>(X),
                                static_cast<uint_fast16_t>(Y))
            : Self->EncodeFunc(static_cast<uint_fast16_t>(X + Margin),
                               static_cast<uint_fast16_t>(Y + Margin));

        Data[Index] = Value;
    }

    FORCEINLINE ElementType Get(int16 X, int16 Y) const
    {
        ensure(X >= -Margin && X < Size + Margin);
        ensure(Y >= -Margin && Y < Size + Margin);

        const Derived* Self = static_cast<const Derived*>(this);
        const uint32 Index  = (Margin == 0)
            ? Self->EncodeFunc(static_cast<uint_fast16_t>(X),
                                static_cast<uint_fast16_t>(Y))
            : Self->EncodeFunc(static_cast<uint_fast16_t>(X + Margin),
                               static_cast<uint_fast16_t>(Y + Margin));

        return Data[Index];
    }

    /* FVector2D 기반 버전 */
    FORCEINLINE void Set(const FIntPoint& Coord, ElementType Value) { Set(Coord.X, Coord.Y, Value); }
    FORCEINLINE ElementType Get(const FIntPoint& Coord) const       { return Get(Coord.X, Coord.Y); }

    /* ------- 반복자 ------- */
    template<bool bIsConst>
    class Iterator
    {
        using OwnerType    = std::conditional_t<bIsConst, const TCaveArrayBase2D*, TCaveArrayBase2D*>;
        using ValuePtrType = std::conditional_t<bIsConst, const ElementType*, ElementType*>;

    public:
        Iterator(OwnerType InOwner, int32 InIndex) : Owner(InOwner), Index(InIndex) { UpdatePair(); }

        Iterator& operator++()   { ++Index;  UpdatePair(); return *this; }
        bool operator!=(const Iterator& Other) const { return Index != Other.Index; }

        TPair<FIntPoint, ValuePtrType>& operator*()  { return Pair; }
        TPair<FIntPoint, ValuePtrType>* operator->() { return &Pair; }

    private:
        OwnerType Owner;
        int32     Index;
        TPair<FIntPoint, ValuePtrType> Pair;

        void UpdatePair()
        {
            if (Index < NumElements)
            {
                uint_fast32_t x, y;
                Derived::DecodeFunc(Index, x, y);
                Pair.Key   = FIntPoint(x - Margin, y - Margin);
                Pair.Value = &Owner->Data[Index];
            }
        }
    };

    /* begin/end (const + non-const) */
    Iterator<false> begin()             { return Iterator<false>(this, 0); }
    Iterator<false> end()               { return Iterator<false>(this, NumElements); }
    Iterator<true>  begin() const       { return Iterator<true>(this, 0); }
    Iterator<true>  end()   const       { return Iterator<true>(this, NumElements); }

    /* 데이터 직접 접근 */
    FORCEINLINE const TArray<ElementType>& GetData() const { return Data; }
    FORCEINLINE TArray<ElementType>&       GetData()       { return Data; }

    static constexpr int32 GetMargin()      { return Margin;      }
    static constexpr int32 GetPaddedSize()  { return PaddedSize;  }
    static constexpr int32 GetNumElements() { return NumElements; }

protected:
    TArray<ElementType> Data;
};

/* ------------------------------------------------------------------------- */
/*           선형 인코딩 2D 구현 – 작은 크기(≈64 이하)에 적합                */
/* ------------------------------------------------------------------------- */
template<
    int32   Size,
    int32   Margin,
    typename ElementType>
class TLinearArray2D
    : public TCaveArrayBase2D<Size, Margin, ElementType, TLinearArray2D<Size, Margin, ElementType>>
{
    using Super = TCaveArrayBase2D<Size, Margin, ElementType, TLinearArray2D>;
public:
    /* (x, y) → 선형 index */
    static FORCEINLINE uint32 EncodeFunc(uint_fast16_t x, uint_fast16_t y)
    {
        constexpr uint32 Dim = Size + 2 * Margin;
        return x + y * Dim;
    }

    /* 선형 index → (x, y) */
    static FORCEINLINE void DecodeFunc(uint32 index, uint_fast32_t& x, uint_fast32_t& y)
    {
        constexpr uint32 Dim = Size + 2 * Margin;
        y = index / Dim;
        x = index % Dim;
    }
};
