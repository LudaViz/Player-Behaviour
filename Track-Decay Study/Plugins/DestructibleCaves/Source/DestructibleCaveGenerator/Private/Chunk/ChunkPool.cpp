// Copyright 2025 J2K2. All Rights Reserved.

#include "Chunk/ChunkPool.h"
#include "Engine/World.h"
#include "Core/ChunkManager.h"

UChunkPool::UChunkPool()
    : CurrentPoolSize(0)
    , TargetPoolSize(0)
    , MaxPoolSize(0)
    , MinPoolSize(1)
    , StepPerTick(1)
    , ResizeState(EChunkPoolResizeState::None)
{
}

void UChunkPool::Initialize(int32 InitialSize, int32 InMaxSize,int32 InMinSize, int32 InStepSize, TSubclassOf<ATerrainChunk> InChunkClass)
{
    CleanupPool();

    ChunkClass = InChunkClass;
    MaxPoolSize = InMaxSize;
    //MinPoolSize = 32; // > 3x3x3
    MinPoolSize=FMath::RoundUpToPowerOfTwo(InMinSize);
    StepPerTick = InStepSize;

    TargetPoolSize = FMath::Clamp(FMath::RoundUpToPowerOfTwo(InitialSize), MinPoolSize, MaxPoolSize);
    for (int i = 0; i < TargetPoolSize; i++)
    {
        if (ATerrainChunk* NewChunk = CreateNewChunk())
        {
            ReturnedChunks.Add(NewChunk);
            ++CurrentPoolSize;
        }
    }
    ResizeState = EChunkPoolResizeState::None;
}

void UChunkPool::RequestResize(int32 NewSize)
{
    UE_LOG(LogDCG, Warning, TEXT("PoolResized %d->%d"),CurrentPoolSize,NewSize);
    TargetPoolSize = FMath::Clamp(FMath::RoundUpToPowerOfTwo(NewSize), MinPoolSize, MaxPoolSize);
    ResizeState = (TargetPoolSize > CurrentPoolSize) ? EChunkPoolResizeState::Expanding :
                  (TargetPoolSize < CurrentPoolSize) ? EChunkPoolResizeState::Shrinking :
                  EChunkPoolResizeState::None;
}

void UChunkPool::TickResize()
{
    if (ResizeState == EChunkPoolResizeState::None) return;

    const int32 Diff = TargetPoolSize - CurrentPoolSize;
    const int32 Step = FMath::Min(FMath::Abs(Diff), StepPerTick);

    for (int32 i = 0; i < Step; ++i)
    {
        if (Diff > 0)
        {
            if (ATerrainChunk* NewChunk = CreateNewChunk())
            {
                ReturnedChunks.Add(NewChunk);
                ++CurrentPoolSize;
            }
        }
        else if (Diff < 0)
        {
            if (ReturnedChunks.Num() > 0)
            {
                auto It = ReturnedChunks.CreateIterator();
                ATerrainChunk* ChunkToDestroy = *It;
                ReturnedChunks.Remove(ChunkToDestroy);

                // Safely destroy only if it exists
                if (IsValid(ChunkToDestroy))
                {
                    ChunkToDestroy->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
                    ChunkToDestroy->Destroy();
                }
                --CurrentPoolSize;
            }
        }
    }

    if (CurrentPoolSize == TargetPoolSize) ResizeState = EChunkPoolResizeState::None;
}

ATerrainChunk* UChunkPool::RentChunk()
{
    ATerrainChunk* Chunk = nullptr;

    while (ReturnedChunks.Num() > 0)
    {
        auto It = ReturnedChunks.CreateIterator();
        Chunk = *It;
        ReturnedChunks.Remove(Chunk);

        if (IsValid(Chunk))
        {
            break; // Found a valid chunk!
        }
        else
        {
            Chunk = nullptr; // It was a deleted ghost pointer
            CurrentPoolSize--;
        }
    }

    if (!Chunk)
    {
        int32 NewTargetSize = FMath::Clamp(FMath::RoundUpToPowerOfTwo(CurrentPoolSize * 2), MinPoolSize, MaxPoolSize);
        if (NewTargetSize > CurrentPoolSize)
        {
            RequestResize(NewTargetSize);
        }

        Chunk = CreateNewChunk();
        if (Chunk) ++CurrentPoolSize;
    }

    if (Chunk)
    {
        Chunk->ResetChunk();
        RentedChunks.Add(Chunk);
        Chunk->SetChunkState(ETerrainChunkState::Idle);
    }
    return Chunk;
}

void UChunkPool::ReturnChunk(ATerrainChunk* Chunk)
{
    TRACE_CPUPROFILER_EVENT_SCOPE_STR("UChunkPool::ReturnChunk")
    if (RentedChunks.Remove(Chunk) > 0)
    {
        Chunk->ResetChunk();
        if (!ReturnedChunks.Contains(Chunk))
            ReturnedChunks.Add(Chunk);
    }
    
    // Check if we should shrink
    
    if (ReturnedChunks.Num() > CurrentPoolSize / 2)
    {
        int32 ShrinkTarget = FMath::Clamp(CurrentPoolSize/2/**2 / 3*/, MinPoolSize, MaxPoolSize);
        RequestResize(ShrinkTarget);
    }
}

void UChunkPool::CleanupPool()
{
    for (ATerrainChunk* Chunk : ReturnedChunks)
    {
        if (IsValid(Chunk))
        {
            Chunk->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
            Chunk->Destroy();
        }
    }
    ReturnedChunks.Empty();

    for (ATerrainChunk* Chunk : RentedChunks)
    {
        if (IsValid(Chunk))
        {
            Chunk->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
            Chunk->Destroy();
        }
    }
    RentedChunks.Empty();

    CurrentPoolSize = 0;
    TargetPoolSize = 0;
    ResizeState = EChunkPoolResizeState::None;
}


ATerrainChunk* UChunkPool::CreateNewChunk() const
{
    UWorld* World = GetWorld();
    if (!World || !*ChunkClass) return nullptr;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.bHideFromSceneOutliner = true;

    // RF_DuplicateTransient prevents the chunks from duplicating and lagging out when pressing "Play"
    SpawnParams.ObjectFlags |= RF_Transient;

    return World->SpawnActor<ATerrainChunk>(ChunkClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
}