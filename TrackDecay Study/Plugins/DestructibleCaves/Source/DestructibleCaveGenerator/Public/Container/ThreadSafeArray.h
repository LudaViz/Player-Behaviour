// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"

template<typename ElementType>
class TThreadSafeArray
{
public:
    // Add element to end
    void Add(const ElementType& Element)
    {
        FScopeLock Lock(&CriticalSection);
        Array.Add(Element);
    }

    // Add element to end (rvalue)
    void Add(ElementType&& Element)
    {
        FScopeLock Lock(&CriticalSection);
        Array.Add(MoveTemp(Element));
    }

    // Remove element (by value), returns number removed
    int32 Remove(const ElementType& Element)
    {
        FScopeLock Lock(&CriticalSection);
        return Array.Remove(Element);
    }

    // Remove at index
    void RemoveAt(int32 Index, int32 Count = 1, bool bAllowShrinking = true)
    {
        FScopeLock Lock(&CriticalSection);
        Array.RemoveAt(Index, Count, bAllowShrinking);
    }

    // Find index of element, or INDEX_NONE if not found
    int32 Find(const ElementType& Element) const
    {
        FScopeLock Lock(&CriticalSection);
        return Array.Find(Element);
    }

    // Get pointer to element at index (nullptr if out of bounds)
    ElementType* GetPtr(int32 Index)
    {
        FScopeLock Lock(&CriticalSection);
        return Array.IsValidIndex(Index) ? &Array[Index] : nullptr;
    }

    // Get pointer to element at index (const version)
    const ElementType* GetPtr(int32 Index) const
    {
        FScopeLock Lock(&CriticalSection);
        return Array.IsValidIndex(Index) ? &Array[Index] : nullptr;
    }

    // operator[] (non-const) - throws check if invalid
    ElementType& operator[](int32 Index)
    {
        FScopeLock Lock(&CriticalSection);
        check(Array.IsValidIndex(Index));
        return Array[Index];
    }

    // operator[] (const) - throws check if invalid
    const ElementType& operator[](int32 Index) const
    {
        FScopeLock Lock(&CriticalSection);
        check(Array.IsValidIndex(Index));
        return Array[Index];
    }

    // Get array size
    int32 Num() const
    {
        FScopeLock Lock(&CriticalSection);
        return Array.Num();
    }

    // Set array size (will add or remove elements)
    void SetNum(int32 NewNum, bool bAllowShrinking = true)
    {
        FScopeLock Lock(&CriticalSection);
        Array.SetNum(NewNum, bAllowShrinking);
    }

    // Empty array
    void Empty(int32 Slack = 0)
    {
        FScopeLock Lock(&CriticalSection);
        Array.Empty(Slack);
    }

    // Copy to out array (thread-safe snapshot)
    void CopyTo(TArray<ElementType>& OutArray) const
    {
        FScopeLock Lock(&CriticalSection);
        OutArray = Array;
    }

    // Thread-safe iteration (calls Func with each element, snapshot)
    template<typename FuncType>
    void ForEach(FuncType Func) const
    {
        FScopeLock Lock(&CriticalSection);
        for (const auto& Elem : Array)
        {
            Func(Elem);
        }
    }

private:
    TArray<ElementType> Array;
    mutable FCriticalSection CriticalSection;
};