// Copyright 2025 J2K2. All Rights Reserved.

#pragma once

/**
 * method is thread-safe but iterator is unsafe.
 * use ForEach() for safe iteration.
 */
template<typename KeyType, typename ValueType>
class TThreadSafeMap
{
public:
	using MapType = TMap<KeyType, ValueType>;
	using Iterator = typename MapType::TIterator;
	using ConstIterator = typename MapType::TConstIterator;
	
	// Insert or update
	void Add(const KeyType& Key, const ValueType& Value)
	{
		FScopeLock Lock(&CriticalSection);
		Map.Add(Key, Value);
	}

	void Add(const KeyType& Key)
	{
		FScopeLock Lock(&CriticalSection);
		Map.Add(Key);
	}

	// Remove by key
	bool Remove(const KeyType& Key)
	{
		FScopeLock Lock(&CriticalSection);
		return Map.Remove(Key) > 0;
	}

	// Find value pointer (nullptr if not found)
	ValueType* Find(const KeyType& Key)
	{
		FScopeLock Lock(&CriticalSection);
		return Map.Find(Key);
	}

	// Find value pointer (const version)
	const ValueType* Find(const KeyType& Key) const
	{
		FScopeLock Lock(&CriticalSection);
		return Map.Find(Key);
	}

	// Get or add value
	ValueType& FindOrAdd(const KeyType& Key)
	{
		FScopeLock Lock(&CriticalSection);
		return Map.FindOrAdd(Key);
	}

	// operator[] override (non-const): Find or add
	ValueType& operator[](const KeyType& Key)
	{
		FScopeLock Lock(&CriticalSection);
		return Map.FindOrAdd(Key);
	}

	// operator[] override (const): find, error if not found
	const ValueType& operator[](const KeyType& Key) const
	{
		FScopeLock Lock(&CriticalSection);
		return Map[Key];
	}

	// Get number of pairs
	int32 Num() const
	{
		FScopeLock Lock(&CriticalSection);
		return Map.Num();
	}

	// Clear all entries
	void Empty()
	{
		FScopeLock Lock(&CriticalSection);
		Map.Empty();
	}

	bool Contains(const KeyType& Key) const
	{
		FScopeLock Lock(&CriticalSection);
		return Map.Contains(Key);
	}

	// Thread-safe iteration
	template<typename FuncType>
	void ForEach(FuncType Func) const
	{
		FScopeLock Lock(&CriticalSection);
		for (const auto& Pair : Map)
		{
			Func(Pair.Key, Pair.Value);
		}
	}

	// Range-based for (thread unsafe)
	// FORCEINLINE Iterator begin()             { return Map.begin(); }
	// FORCEINLINE Iterator end()               { return Map.end(); }
	// FORCEINLINE ConstIterator begin() const  { return Map.begin(); }
	// FORCEINLINE ConstIterator end() const    { return Map.end(); }

	TArray<KeyType> GetKeys() const
	{
		FScopeLock Lock(&CriticalSection);
		return Map.GetKeys();
	}

	template<typename Allocator> int32 GetKeys(TArray<KeyType, Allocator>& OutKeys) const
	{
		FScopeLock Lock(&CriticalSection);
		return Map.GetKeys(OutKeys);
	}

	MapType& GetData() { return Map; }
	
	FCriticalSection& GetCriticalSection() const { return CriticalSection; }
	
private:
	MapType Map;
	mutable FCriticalSection CriticalSection;
};